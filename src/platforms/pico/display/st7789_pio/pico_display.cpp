//
// Created by cpasjuste on 30/05/23.
//

#include <cstdio>

#include "platform.h"
#include "pico_display.h"
#include "st7789.h"

using namespace mb;

PicoDisplay::PicoDisplay() : Display({DISPLAY_WIDTH, DISPLAY_HEIGHT}) {
    printf("PicoDisplay(): %ix%i\r\n", m_size.x, m_size.y);

    // init st7789 display
    st7789_init();

    // clear the display
    PicoDisplay::clear();
}

void PicoDisplay::setCursor(uint16_t x, uint16_t y) {
    st7789_set_cursor(x, y);
}

void PicoDisplay::putCursor(uint16_t color) {
    st7789_put(color);
}
