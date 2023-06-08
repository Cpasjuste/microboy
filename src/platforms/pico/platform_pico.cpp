//
// Created by cpasjuste on 30/05/23.
//

#include <pico/time.h>
#include <pico/stdlib.h>
#include <hardware/vreg.h>
#include <class/cdc/cdc_device.h>
#include "platform.h"

using namespace mb;

PicoPlatform::PicoPlatform() : Platform() {
    // overclock - the value for VCO set here is meant for least power consumption
    const unsigned vco = 1596 * 1000 * 1000; // 266MHz
    const unsigned div1 = 6, div2 = 1;
    vreg_set_voltage(VREG_VOLTAGE_DEFAULT);
    //vreg_set_voltage(VREG_VOLTAGE_1_30);
    sleep_ms(2);
    set_sys_clock_pll(vco, div1, div2);
    //set_sys_clock_khz(300000, true);
    sleep_ms(2);

    // initialise USB serial connection for debugging
    stdio_init_all();
#ifndef NDEBUG
    // wait for usb serial
    while (!tud_cdc_connected()) { sleep_ms(100); }
#endif

    p_display = new PicoDisplay();
    p_audio = new PicoAudio();
    p_input = new PicoInput();
    p_io = new PicoIo();
}

PicoPlatform::~PicoPlatform() {
    printf("~PicoPlatform()\n");
    reset_usb_boot(0, 0);
}
