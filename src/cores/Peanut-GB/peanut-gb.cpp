//
// Created by cpasjuste on 06/06/23.
//

#include "platform.h"

#include "peanut-gb.h"
#include "gbcolors.h"
#include "hedley.h"
extern "C" {
#define ENABLE_SOUND 1
#include "minigb_apu.h"
#include "peanut_gb.h"
};

using namespace mb;

//#define ENABLE_RAM_BANK
#ifdef ENABLE_RAM_BANK
static unsigned char rom_bank0[65536];
#endif
static const uint8_t *gb_rom = nullptr;
static uint8_t gb_ram[32768];
static int lcd_line_busy = 0;

static palette_t palette;
static uint8_t manual_palette_selected = 0;

#define AUDIO_BUFFER_SIZE (AUDIO_SAMPLES * 4)
static uint16_t audio_stream[AUDIO_BUFFER_SIZE];

/* Multicore command structure. */
union core_cmd {
    struct {
#define CORE_CMD_NOP        0
#define CORE_CMD_LCD_LINE   1
#define CORE_CMD_LCD_FLIP   2
        uint8_t cmd;
        uint8_t data;
    };
    uint32_t full;
};

struct gb_priv {
    PeanutGB *gb;
};
static struct gb_priv gb_priv{};

static struct gb_s gameboy;

uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr) {
    (void) gb;
#ifdef ENABLE_RAM_BANK
    if (addr < sizeof(rom_bank0)) {
        return rom_bank0[addr];
    }
#endif

    return gb_rom[addr];
}

uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr) {
    (void) gb;
    return gb_ram[addr];
}

void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val) {
    (void) gb;
    gb_ram[addr] = val;
}

void gb_error(struct gb_s *gb, const enum gb_error_e gb_err, const uint16_t val) {
    (void) gb;
    (void) val;
#if 1
    const char *gb_err_str[GB_INVALID_MAX] = {
            "UNKNOWN",
            "INVALID OPCODE",
            "INVALID READ",
            "INVALID WRITE",
            "HALT"
    };
    printf("Error %d occurred: %s (abort)\r\n",
           gb_err,
           gb_err >= GB_INVALID_MAX ? gb_err_str[0] : gb_err_str[gb_err]);
    abort();
#endif
}

static bool m_scale = true;
static uint16_t pixels_buffer[LCD_WIDTH];
static Utility::Vec2i drawingPos{};

void core1_lcd_draw_line(const uint_fast8_t line) {
    auto display = gb_priv.gb->getPlatform()->getDisplay();
    display->setCursor(drawingPos.x, drawingPos.y + line);
    display->drawPixelLine(pixels_buffer, LCD_WIDTH);

    if (line == LCD_HEIGHT - 1) {
        display->flip();
    }

    __atomic_store_n(&lcd_line_busy, 0, __ATOMIC_SEQ_CST);
}

void core1_lcd_flip(const uint_fast8_t idx) {
    //printf("core1_lcd_flip(%i)\r\n", idx);
    auto display = gb_priv.gb->getPlatform()->getDisplay();
    if (m_scale) {
        auto surfaceSize = gb_priv.gb->getSurface(idx)->getSize();
        auto displaySize = display->getSize();
        auto dstSize = Utility::Vec2i(
                displaySize.x, (int16_t) ((float) displaySize.x * ((float) surfaceSize.y / (float) surfaceSize.x)));
        auto dstPos = Utility::Vec2i(0, (int16_t) ((displaySize.y - dstSize.y) / 2));
        display->drawSurface(gb_priv.gb->getSurface(idx), dstPos, dstSize);
    } else {
        display->drawSurface(gb_priv.gb->getSurface(idx), drawingPos);
    }

    display->flip();

    //__atomic_store_n(&lcd_line_busy, 0, __ATOMIC_SEQ_CST);
}

_Noreturn void main_core1() {
    union core_cmd cmd{};

    // handle commands coming from core0
    while (true) {
        cmd.full = multicore_fifo_pop_blocking();
        switch (cmd.cmd) {
            case CORE_CMD_LCD_LINE:
                core1_lcd_draw_line(cmd.data);
                break;
            case CORE_CMD_LCD_FLIP:
                core1_lcd_flip(cmd.data);
                break;
            case CORE_CMD_NOP:
            default:
                break;
        }
    }

    HEDLEY_UNREACHABLE();
}

void lcd_draw_line(struct gb_s *gb, const uint8_t pixels[LCD_WIDTH], const uint_fast8_t line) {
    if (gb_priv.gb->isBuffered()) {
        uint8_t bufferIndex = gb_priv.gb->getBufferIndex();

        for (uint_fast8_t x = 0; x < LCD_WIDTH; x++) {
            gb_priv.gb->getSurface(bufferIndex)->setPixel(
                    x, line, palette[(pixels[x] & LCD_PALETTE_ALL) >> 4][pixels[x] & 3]);
        }

        if (line == LCD_HEIGHT - 1) {
            // wait until previous surface flip complete
            //while (__atomic_load_n(&lcd_line_busy, __ATOMIC_SEQ_CST))
            //  tight_loop_contents();

            union core_cmd cmd{};
            cmd.cmd = CORE_CMD_LCD_FLIP;
            cmd.data = bufferIndex;
            // flip buffers
            gb_priv.gb->setBufferIndex(bufferIndex == 0 ? 1 : 0);
#if LINUX
            core1_lcd_flip(cmd.data);
#else
            // send cmd
            //__atomic_store_n(&lcd_line_busy, 1, __ATOMIC_SEQ_CST);
            multicore_fifo_push_blocking(cmd.full);
#endif
        }
    } else {
        // wait until previous line is sent
        while (__atomic_load_n(&lcd_line_busy, __ATOMIC_SEQ_CST)) tight_loop_contents();

        for (uint_fast8_t x = 0; x < LCD_WIDTH; x++) {
            pixels_buffer[x] = palette[(pixels[x] & LCD_PALETTE_ALL) >> 4][pixels[x] & 3];
        }

        union core_cmd cmd{};
        cmd.cmd = CORE_CMD_LCD_LINE;
        cmd.data = line;
        // send cmd
#if LINUX
        core1_lcd_draw_line(cmd.data);
#else
        __atomic_store_n(&lcd_line_busy, 1, __ATOMIC_SEQ_CST);
        multicore_fifo_push_blocking(cmd.full);
#endif
    }
}

