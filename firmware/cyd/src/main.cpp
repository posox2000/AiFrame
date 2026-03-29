#include <Arduino.h>

#include "config.h"
#include "db.h"
#include "gen.h"
#include "settings.h"
#include "tft.h"

void setup() {
    Serial.begin(115200);
    Serial.println();

    // RGB LED off (active LOW)
    pinMode(LED_R, OUTPUT); digitalWrite(LED_R, HIGH);
    pinMode(LED_G, OUTPUT); digitalWrite(LED_G, HIGH);
    pinMode(LED_B, OUTPUT); digitalWrite(LED_B, HIGH);

    WiFi.mode(WIFI_AP_STA);

    db_init();
    sett_init();
    tft_init();

    gen.setKey(db[kk::poll_key]);

    // ======= AP =======
    WiFi.softAP("AiFrame CYD");

    tft.print("AiFrame v");
    tft.println(F_VERSION);
    tft.println("AiFrame CYD AP");
    tft.print("IP: ");
    tft.println(WiFi.softAPIP());
    tft.println();

    // ======= STA =======
    bool wifi_ok = false;

    if (db[kk::wifi_ssid].length()) {
        WiFi.begin(db[kk::wifi_ssid], db[kk::wifi_pass]);
        wifi_ok = true;
        tft.print("Connecting");
        int tries = 20;
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            tft.print('.');
            if (!--tries) {
                wifi_ok = false;
                break;
            }
        }
        tft.println();
        tft.print("IP: ");
        tft.println(WiFi.localIP());
    } else {
        tft.println("STA not configured");
    }
    tft.println();

    if (!wifi_ok) return;

    tft.println("Ready!");
    Serial.println("Ready!");

    if (WiFi.status() == WL_CONNECTED) ota.checkUpdate();
}

void loop() {
    sett_tick();
    gen_tick();
}
