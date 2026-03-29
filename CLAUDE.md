# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

AiFrame is an AI-powered digital photo frame that generates images from text prompts and displays them on a TFT screen. Settings are exposed via a web UI served from the device itself.

Three firmware variants exist:

| Variant | Directory | MCU | Display | AI Backend |
|---|---|---|---|---|
| tft4 | `firmware/tft4/` | ESP8266 D1 Mini | 4" ST7796S, 320×480 | Kandinsky (fusionbrain.ai) |
| tft1.8 | `firmware/tft1.8/` | ESP8266 D1 Mini | 1.8" ST7735, 160×128 | Kandinsky (fusionbrain.ai) |
| cyd | `firmware/cyd/` | ESP32 (CYD) | 2.8" ILI9341, 320×240 | pollinations.ai |

The `cyd` variant is the active development target (see [Planned: CYD Variant](#planned-cyd-variant) below).

## Build System

**PlatformIO** is the primary build tool. Open the target directory in PlatformIO IDE; dependencies install automatically from `platformio.ini`. The `.ino` files are empty placeholders — all logic is in `src/`.

Serial monitor baud rate: **115200**. Upload speed: 921600.
- ESP8266 variants: exception decoder via `esp8266_exception_decoder` monitor filter
- CYD variant: exception decoder via `esp32_exception_decoder` monitor filter

There is no automated test suite — verification is done on hardware (watch for `"Ready!"` on Serial and the IP address on the TFT display).

## Architecture

The main loop (`main.cpp`) calls two tick functions on every iteration:

| Function | Source | Responsibility |
|---|---|---|
| `sett_tick()` | `settings.h` | Web UI, WiFi, OTA updates |
| `gen_tick()` | `gen.h` | AI image generation |

**State flow:**
1. `db.h` — initialises LittleFS and loads persistent key-value settings via `GyverDB`.
2. `settings.h` — builds the web UI with `SettingsGyver` and handles callbacks that write back to `GyverDB`.
3. `tft.h` — initialises the display driver.
4. AI library (`Kandinsky/Kandinsky.h` or `Pollinations/Pollinations.h`) — fetches and streams a JPEG image, decoded in-place by the vendored `tjpgd/` tiny JPEG decoder and rendered via a callback.
5. `timer.h` — lightweight interval timer used by `gen.h` for auto-generation.

## Key Configuration

`config.h` in each variant's `src/` controls display resolution and `DISP_SCALE` (1, 2, 4, or 8).

`project.json` at the repo root is the OTA update manifest consumed by AutoOTA. Compiled binary goes to `bin/firmware.bin`.

## Libraries

Vendored in `libraries/` (ESP8266 variants) or fetched by PlatformIO (CYD). Core ones:
- **GyverDB** — LittleFS key-value store
- **Settings** — HTTP-based web settings UI
- **GSON** — JSON parser
- **GyverHTTP** — HTTP client
- **AutoOTA** — OTA update check/apply
- **Adafruit GFX / ST7796S / ST7735** — display drivers (ESP8266 variants)
- **TFT_eSPI** — display driver (CYD variant)

---

## Planned: CYD Variant

### Target hardware
CYD ("Cheap Yellow Display") — ESP32 dev board with built-in 2.8" ILI9341 TFT (320×240), resistive touch (XPT2046), RGB LED (active LOW), backlight on GPIO 21.

**CYD SPI pins:** TFT_CS=15, TFT_DC=2, TFT_RST=-1, CLK=14, MOSI=13, MISO=12, TOUCH_CS=33, BLK=21, LED R/G/B = 4/16/17.

### AI backend change: pollinations.ai
Replaces the multi-step Kandinsky async state machine with a single blocking HTTPS GET:
```
GET https://gen.pollinations.ai/image/{encoded_prompt}?model=flux&key={api_key}
```
Returns raw JPEG bytes directly — no polling, no base64 decode. The `generate()` call blocks for ~10–20s while the image streams in. Width/height query params (`&width=320&height=240`) can be appended as needed.

Authentication uses a `key=` query parameter (bearer-style API key, e.g. `sk_...`). The key is stored in `db.h` and configured via the settings web UI.

### New/changed files in `firmware/cyd/src/`

| File | Change |
|---|---|
| `platformio.ini` | `esp32dev` board, TFT_eSPI via build_flags (`USER_SETUP_LOADED=1`, ILI9341 pin defines), remove Adafruit libs |
| `config.h` | `DISP_HEIGHT=240`, `DISP_SCALE=1`, add `TFT_BL=21`, `LED_R/G/B` pins |
| `db.h` | Remove `kand_token`, `kand_secret`, `gen_style`; add `poll_key` for the pollinations API key |
| `tft.h` | `TFT_eSPI` driver, `tft.pushImage()`, drive backlight GPIO, `setRotation(1)` |
| `settings.h` | Replace Kandinsky credentials with a single `poll_key` input; remove style selector; `ESP.restart()` (not `reset()`) |
| `gen.h` | Include `Pollinations/Pollinations.h`; remove `gen.tick()`; simplified `generate()` call |
| `main.cpp` | Remove Kandinsky init calls; init RGB LED pins; rename AP to `"AiFrame CYD"` |
| `Pollinations/Pollinations.h` | **New** — replaces Kandinsky library (see below) |
| `Pollinations/tjpgd/` | Copied verbatim from `Kandinsky/tjpgd/` |

Files **not** copied from tft4: `Kandinsky.h`, `StreamB64.h`, `web/`.

### `Pollinations.h` design
- `setKey(key)` — stores the API key, appended as `key=` query param
- `generate(query, width, height, negative)` — blocking HTTPS GET to `gen.pollinations.ai` via `WiFiClientSecure` + `GyverHTTP`; appends `model=flux&key={key}`
- Streams response body directly into `jd_input_cb` → tjpgd → `onRender(x, y, w, h, buf)` callback
- URL-encodes prompt; appends `seed=millis()` for variety
- Calls `esp_task_wdt_reset()` inside `jd_input_cb` to prevent watchdog reset during the long blocking call
- No `tick()`, no async state, no base64, no style enum

### ESP32 watchdog note
The ESP32 hardware watchdog (~5–8s) will fire during blocking HTTPS. Pet it with `esp_task_wdt_reset()` (from `esp_task_wdt.h`) on every `jd_input_cb` invocation.