PeanutGB::PeanutGB(Platform *p) : Core(p) {
    // create some render surfaces (double buffering)
    if (m_doubleBuffer) {
        p_surface[0] = new Surface({LCD_WIDTH, LCD_HEIGHT});
        p_surface[1] = new Surface({LCD_WIDTH, LCD_HEIGHT});
    }

    // cache drawing position
    drawingPos = {
            (int16_t) ((p_platform->getDisplay()->getSize().x - LCD_WIDTH) / 2),
            (int16_t) ((p_platform->getDisplay()->getSize().y - LCD_HEIGHT) / 2)
    };

    // init audio
    p_platform->getAudio()->setup(AUDIO_SAMPLE_RATE, AUDIO_SAMPLES);
    audio_init();

    gb_priv.gb = this;
}

bool PeanutGB::loadRom(const std::string &path) {
    size_t size;
    uint8_t *rom = p_platform->getIo()->load("/roms/rom.gb", &size);
    if (!rom) {
        printf("PeanutGB::loadRom: failed to load rom (%s)\r\n", path.c_str());
        return false;
    }

    return loadRom(rom, size);
}

bool PeanutGB::loadRom(const uint8_t *buffer, size_t size) {
    enum gb_init_error_e ret;

    gb_rom = buffer;
#ifdef ENABLE_RAM_BANK
    memcpy(rom_bank0, buffer, sizeof(rom_bank0));
#endif

    // start Core1, which processes requests to the LCD
    multicore_launch_core1(main_core1);

    // initialise GB context
    ret = gb_init(&gameboy, &gb_rom_read, &gb_cart_ram_read, &gb_cart_ram_write, &gb_error, &gb_priv);

    if (ret != GB_INIT_NO_ERROR) {
        printf("PeanutGB::loadRom: error %d\r\n", ret);
        return false;
    }

    // automatically assign a colour palette to the game
    char rom_title[16];
    auto_assign_palette(palette, gb_colour_hash(&gameboy), gb_get_rom_name(&gameboy, rom_title));

    gb_init_lcd(&gameboy, &lcd_draw_line);

    // not enough fps when scaling is used (with double buffering, st7789), enable interlacing
    //if (m_doubleBuffer) gameboy.direct.interlace = 1;

    return true;
}

bool PeanutGB::loop() {
    gameboy.gb_frame = 0;

    do {
        __gb_step_cpu(&gameboy);
        tight_loop_contents();
    } while (HEDLEY_LIKELY(gameboy.gb_frame == 0));

    // send audio buffer to playback device
    audio_callback(nullptr, reinterpret_cast<uint8_t *>(audio_stream), AUDIO_BUFFER_SIZE);
    gb_priv.gb->getPlatform()->getAudio()->play(audio_stream, AUDIO_SAMPLES);

    /* Required since we do not know whether a button remains
     * pressed over a serial connection. */
    //if (frames % 4 == 0) gameboy.direct.joypad = 0xFF;
    gameboy.direct.joypad = 0xFF;

#ifndef LINUX
    int input = getchar_timeout_us(0);
    switch (input) {
        case 's':
            gameboy.direct.joypad_bits.start = 0;
            break;
        default:
            break;
    }
#endif

    // handle input
    uint16_t buttons = p_platform->getInput()->getButtons();
    if (buttons > 0 && !(buttons & mb::Input::Button::DELAY)) {
        // exit requested (linux)
        if (buttons & mb::Input::Button::QUIT) return false;

        // emulation inputs
        gameboy.direct.joypad_bits.a = !(buttons & mb::Input::Button::B1);
        gameboy.direct.joypad_bits.b = !(buttons & mb::Input::Button::B2);
        gameboy.direct.joypad_bits.select = !(buttons & mb::Input::Button::SELECT);
        gameboy.direct.joypad_bits.start = !(buttons & mb::Input::Button::START);
        gameboy.direct.joypad_bits.up = !(buttons & mb::Input::Button::UP);
        gameboy.direct.joypad_bits.right = !(buttons & mb::Input::Button::RIGHT);
        gameboy.direct.joypad_bits.down = !(buttons & mb::Input::Button::DOWN);
        gameboy.direct.joypad_bits.left = !(buttons & mb::Input::Button::LEFT);

        // hotkey / combos
        if (buttons & mb::Input::Button::SELECT) {
            p_platform->getInput()->setRepeatDelay(INPUT_DELAY_UI);
            // palette selection
            if (buttons & mb::Input::Button::LEFT) {
                if (manual_palette_selected > 0) {
                    manual_palette_selected--;
                    manual_assign_palette(palette, manual_palette_selected);
                }
            } else if (buttons & mb::Input::Button::RIGHT) {
                if (manual_palette_selected < NUMBER_OF_MANUAL_PALETTES) {
                    manual_palette_selected++;
                    manual_assign_palette(palette, manual_palette_selected);
                }
            }
        } else {
            p_platform->getInput()->setRepeatDelay(0);
        }
    }

    return true;
}

PeanutGB::~PeanutGB() {
    if (m_doubleBuffer) {
        delete (p_surface[0]);
        delete (p_surface[1]);
    }
}
