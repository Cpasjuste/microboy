//
// Created by cpasjuste on 31/05/23.
//

#include "platform.h"
#include "pinout.h"

using namespace mb;

struct Mapping {
    uint8_t button;
    int8_t pin;
};

const Mapping mapping[MAX_BUTTONS] = {
        {Input::Button::B1,     BTN_PIN_B1},
        {Input::Button::B2,     BTN_PIN_B2},
        {Input::Button::START,  BTN_PIN_START},
        {Input::Button::SELECT, BTN_PIN_SELECT},
        {Input::Button::LEFT,   BTN_PIN_LEFT},
        {Input::Button::RIGHT,  BTN_PIN_RIGHT},
        {Input::Button::UP,     BTN_PIN_UP},
        {Input::Button::DOWN,   BTN_PIN_DOWN}
};

PicoInput::PicoInput() : Input() {
    for (const auto &map: mapping) {
        if (map.pin != -1) {
            gpio_set_function(map.pin, GPIO_FUNC_SIO);
            gpio_set_dir(map.pin, false);
            gpio_pull_up(map.pin);
        }
    }
}

uint16_t PicoInput::getButtons() {
    // reset buttons state
    m_buttons = 0;

    // check for buttons (keyboard) press
    for (const auto &map: mapping) {
        if (map.pin != -1) m_buttons |= gpio_get(map.pin) ? 0 : map.button;
    }

#ifndef NDEBUG
    int c = getchar_timeout_us(0);
    switch (c) {
        case 49: // NUMPAD 1
            m_buttons |= B1;
            break;
        case 50: // NUMPAD 2
            m_buttons |= B2;
            break;
        case 52: // NUMPAD 4
            m_buttons |= SELECT;
            break;
        case 53: // NUMPAD 5
            m_buttons |= START;
            break;
        case 65: // ARROW UP
            m_buttons |= UP;
            break;
        case 66: // ARROW DOWN
            m_buttons |= DOWN;
            break;
        case 67: // ARROW RIGHT
            m_buttons |= RIGHT;
            break;
        case 68: // ARROW LEFT
            m_buttons |= LEFT;
            break;
        case 'q':
            m_buttons |= QUIT;
            break;
        default:
            break;
    }
#endif

    // handle repeat delay
    return Input::getButtons();
}
