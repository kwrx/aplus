/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/hal.h>
#include <aplus/input.h>
#include <aplus/events.h>
#include <aplus/errno.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include <dev/interface.h>
#include <dev/char.h>

#if defined(__i386__) || defined(__x86_64__)
#include <arch/x86/cpu.h>
#endif

#include "ps2-codemap.h"



MODULE_NAME("input/ps2");
MODULE_DEPS("dev/interface,dev/char");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



device_t keyboard = {

    .type = DEVICE_TYPE_CHAR,

    .name = "kbd",
    .description = "PS/2 keyboard input device",

    .major = 11,
    .minor = 0,

    .status = DEVICE_STATUS_UNKNOWN,

    .init =  NULL,
    .dnit =  NULL,
    .reset = NULL,

    .chr.io    = CHAR_IO_FBF,
    .chr.write = NULL,
    .chr.read  = NULL,
    .chr.flush = NULL

};

device_t mouse = {

    .type = DEVICE_TYPE_CHAR,

    .name = "mouse",
    .description = "PS/2 mouse input device",

    .major = 13,
    .minor = 32,

    .status = DEVICE_STATUS_UNKNOWN,

    .init =  NULL,
    .dnit =  NULL,
    .reset = NULL,

    .chr.io    = CHAR_IO_FBF,
    .chr.write = NULL,
    .chr.read  = NULL,
    .chr.flush = NULL

};


static bool __mouse_buttons[16] = { 0 };
static int __mouse_id = 0;


#define PS2_IO_PORT_DATA                0x60
#define PS2_IO_PORT_STATUS              0x64
#define PS2_IO_PORT_COMMAND             0x64


#define PS2_STATUS_OUTPUT               0x01
#define PS2_STATUS_INPUT                0x02
#define PS2_STATUS_SYSTEM               0x04
#define PS2_STATUS_CMD_DATA             0x08
#define PS2_STATUS_SYS_FLAG             0x10
#define PS2_STATUS_INHIBIT_FLAG         0x20
#define PS2_STATUS_AUX_FLAG             0x40
#define PS2_STATUS_TIMEOUT_FLAG         0x80


#define PS2_COMMAND_READ(x)             ((x) + 0x20)
#define PS2_COMMAND_WRITE(x)            ((x) + 0x60)
#define PS2_COMMAND_DISABLE_AUX         0xA7
#define PS2_COMMAND_ENABLE_AUX          0xA8
#define PS2_COMMAND_ENABLE              0xAE
#define PS2_COMMAND_WRITE_AUX           0xD4
#define PS2_COMMAND_RESET               0xFF

#define PS2_CFG_FIRST_INTR              0x01
#define PS2_CFG_SECOND_INTR             0x02
#define PS2_CFG_SYSTEM_FLAG             0x04
#define PS2_CFG_FIRST_CLOCK             0x10
#define PS2_CFG_SECOND_CLOCK            0x20
#define PS2_CFG_FIRST_TRANSLATE         0x40

#define PS2_CTL_SYSTEM_RESET            0x01
#define PS2_CTL_A20_GATE                0x02


#define PS2_MOUSE_RESET                     0xFF
#define PS2_MOUSE_RESEND                    0xFE
#define PS2_MOUSE_SET_DEFAULTS              0xF6
#define PS2_MOUSE_SET_SAMPLE_RATE           0xF3
#define PS2_MOUSE_SET_REMOTE                0xF0
#define PS2_MOUSE_SET_STREAM                0xEA
#define PS2_MOUSE_SET_WRAP                  0xEE
#define PS2_MOUSE_SET_RESOLUTION            0xE8
#define PS2_MOUSE_SET_SCALING               0xE6
#define PS2_MOUSE_SET_ACCELERATION          0xE7
#define PS2_MOUSE_REQUEST_SINGLE_PACKET     0xEB
#define PS2_MOUSE_ENABLE_DATA_REPORTING     0xF4
#define PS2_MOUSE_DISABLE_DATA_REPORTING    0xF5
#define PS2_MOUSE_REQUEST_STATUS            0xE9
#define PS2_MOUSE_REQUEST_ID                0xF2

#define PS2_KBD_RESEND                  0xFE
#define PS2_ACK                         0xFA


#define __button_is_down(x)             (__mouse_buttons[(x)] == true)
#define __button_is_up(x)               (__mouse_buttons[(x)] == false)
#define __button_set_state(x, y)        (__mouse_buttons[(x)] = !!(y))




static inline void ps2_wait(uint8_t mask, uint8_t status) {

    int timeout = 100000;

    while((!!(inb(PS2_IO_PORT_STATUS) & mask)) != status && --timeout) {
        __cpu_pause();
    }

    DEBUG_ASSERT(timeout);

}

