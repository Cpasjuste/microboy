//
// Created by cpasjuste on 06/06/23.
//

#include <cstdarg>
#include "platform.h"
#include "infones.h"
#include "InfoNES.h"
#include "InfoNES_System.h"
#include "K6502.h"
#include "cores/Peanut-GB/hedley.h"

using namespace mb;

#define CC(x) (((x >> 1) & 15) | (((x >> 6) & 15) << 4) | (((x >> 11) & 15) << 8))
const WORD __not_in_flash_func(NesPalette)[64] = {
        CC(0x39ce), CC(0x1071), CC(0x0015), CC(0x2013), CC(0x440e), CC(0x5402), CC(0x5000), CC(0x3c20),
        CC(0x20a0), CC(0x0100), CC(0x0140), CC(0x00e2), CC(0x0ceb), CC(0x0000), CC(0x0000), CC(0x0000),
        CC(0x5ef7), CC(0x01dd), CC(0x10fd), CC(0x401e), CC(0x5c17), CC(0x700b), CC(0x6ca0), CC(0x6521),
        CC(0x45c0), CC(0x0240), CC(0x02a0), CC(0x0247), CC(0x0211), CC(0x0000), CC(0x0000), CC(0x0000),
        CC(0x7fff), CC(0x1eff), CC(0x2e5f), CC(0x223f), CC(0x79ff), CC(0x7dd6), CC(0x7dcc), CC(0x7e67),
        CC(0x7ae7), CC(0x4342), CC(0x2769), CC(0x2ff3), CC(0x03bb), CC(0x0000), CC(0x0000), CC(0x0000),
        CC(0x7fff), CC(0x579f), CC(0x635f), CC(0x6b3f), CC(0x7f1f), CC(0x7f1b), CC(0x7ef6), CC(0x7f75),
        CC(0x7f94), CC(0x73f4), CC(0x57d7), CC(0x5bf9), CC(0x4ffe), CC(0x0000), CC(0x0000), CC(0x0000)};

static Platform *platform;
static bool quit = false;
static bool frameLoaded = false;
#define LINE_BUFFER_COUNT 16
static WORD lineBufferRGB444[LINE_BUFFER_COUNT][NES_DISP_WIDTH];
static uint8_t lineBufferIndex = 0;
extern int SpriteJustHit;
static int lcd_line_busy = 0;

_Noreturn [[noreturn]] void __not_in_flash_func(core1_main)();

union core_cmd {
    struct {
#define CORE_CMD_NOP        0
#define CORE_CMD_LCD_LINE   1
        uint8_t cmd;
        uint8_t line;
        uint8_t index;
    };
    uint32_t full;
};

InfoNES::InfoNES(Platform *p) : Core(p) {
    // crappy
    platform = p;

    // start Core1, which processes requests to the LCD
    multicore_launch_core1(core1_main);
}

bool InfoNES::loadRom(const std::string &path) {
    size_t size;
    uint8_t *rom = p_platform->getIo()->load("/roms/rom.nes", &size);
    if (!rom) {
        printf("InfoNES::loadRom: failed to load rom (%s)\r\n", path.c_str());
        return false;
    }

    return loadRom(rom, size);
}

bool InfoNES::loadRom(const uint8_t *buffer, size_t size) {
    memcpy(&NesHeader, buffer, sizeof(NesHeader));
    if (memcmp(NesHeader.byID, "NES\x1a", 4) != 0) {
        return false;
    }

    buffer += sizeof(NesHeader);
    memset(SRAM, 0, SRAM_SIZE);
    if (NesHeader.byInfo1 & 4) {
        memcpy(&SRAM[0x1000], buffer, 512);
        buffer += 512;
    }

    auto romSize = NesHeader.byRomSize * 0x4000;
    ROM = (BYTE *) buffer;
    buffer += romSize;

    if (NesHeader.byVRomSize > 0) {
        auto vSize = NesHeader.byVRomSize * 0x2000;
        VROM = (BYTE *) buffer;
        buffer += vSize;
    }

    if (InfoNES_Reset() < 0) {
        printf("InfoNES::loadRom: NES reset error\n");
        return false;
    }

    InfoNES_Init();

    return true;
}

