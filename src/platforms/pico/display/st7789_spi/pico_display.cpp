//
// Created by cpasjuste on 30/05/23.
//

#include <cstdio>

#include "platform.h"
#include "pico_display.h"
#include "st7789.h"

using namespace mb;

const struct st7789_config lcd_config = {
        .spi      = spi0,
        .gpio_din = PIN_DIN,
        .gpio_clk = PIN_CLK,
        .gpio_cs  = PIN_CS,
        .gpio_dc  = PIN_DC,
        .gpio_rst = PIN_RESET,
        .gpio_bl  = PIN_BL,
};

PicoDisplay::PicoDisplay() : Display() {
    printf("PicoDisplay(%ix%i)\r\n", m_size.x, m_size.y);
    st7789_init(&lcd_config, m_size.x, m_size.y);
    st7789_fill(0x0000);
}

void PicoDisplay::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if ((x < 0) || (y < 0) || x > m_size.x || y > m_size.y) return;

    int16_t t;
    switch (rotation) {
        case 1:
            t = x;
            x = (int16_t) (m_size.x - 1 - y);
            y = t;
            break;
        case 2:
            x = (int16_t) (m_size.x - 1 - x);
            y = (int16_t) (m_size.y - 1 - y);
            break;
        case 3:
            t = x;
            x = y;
            y = (int16_t) (m_size.y - 1 - t);
            break;
    }

    st7789_set_cursor(x, y);
    st7789_put(color);
}

void PicoDisplay::drawPixelLine(uint16_t x, uint16_t y, uint16_t width,
                                const uint16_t *pixels, const Format &format) {
    //st7789_set_cursor(x, y);
    st7789_write(pixels, width * 2);
}

void PicoDisplay::clear() {
    st7789_fill(0x0000);
    st7789_set_cursor(0, 0);
}

void PicoDisplay::flip() {
    st7789_set_cursor(0, 0);
}