static inline void ps2_write(uint16_t port, uint8_t data) {

    ps2_wait(PS2_STATUS_INPUT, 0);
    outb(port, data);

}

static inline uint8_t ps2_read(uint16_t port) {

    ps2_wait(PS2_STATUS_OUTPUT, 1);
    return inb(port);

}

static inline void ps2_ack() {

    if(ps2_read(PS2_IO_PORT_DATA) != PS2_ACK) {
        kpanicf("ps2: PS/2 controller ack error");
    }

}



void ps2_keyboard_irq(void* context, irq_t irq) {

    static int vk_e0 = 0;


    uint8_t vkscan = ps2_read(PS2_IO_PORT_DATA);

    switch(vkscan) {

        case PS2_ACK:
        case PS2_KBD_RESEND:
            return;

        case 0xE0:
        case 0xE1:
            vk_e0 = 1;
            return;

    }


    uint16_t vkey = KEY_RESERVED;

    if(vk_e0) {
        vkey = ps2_codemap[0x100 | (vkscan & 0x7F)];
    } else {
        vkey = ps2_codemap[vkscan & 0x7F];
    }

    vk_e0 = 0;


    if(vkey == KEY_RESERVED) {
        return;
    }
    

    event_t e = {};

    e.ev_devid    = (keyboard.major << 16) | (keyboard.minor & 0xFFFF);
    e.ev_type     = EV_KEY;
    e.ev_key.vkey = (vkey);
    e.ev_key.down = (vkscan & 0x80) ? 0 : 1;
    
    vfs_write(keyboard.inode, &e, 0, sizeof(e));

}


void ps2_mouse_irq(void* context, irq_t irq) {

    static uint8_t packet[5] = { 0 };
    static uint8_t cycle = 0;


    uint8_t s = inb(PS2_IO_PORT_STATUS);
    uint8_t d = 0;

    while((s & 0x01) != 0) {
        
        d = inb(PS2_IO_PORT_DATA);

        if(s & 0x20) {

            packet[cycle] = d;

            switch(cycle) {

                case 0:

                    if((packet[cycle] & 0x08) == 0)
                        return;

                    cycle++;
                    break;

                case 1:

                    cycle++;
                    break;

                case 2:
                case 3:

                    if(__mouse_id > 0 && cycle == 2) {
                        cycle++;
                        break;
                    }


                    for(size_t button = 0; button < 3; button++) {

                        if(__button_is_up(button) && (packet[0] & (1 << button))) {

                            event_t ev = {};

                            ev.ev_devid    = (mouse.major << 16) | (mouse.minor & 0xFFFF);
                            ev.ev_type     = EV_KEY;
                            ev.ev_key.vkey = BTN_MOUSE + button;
                            ev.ev_key.down = 1;

                            vfs_write(mouse.inode, &ev, 0, sizeof(ev));

                        } else if (__button_is_down(button) && !(packet[0] & (1 << button))) {

                            event_t ev = {};

                            ev.ev_devid    = (mouse.major << 16) | (mouse.minor & 0xFFFF);
                            ev.ev_type     = EV_KEY;
                            ev.ev_key.vkey = BTN_MOUSE + button;
                            ev.ev_key.down = 0;

                            vfs_write(mouse.inode, &ev, 0, sizeof(ev));

                        }

                        __button_set_state(button, packet[0] & (1 << button));

                    }


                    if(__mouse_id == 4) {

                        for(size_t button = 4; button < 6; button++) {

                            if(__button_is_up(button) && (packet[3] & (1 << button))) {

                                event_t ev = {};

                                ev.ev_devid    = (mouse.major << 16) | (mouse.minor & 0xFFFF);
                                ev.ev_type     = EV_KEY;
                                ev.ev_key.vkey = BTN_MOUSE + button;
                                ev.ev_key.down = 1;

                                vfs_write(mouse.inode, &ev, 0, sizeof(ev));

                            } else if (__button_is_down(button) && !(packet[3] & (1 << button))) {

                                event_t ev = {};

                                ev.ev_devid    = (mouse.major << 16) | (mouse.minor & 0xFFFF);
                                ev.ev_type     = EV_KEY;
                                ev.ev_key.vkey = BTN_MOUSE + button;
                                ev.ev_key.down = 0;

                                vfs_write(mouse.inode, &ev, 0, sizeof(ev));

                            }

                            __button_set_state(button, packet[0] & (1 << button));

                        }

                    }


                    if((packet[0] & 0x40) == 0 && (packet[0] & 0x80) == 0) {

                        event_t ev = {};

                        ev.ev_devid    = (mouse.major << 16) | (mouse.minor & 0xFFFF);
                        ev.ev_type     = EV_REL;
                        ev.ev_rel.x    = (vaxis_t) (packet[1] - ((packet[0] & 0x10) ? 256 : 0));
                        ev.ev_rel.y    = (vaxis_t) (packet[2] - ((packet[0] & 0x20) ? 256 : 0));


                        switch(__mouse_id) {

                            case 3:
                                ev.ev_rel.z    = (vaxis_t) (packet[3] - ((packet[3] & 0x80) ? 256 : 0));
                                break;

                            case 4:
                                ev.ev_rel.z    = (vaxis_t) (packet[3] - ((packet[3] & 0x08) ? 16 : 0));                
                                break;

                            default:
                                ev.ev_rel.z    = 0;
                                break;

                        }

                        vfs_write(mouse.inode, &ev, 0, sizeof(ev));

                    }


                    cycle = 0;

                    break;


            }

        }


        s = inb(PS2_IO_PORT_STATUS);

    }

}




