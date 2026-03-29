#pragma once
#include <TFT_eSPI.h>

#include "config.h"
#include "gen.h"

// CYD SPI pins configured via platformio.ini build_flags:
// TFT_CS=15, TFT_DC=2, TFT_RST=-1, TFT_MOSI=13, TFT_SCLK=14, TFT_MISO=12
// Backlight: GPIO 21 (active HIGH)

TFT_eSPI tft;

void tft_render(int x, int y, int w, int h, uint8_t* buf) {
    tft.pushImage(x, y, w, h, (uint16_t*)buf);
}

void tft_init() {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);

    gen.onRender(tft_render);
}
