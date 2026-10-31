/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2026 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/events.h>
#include <aplus/hal.h>
#include <aplus/input.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <dev/char.h>
#include <dev/interface.h>
#include <dev/usb/hid.h>


MODULE_NAME("usb/keyboard");
MODULE_DEPS("dev/interface,dev/char,usb/hid,input/ps2");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#define KBD_REPORT_KEYS      2
#define KBD_ROLLOVER_ERROR   0x01
#define KBD_FIRST_USAGE      0x04
#define KBD_REPEAT_DELAY_MS  250
#define KBD_REPEAT_PERIOD_MS 33


static const uint16_t kbd_usage_keys[256] = {
    [0x04] = KEY_A,          [0x05] = KEY_B,          [0x06] = KEY_C,          [0x07] = KEY_D,          [0x08] = KEY_E,          [0x09] = KEY_F,         [0x0a] = KEY_G,
    [0x0b] = KEY_H,          [0x0c] = KEY_I,          [0x0d] = KEY_J,          [0x0e] = KEY_K,          [0x0f] = KEY_L,          [0x10] = KEY_M,         [0x11] = KEY_N,
    [0x12] = KEY_O,          [0x13] = KEY_P,          [0x14] = KEY_Q,          [0x15] = KEY_R,          [0x16] = KEY_S,          [0x17] = KEY_T,         [0x18] = KEY_U,
    [0x19] = KEY_V,          [0x1a] = KEY_W,          [0x1b] = KEY_X,          [0x1c] = KEY_Y,          [0x1d] = KEY_Z,          [0x1e] = KEY_1,         [0x1f] = KEY_2,
    [0x20] = KEY_3,          [0x21] = KEY_4,          [0x22] = KEY_5,          [0x23] = KEY_6,          [0x24] = KEY_7,          [0x25] = KEY_8,         [0x26] = KEY_9,
    [0x27] = KEY_0,          [0x28] = KEY_ENTER,      [0x29] = KEY_ESC,        [0x2a] = KEY_BACKSPACE,  [0x2b] = KEY_TAB,        [0x2c] = KEY_SPACE,     [0x2d] = KEY_MINUS,
    [0x2e] = KEY_EQUAL,      [0x2f] = KEY_LEFTBRACE,  [0x30] = KEY_RIGHTBRACE, [0x31] = KEY_BACKSLASH,  [0x32] = KEY_BACKSLASH,  [0x33] = KEY_SEMICOLON, [0x34] = KEY_APOSTROPHE,
    [0x35] = KEY_GRAVE,      [0x36] = KEY_COMMA,      [0x37] = KEY_DOT,        [0x38] = KEY_SLASH,      [0x39] = KEY_CAPSLOCK,   [0x3a] = KEY_F1,        [0x3b] = KEY_F2,
    [0x3c] = KEY_F3,         [0x3d] = KEY_F4,         [0x3e] = KEY_F5,         [0x3f] = KEY_F6,         [0x40] = KEY_F7,         [0x41] = KEY_F8,        [0x42] = KEY_F9,
    [0x43] = KEY_F10,        [0x44] = KEY_F11,        [0x45] = KEY_F12,        [0x46] = KEY_SYSRQ,      [0x47] = KEY_SCROLLLOCK, [0x48] = KEY_PAUSE,     [0x49] = KEY_INSERT,
    [0x4a] = KEY_HOME,       [0x4b] = KEY_PAGEUP,     [0x4c] = KEY_DELETE,     [0x4d] = KEY_END,        [0x4e] = KEY_PAGEDOWN,   [0x4f] = KEY_RIGHT,     [0x50] = KEY_LEFT,
    [0x51] = KEY_DOWN,       [0x52] = KEY_UP,         [0x53] = KEY_NUMLOCK,    [0x54] = KEY_KPSLASH,    [0x55] = KEY_KPASTERISK, [0x56] = KEY_KPMINUS,   [0x57] = KEY_KPPLUS,
    [0x58] = KEY_KPENTER,    [0x59] = KEY_KP1,        [0x5a] = KEY_KP2,        [0x5b] = KEY_KP3,        [0x5c] = KEY_KP4,        [0x5d] = KEY_KP5,       [0x5e] = KEY_KP6,
    [0x5f] = KEY_KP7,        [0x60] = KEY_KP8,        [0x61] = KEY_KP9,        [0x62] = KEY_KP0,        [0x63] = KEY_KPDOT,      [0x64] = KEY_102ND,     [0x65] = KEY_COMPOSE,
    [0x66] = KEY_POWER,      [0x67] = KEY_KPEQUAL,    [0x68] = KEY_F13,        [0x69] = KEY_F14,        [0x6a] = KEY_F15,        [0x6b] = KEY_F16,       [0x6c] = KEY_F17,
    [0x6d] = KEY_F18,        [0x6e] = KEY_F19,        [0x6f] = KEY_F20,        [0x70] = KEY_F21,        [0x71] = KEY_F22,        [0x72] = KEY_F23,       [0x73] = KEY_F24,
    [0x7f] = KEY_MUTE,       [0x80] = KEY_VOLUMEUP,   [0x81] = KEY_VOLUMEDOWN, [0x85] = KEY_KPCOMMA,    [0x87] = KEY_RO,         [0x88] = KEY_KATAKANAHIRAGANA,
    [0x89] = KEY_YEN,        [0x8a] = KEY_HENKAN,     [0x8b] = KEY_MUHENKAN,   [0x8c] = KEY_KPJPCOMMA,  [0x90] = KEY_HANGEUL,    [0x91] = KEY_HANJA,
};

