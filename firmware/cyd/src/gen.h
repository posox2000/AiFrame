#pragma once
#include "Pollinations/Pollinations.h"

#include "config.h"
#include "db.h"

Pollinations gen;
bool gen_flag = 0;
bool gen_image_ready = false;

void generate() {
    gen_flag = 1;
}

void gen_tick() {
    if (gen_flag) {
        gen_flag = 0;
        gen.setScale(DISP_SCALE);
        bool ok = gen.generate(
            db[kk::gen_query],
            DISP_WIDTH,
            DISP_HEIGHT,
            db[kk::gen_negative]);
        if (ok) gen_image_ready = true;
    }
}