void init(const char* args) {


    memset(&__mouse_buttons[0], 0, sizeof(__mouse_buttons));

    
    inb(PS2_IO_PORT_DATA);

    ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_ENABLE);
    ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_ENABLE_AUX);
    ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_READ(0));


    uint8_t status = ps2_read(PS2_IO_PORT_DATA);


    ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE(0));
    ps2_write(PS2_IO_PORT_DATA, status | PS2_CFG_FIRST_INTR | PS2_CFG_SECOND_INTR);

    device_mkdev(&keyboard, 0666);


    if((status & PS2_CFG_SECOND_CLOCK) == 0) {

        ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
        ps2_write(PS2_IO_PORT_DATA, PS2_MOUSE_REQUEST_ID);

        ps2_ack();

        __mouse_id = ps2_read(PS2_IO_PORT_DATA);


        ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
        ps2_write(PS2_IO_PORT_DATA, PS2_MOUSE_SET_DEFAULTS);

        ps2_ack();


        if(__mouse_id == 0) {

            ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
            ps2_write(PS2_IO_PORT_DATA, PS2_MOUSE_SET_SAMPLE_RATE);

            ps2_ack();

            ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
            ps2_write(PS2_IO_PORT_DATA, 200);

            ps2_ack();

            ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
            ps2_write(PS2_IO_PORT_DATA, PS2_MOUSE_SET_SAMPLE_RATE);

            ps2_ack();

            ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
            ps2_write(PS2_IO_PORT_DATA, 100);

            ps2_ack();

            ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
            ps2_write(PS2_IO_PORT_DATA, PS2_MOUSE_SET_SAMPLE_RATE);

            ps2_ack();

            ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
            ps2_write(PS2_IO_PORT_DATA, 80);

            ps2_ack();

            ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
            ps2_write(PS2_IO_PORT_DATA, PS2_MOUSE_REQUEST_ID);

            ps2_ack();

            __mouse_id = ps2_read(PS2_IO_PORT_DATA);

        }


        if(__mouse_id == 3) {

            ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
            ps2_write(PS2_IO_PORT_DATA, PS2_MOUSE_SET_SAMPLE_RATE);

            ps2_ack();

            ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
            ps2_write(PS2_IO_PORT_DATA, 200);

            ps2_ack();

            ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
            ps2_write(PS2_IO_PORT_DATA, PS2_MOUSE_SET_SAMPLE_RATE);

            ps2_ack();

            ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
            ps2_write(PS2_IO_PORT_DATA, 200);

            ps2_ack();

            ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
            ps2_write(PS2_IO_PORT_DATA, PS2_MOUSE_SET_SAMPLE_RATE);

            ps2_ack();

            ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
            ps2_write(PS2_IO_PORT_DATA, 80);

            ps2_ack();

            ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
            ps2_write(PS2_IO_PORT_DATA, PS2_MOUSE_REQUEST_ID);

            ps2_ack();

            __mouse_id = ps2_read(PS2_IO_PORT_DATA);

        }


        ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_WRITE_AUX);
        ps2_write(PS2_IO_PORT_DATA, PS2_MOUSE_ENABLE_DATA_REPORTING);

        ps2_ack();


        device_mkdev(&mouse, 0666);

    } else {

#if DEBUG_LEVEL_WARN
        kprintf("ps2: WARN! PS/2 mouse device not found\n");
#endif

        ps2_write(PS2_IO_PORT_COMMAND, PS2_COMMAND_DISABLE_AUX);

    }
 


    arch_intr_map_irq(1,  ps2_keyboard_irq);
    arch_intr_map_irq(12, ps2_mouse_irq);


}


void dnit(void) {
    device_unlink(&keyboard);
    device_unlink(&mouse);
}