static const uint16_t kbd_modifier_keys[8] = {
    KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_LEFTALT, KEY_LEFTMETA, KEY_RIGHTCTRL, KEY_RIGHTSHIFT, KEY_RIGHTALT, KEY_RIGHTMETA,
};


struct usbkbd {
    uint8_t previous[USB_HID_REPORT_MAX];
    uint16_t repeat_key;
    uint64_t repeat_at;
};


static struct usb_hid_sink sink = {
    .device =
        {
            .type        = DEVICE_TYPE_CHAR,
            .name        = "kbd",
            .description = "USB keyboard input device",
            .major       = 11,
            .minor       = 1,
            .status      = DEVICE_STATUS_UNKNOWN,
            .chr.io      = CHAR_IO_FBF,
        },
};



static void usbkbd_key(uint16_t vkey, bool down) {

    event_t ev = {};

    ev.ev_type     = EV_KEY;
    ev.ev_key.vkey = vkey;
    ev.ev_key.down = down ? 1 : 0;

    usb_hid_sink_write(&sink, &ev);
}


static bool usbkbd_has(const uint8_t* report, uint8_t usage) {

    for (size_t i = KBD_REPORT_KEYS; i < USB_HID_REPORT_MAX; i++) {
        if (report[i] == usage)
            return true;
    }

    return false;
}


static void usbkbd_report(void* ctx, const uint8_t* report) {

    struct usbkbd* kbd = ctx;

    if (report[KBD_REPORT_KEYS] == KBD_ROLLOVER_ERROR)
        return;

    const uint8_t changed = report[0] ^ kbd->previous[0];

    for (size_t i = 0; i < 8; i++) {
        if (changed & (1 << i))
            usbkbd_key(kbd_modifier_keys[i], report[0] & (1 << i));
    }

    for (size_t i = KBD_REPORT_KEYS; i < USB_HID_REPORT_MAX; i++) {

        const uint8_t usage = kbd->previous[i];

        if (usage < KBD_FIRST_USAGE || kbd_usage_keys[usage] == KEY_RESERVED || usbkbd_has(report, usage))
            continue;

        usbkbd_key(kbd_usage_keys[usage], false);

        if (kbd->repeat_key == kbd_usage_keys[usage])
            kbd->repeat_key = KEY_RESERVED;
    }

    for (size_t i = KBD_REPORT_KEYS; i < USB_HID_REPORT_MAX; i++) {

        const uint8_t usage = report[i];

        if (usage < KBD_FIRST_USAGE || kbd_usage_keys[usage] == KEY_RESERVED || usbkbd_has(kbd->previous, usage))
            continue;

        usbkbd_key(kbd_usage_keys[usage], true);

        kbd->repeat_key = kbd_usage_keys[usage];
        kbd->repeat_at  = arch_timer_generic_getms() + KBD_REPEAT_DELAY_MS;
    }

    memcpy(kbd->previous, report, USB_HID_REPORT_MAX);
}


static void usbkbd_tick(void* ctx) {

    struct usbkbd* kbd = ctx;

    if (kbd->repeat_key == KEY_RESERVED)
        return;

    const uint64_t now = arch_timer_generic_getms();

    if (now < kbd->repeat_at)
        return;

    usbkbd_key(kbd->repeat_key, true);

    kbd->repeat_at = now + KBD_REPEAT_PERIOD_MS;
}


static void* usbkbd_attach(usbdev_t* dev) {
    return kcalloc(1, sizeof(struct usbkbd), GFP_KERNEL);
}


static void usbkbd_detach(void* ctx) {

    static const uint8_t released[USB_HID_REPORT_MAX] = {0};

    usbkbd_report(ctx, released);

    kfree(ctx);
}


static const struct usb_hid_handler handler = {
    .protocol = USB_HID_PROTOCOL_KEYBOARD,
    .attach   = usbkbd_attach,
    .report   = usbkbd_report,
    .tick     = usbkbd_tick,
    .detach   = usbkbd_detach,
};



void init(const char* args) {

    usb_hid_sink_open(&sink, "/dev/kbd");

    if (usb_hid_register(&handler) < 0) {
#if DEBUG_LEVEL_ERROR
        kprintf("usb-keyboard: ERROR! keyboards already have a handler\n");
#endif
    }
}


void dnit(void) {
    usb_hid_unregister(&handler);
    usb_hid_sink_close(&sink);
}
