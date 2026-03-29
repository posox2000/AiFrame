#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// #define GHTTP_HEADERS_LOG Serial
#include <GyverHTTP.h>

#include "tjpgd/tjpgd.h"

#define POLL_HOST "gen.pollinations.ai"
#define POLL_PORT 443
#define POLL_LOG(x) Serial.println(x)

class Pollinations {
    typedef std::function<void(int x, int y, int w, int h, uint8_t* buf)> RenderCallback;
    typedef std::function<void()> RenderEndCallback;

   public:
    Pollinations() {}

    void setKey(const String& key) {
        _api_key = key;
    }

    void onRender(RenderCallback cb) {
        _rnd_cb = cb;
    }

    void onRenderEnd(RenderEndCallback cb) {
        _end_cb = cb;
    }

    // scale: 1, 2, 4, 8 — maps to tjpgd 0, 1, 2, 3
    void setScale(uint8_t scale) {
        switch (scale) {
            case 2: _scale = 1; break;
            case 4: _scale = 2; break;
            case 8: _scale = 3; break;
            default: _scale = 0; break;
        }
    }

    bool generate(const String& query, uint16_t width = 320, uint16_t height = 240, const String& negative = "") {
        if (!query.length()) { status = "empty query"; return false; }
        status = "generating";

        String path = "/image/";
        path += urlEncode(query);
        path += "?model=flux";
        path += "&width=";
        path += width;
        path += "&height=";
        path += height;
        if (negative.length()) {
            path += "&negative=";
            path += urlEncode(negative);
        }
        if (_api_key.length()) {
            path += "&key=";
            path += _api_key;
        }
        path += "&seed=";
        path += (uint32_t)millis();
        path += "&nologo=true";

        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(30);  // seconds

        ghttp::Client http(client, POLL_HOST, POLL_PORT);

        ghttp::Client::Headers headers;
        headers.add("User-Agent", "AiFrame/1.0 ESP32");
        headers.add("Accept", "image/jpeg");
        headers.add("Connection", "close");

        if (!http.request(path, "GET", headers)) {
            status = "request error";
            return false;
        }

        ghttp::Client::Response resp = http.getResponse();
        if (!resp || resp.code() < 200 || resp.code() >= 300) {
            http.flush();
            status = "http error";
            return false;
        }

        bool ok = decodeJpeg(resp.body());
        http.flush();
        status = ok ? "done" : "jpeg error";
        return ok;
    }

    String status = "idle";

   private:
    String _api_key;
    uint8_t _scale = 0;
    RenderCallback _rnd_cb = nullptr;
    RenderEndCallback _end_cb = nullptr;
    Stream* _jpegStream = nullptr;

    static Pollinations* self;

    static size_t jd_input_cb(JDEC* jdec, uint8_t* buf, size_t len) {
        yield();  // pet the watchdog
        if (!self || !self->_jpegStream) return 0;
        if (buf) {
            return self->_jpegStream->readBytes(buf, len);
        } else {
            // skip len bytes
            for (size_t i = 0; i < len; i++) self->_jpegStream->read();
            return len;
        }
    }

    static int jd_output_cb(JDEC* jdec, void* bitmap, JRECT* rect) {
        if (self && self->_rnd_cb) {
            self->_rnd_cb(rect->left, rect->top,
                          rect->right - rect->left + 1,
                          rect->bottom - rect->top + 1,
                          (uint8_t*)bitmap);
        }
        return 1;
    }

    bool decodeJpeg(Stream& stream) {
        uint8_t* workspace = new uint8_t[TJPGD_WORKSPACE_SIZE];
        if (!workspace) { POLL_LOG("alloc error"); return false; }

        JDEC jdec;
        jdec.swap = 1;
        _jpegStream = &stream;
        self = this;

        JRESULT res = jd_prepare(&jdec, jd_input_cb, workspace, TJPGD_WORKSPACE_SIZE, nullptr);
        if (res == JDR_OK) {
            res = jd_decomp(&jdec, jd_output_cb, _scale);
            if (res == JDR_OK && _end_cb) _end_cb();
        }

        self = nullptr;
        _jpegStream = nullptr;
        delete[] workspace;

        if (res != JDR_OK) POLL_LOG("tjpgd error");
        return res == JDR_OK;
    }

    static String urlEncode(const String& s) {
        String out;
        out.reserve(s.length() * 3);
        for (size_t i = 0; i < s.length(); i++) {
            char c = s[i];
            if (isAlphaNumeric(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                out += c;
            } else {
                char hex[4];
                snprintf(hex, sizeof(hex), "%%%02X", (uint8_t)c);
                out += hex;
            }
        }
        return out;
    }
};

Pollinations* Pollinations::self __attribute__((weak)) = nullptr;
