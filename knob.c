/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"
#include "hardware/gpio.h"

#include "usb_descriptors.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum
{
    BLINK_NOT_MOUNTED = 250,
    BLINK_MOUNTED = 1000,
    BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void hid_task(void);
signed char gray_diff(signed char prev, signed char curr);

#define ENC1 5
#define ENC2 2
#define ENC3 4
#define ENC4 3

const char gray[] = {10, 11, 9, 8, 13, 12, 14, 15, 5, 4, 6, 7, 2, 3, 1, 0};
signed char prev;
signed char curr;
signed char diff;
bool clear_flag;

/*------------- MAIN -------------*/
int main(void)
{
    board_init();
    tusb_init();

    gpio_init(ENC1);
    gpio_pull_up(ENC1);
    gpio_init(ENC2);
    gpio_pull_up(ENC2);
    gpio_init(ENC3);
    gpio_pull_up(ENC3);
    gpio_init(ENC4);
    gpio_pull_up(ENC4);

    prev = gray[gpio_get(ENC1) | (gpio_get(ENC2) << 1) | (gpio_get(ENC3) << 2) | (gpio_get(ENC4) << 3)];

    while (true)
    {
        tud_task(); // tinyusb device task

        hid_task();
    }

    return 0;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
    blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    blink_interval_ms = BLINK_MOUNTED;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

void hid_task(void)
{
    // Poll every 10ms
    const uint32_t interval_ms = 10;
    static uint32_t start_ms = 0;

    if (board_millis() - start_ms < interval_ms)
        return; // not enough time
    start_ms += interval_ms;

    uint32_t const btn = 1;

    // Remote wakeup
    if (tud_suspended() && btn)
    {
        // Wake up host if we are in suspend mode
        // and REMOTE_WAKEUP feature is enabled by host
        tud_remote_wakeup();
    }

    /*------------- Mouse -------------*/
    // if (tud_hid_ready())
    // {
    // if (btn) {
    //     int8_t const delta = 5;

    //     // no button, right + down, no scroll pan
    //     tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, delta, delta, 0, 0);

    //     // delay a bit before attempt to send keyboard report
    //     board_delay(10);
    // }
    // }

    /*------------- Keyboard -------------*/
    if (tud_hid_ready())
    {
        if (clear_flag)
        {
            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
            clear_flag = false;
        }
        else
        {
            curr = gray[gpio_get(ENC1) | (gpio_get(ENC2) << 1) | (gpio_get(ENC3) << 2) | (gpio_get(ENC4) << 3)];

            diff = gray_diff(prev, curr);

            if (diff != 0)
            {
                uint8_t keycode[6] = {0};
                
                if (diff > 0)
                    keycode[0] = HID_KEY_BACKSLASH;
                else if (diff < 0)
                    keycode[0] = HID_KEY_BRACKET_RIGHT;

                tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
                clear_flag = true;
            }

            prev = curr;
        }
    }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    // TODO not Implemented
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    // TODO set LED based on CAPLOCK, NUMLOCK etc...
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

signed char gray_diff(signed char prev, signed char curr)
{
    signed char diff = curr - prev;

    if (diff == 0 || diff == -8 || diff == 8)
        return 0;

    if (diff > 8)
        return diff - 16;

    if (diff < -8)
        return diff + 16;

    return diff;
}