bool InfoNES::loop() {
    if (InfoNES_Menu() == -1) return false; // quit

    while (!frameLoaded) {
        // Set a flag if a scanning line is a hit in the sprite #0
        if (SpriteJustHit == PPU_Scanline && PPU_ScanTable[PPU_Scanline] == SCAN_ON_SCREEN) {
            // # of Steps to execute before sprite #0 hit
            int nStep = SPRRAM[SPR_X] * STEP_PER_SCANLINE / NES_DISP_WIDTH;

            // Execute instructions
            K6502_Step(nStep);

            // Set a sprite hit flag
            if ((PPU_R1 & R1_SHOW_SP) && (PPU_R1 & R1_SHOW_SCR))
                PPU_R2 |= R2_HIT_SP;

            // NMI is required if there is necessity
            if ((PPU_R0 & R0_NMI_SP) && (PPU_R1 & R1_SHOW_SP))
                NMI_REQ;

            // Execute instructions
            K6502_Step(STEP_PER_SCANLINE - nStep);
        } else {
            // Execute instructions
            K6502_Step(STEP_PER_SCANLINE);
        }

        // Frame IRQ in H-Sync
        FrameStep += STEP_PER_SCANLINE;
        if (FrameStep > STEP_PER_FRAME && FrameIRQ_Enable) {
            FrameStep %= STEP_PER_FRAME;
            IRQ_REQ;
            APU_Reg[0x4015] |= 0x40;
        }

        // A mapper function in H-Sync
        MapperHSync();

        // A function in H-Sync
        if (InfoNES_HSync() == -1) return false;

        // HSYNC Wait
        //InfoNES_Wait();
    }

    frameLoaded = false;

    return true;
}

InfoNES::~InfoNES() = default;

void core1_lcd_draw_line(const uint_fast8_t line, const uint_fast8_t index) {
    // crop pixel buffer, assume a 240x240 display size for now...
    auto display = platform->getDisplay();
    display->setCursor(0, line);
    display->drawPixelLine(lineBufferRGB444[index] + 8, 240, Display::Format::RGB444);

    // signal we are done
    //__atomic_store_n(&lcd_line_busy, 0, __ATOMIC_SEQ_CST);
}

_Noreturn void __not_in_flash_func(core1_main)() {
    union core_cmd cmd{};

    while (true) {
        cmd.full = multicore_fifo_pop_blocking();
        switch (cmd.cmd) {
            case CORE_CMD_LCD_LINE:
                core1_lcd_draw_line(cmd.line, cmd.index);
                break;
            case CORE_CMD_NOP:
            default:
                break;
        }
    }

    HEDLEY_UNREACHABLE();
}

void __not_in_flash_func(InfoNES_PreDrawLine)(int line) {
    //printf("InfoNES_PreDrawLine(%i)\r\n", line);

    // wait until previous line is sent
    //while (__atomic_load_n(&lcd_line_busy, __ATOMIC_SEQ_CST))
    //    tight_loop_contents();

    InfoNES_SetLineBuffer(lineBufferRGB444[lineBufferIndex], NES_DISP_WIDTH);
}

void __not_in_flash_func(InfoNES_PostDrawLine)(int line) {
    //printf("InfoNES_PostDrawLine(%i)\r\n", line);
#ifdef LINUX
    core1_lcd_draw_line(line, lineBufferIndex);
#else
    // send cmd
    core_cmd cmd{{CORE_CMD_LCD_LINE, (uint8_t) line, lineBufferIndex}};

    //__atomic_store_n(&lcd_line_busy, 1, __ATOMIC_SEQ_CST);
    multicore_fifo_push_blocking(cmd.full);

    // swap line buffer
    lineBufferIndex++;
    if (lineBufferIndex == LINE_BUFFER_COUNT) {
        lineBufferIndex = 0;
    }
#endif
}

