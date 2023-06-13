//
// Created by cpasjuste on 05/06/23.
//

#include "platform.h"
#include "display.h"

using namespace mb;

Display::Display(const Utility::Vec2i &size) : Adafruit_GFX(size.x, size.y) {
    m_size = size;
    m_bpp = 2;
    m_pitch = m_size.x * m_bpp;

    // pixel line buffer for drawSurface
    m_line_buffer = (uint16_t *) malloc(m_pitch);
    memset(m_line_buffer, 0, m_pitch);
}

void __not_in_flash_func(Display::drawSurface)
(Surface *surface, const Utility::Vec2i &pos, const Utility::Vec2i &size) {
    if (!surface) return;

    if (size == surface->getSize()) {
        auto pixels = surface->getPixels();
        auto pitch = surface->getPitch();
        auto width = surface->getSize().x;
        for (uint8_t y = 0; y < size.y; y++) {
            setCursor(pos.x, pos.y + y);
            drawPixelLine((uint16_t *) (pixels + y * pitch), width);
        }
    } else {
        // nearest-neighbor scaling
        int x, y;
        auto pitch = surface->getPitch();
        auto bpp = surface->getBpp();
        auto pixels = surface->getPixels();
        auto srcSize = surface->getSize();
        int xRatio = (srcSize.x << 16) / size.x;
        int yRatio = (srcSize.y << 16) / size.y;
        // here we try to avoid slow "st7789_set_cursor" calls.
        // if line width is equal to display width, skip this call
        if (size.x == m_size.x) {
            setCursor(0, pos.y);
        }
        for (uint8_t i = 0; i < size.y; i++) {
            for (uint8_t j = 0; j < size.x; j++) {
                x = (j * xRatio) >> 16;
                y = (i * yRatio) >> 16;
                m_line_buffer[j + pos.x] = *(uint16_t *) (pixels + y * pitch + x * bpp);
            }
            // here we try to avoid slow "st7789_set_cursor" calls.
            // if line width is equal to display width, skip this call
            if (size.x != m_size.x) {
                setCursor(x, y);
            }
            drawPixelLine(m_line_buffer, size.x);
        }
    }
}

// very slow, obviously...
void __not_in_flash_func(Display::drawPixel)(int16_t x, int16_t y, uint16_t color) {
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

    setCursor(x, y);
    putCursor(color);
}

// faster
void __not_in_flash_func(Display::drawPixelLine)
(const uint16_t *pixels, uint16_t width, const Display::Format &format) {
    if (format == Format::RGB565) {
        for (int_fast16_t i = 0; i < width; i++) {
            putCursor(pixels[i]);
        }
    } else {
        for (int_fast16_t i = 0; i < width; i++) {
            uint_fast16_t p = pixels[i];
            uint_fast8_t red = (p >> 8) & 0xF;
            uint_fast8_t green = (p >> 4) & 0xF;
            uint_fast8_t blue = p & 0xF;
            red = (red << 1) | (red >> 3);
            green = (green << 2) | (green >> 2);
            blue = (blue << 1) | (blue >> 3);
            putCursor((red << 11) | (green << 5) | blue);
        }
    }
}

void __not_in_flash_func(Display::clear)() {
    setCursor(0, 0);
    for (int y = 0; y < m_size.x; ++y) {
        for (int x = 0; x < m_size.y; ++x) {
            putCursor(0x0000);
        }
    }
    setCursor(0, 0);
}
