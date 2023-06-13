//
// Created by cpasjuste on 30/05/23.
//

#ifndef MICROBOY_PICO_DISPLAY_H
#define MICROBOY_PICO_DISPLAY_H

namespace mb {
    class PicoDisplay : public Display {
    public:
        PicoDisplay();

        void setCursor(uint16_t x, uint16_t y) override;

        void putCursor(uint16_t color) override;
    };
}

#endif //MICROBOY_PICO_DISPLAY_H