void InfoNES_LoadFrame() {
    //printf("InfoNES_LoadFrame\r\n");
    platform->getDisplay()->flip();
    frameLoaded = true;
}

int InfoNES_ReadRom(const char *pszFileName) {
    printf("InfoNES_ReadRom(%s)\r\n", pszFileName);
    return 0;
}

void InfoNES_ReleaseRom() {
    printf("InfoNES_ReleaseRom\r\n");
    ROM = nullptr;
    VROM = nullptr;
}

void InfoNES_PadState(DWORD *pdwPad1, DWORD *pdwPad2, DWORD *pdwSystem) {
    static constexpr int LEFT = 1 << 6;
    static constexpr int RIGHT = 1 << 7;
    static constexpr int UP = 1 << 4;
    static constexpr int DOWN = 1 << 5;
    static constexpr int SELECT = 1 << 2;
    static constexpr int START = 1 << 3;
    static constexpr int A = 1 << 0;
    static constexpr int B = 1 << 1;

    uint16_t buttons = platform->getInput()->getButtons();

    // exit requested (linux)
    if (buttons & mb::Input::Button::QUIT) exit(0); // TODO: clean exit..

    // emulation inputs
    auto &pad = *pdwPad1;
    pad = (buttons & mb::Input::Button::LEFT ? LEFT : 0) |
          (buttons & mb::Input::Button::RIGHT ? RIGHT : 0) |
          (buttons & mb::Input::Button::UP ? UP : 0) |
          (buttons & mb::Input::Button::DOWN ? DOWN : 0) |
          (buttons & mb::Input::Button::B1 ? A : 0) |
          (buttons & mb::Input::Button::B2 ? B : 0) |
          (buttons & mb::Input::Button::SELECT ? SELECT : 0) |
          (buttons & mb::Input::Button::START ? START : 0) |
          0;

#ifndef LINUX
    pad = 0;
    int input = getchar_timeout_us(0);
    switch (input) {
        case 's':
            pad = START;
            break;
        default:
            break;
    }
#endif
}

int InfoNES_Menu() {
    //printf("InfoNES_Menu\r\n");
    return quit ? -1 : 0;
}

void InfoNES_SoundInit() {
    printf("InfoNES_SoundInit\r\n");
    platform->getAudio()->setup(44100, 735, 1);
}

int InfoNES_SoundOpen(int samples_per_sync, int sample_rate) {
    printf("InfoNES_SoundOpen(%i, %i)\r\n", samples_per_sync, sample_rate);
    return 1;
}

void InfoNES_SoundClose() {
}

int __not_in_flash_func(InfoNES_GetSoundBufferSize)() {
    //printf("InfoNES_GetSoundBufferSize\r\n");
    return 735 * sizeof(uint8_t);
}

static uint16_t audio_buffer[1024];
static int audio_index = 0;

void __not_in_flash_func(InfoNES_SoundOutput)
(int samples, BYTE *wave1, BYTE *wave2, BYTE *wave3, BYTE *wave4, BYTE *wave5) {
    //printf("InfoNES_SoundOutput: samples = %i\r\n", samples);
    for (uint_fast32_t i = 0; i < samples; i++) {
        int32_t sample = (wave1[i] + wave2[i] + wave3[i] + wave4[i] + wave5[i]) / 5;
        audio_buffer[audio_index] = (sample - 128) * 256;
        audio_index++;
        if (audio_index >= 735) {
            platform->getAudio()->play(audio_buffer, audio_index);
            audio_index = 0;
        }
    }
}

void InfoNES_MessageBox(const char *pszMsg, ...) {
    printf("[MSG]");
    va_list args;
    va_start(args, pszMsg);
    vprintf(pszMsg, args);
    va_end(args);
    printf("\n");
}
