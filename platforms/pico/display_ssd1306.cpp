//
// Created by cpasjuste on 30/05/23.
//

#include <cstdio>
#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include "display.h"
#include "display_ssd1306.h"

#define SSD1306_SDA 0
#define SSD1306_SCL 1

using namespace mb;

SSD1306Display::SSD1306Display() : Display() {
    // init SSD1306 oled panel
    i2c_init(i2c0, 400000);
    gpio_set_function(SSD1306_SDA, GPIO_FUNC_I2C);
    gpio_set_function(SSD1306_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(SSD1306_SDA);
    gpio_pull_up(SSD1306_SCL);

    ssd1306_init(&m_display, 128, 64, 0x3C, i2c0);
}

void SSD1306Display::clear() {
    ssd1306_clear(&m_display);
}

void SSD1306Display::flip() {
    ssd1306_show(&m_display);
}

void SSD1306Display::drawLine(uint8_t y, uint16_t *line) {
    if (y > m_display.height) return;

    for (int x = 0; x < m_display.width; x++) {
        if (line[x] > 0) ssd1306_draw_pixel(&m_display, x, y);
        else ssd1306_clear_pixel(&m_display, x, y);
    }

    if (y == m_display.height) {
        flip();
    }
}
