//
// Created by cpasjuste on 30/05/23.
//

#ifndef MICROBOY_PICO_DISPLAY_H
#define MICROBOY_PICO_DISPLAY_H

#define PIN_DIN 19      // SPI0 TX  (MOSI)
#define PIN_CLK 18      // SPI0 SCK
#define PIN_CS 17       // SPI0 CSn
#define PIN_DC 16       // SPI0 RX
#define PIN_RESET 21    // GPIO
#define PIN_BL 20       // GPIO

namespace mb {
    class PicoDisplay : public Display {
    public:
        PicoDisplay();

        void clear() override;

        void flip() override;

        void drawPixel(int16_t x, int16_t y, uint16_t color) override;

        void drawPixelLine(uint16_t x, uint16_t y, uint16_t width,
                           const uint16_t *pixels, const Format &format = RGB565) override;
    };
}

#endif //MICROBOY_PICO_DISPLAY_H
