/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _APLUS_EVENTS_H
#define _APLUS_EVENTS_H

#include <stdint.h>
#include <sys/ioctl.h>


#define EV_SYN                  0x00
#define EV_KEY                  0x01
#define EV_REL                  0x02
#define EV_ABS                  0x03
#define EV_MSC                  0x04
#define EV_SW                   0x05
#define EV_LED                  0x11
#define EV_SND                  0x12
#define EV_REP                  0x14
#define EV_FF                   0x15
#define EV_PWR                  0x16
#define EV_FF_STATUS            0x17
#define EV_MAX                  0x1f
#define EV_CNT                  (EV_MAX + 1)

#define EC_SYN                  (1 << 0x00)
#define EC_KEY                  (1 << 0x01)
#define EC_REL                  (1 << 0x02)
#define EC_ABS                  (1 << 0x03)
#define EC_MSC                  (1 << 0x04)
#define EC_SW                   (1 << 0x05)
#define EC_LED                  (1 << 0x11)
#define EC_SND                  (1 << 0x12)
#define EC_REP                  (1 << 0x14)
#define EC_FF                   (1 << 0x15)
#define EC_PWR                  (1 << 0x16)
#define EC_FF_STATUS            (1 << 0x17)

// // #define EVIOGID                 _IOR('e', 1, long[2])
// // #define EVIOGNAME               _IOR('e', 2, char[64 + sizeof(evid_t)])
// // #define EVIOGCAPS               _IOR('e', 3, evid_t)
// // #define EVIOGSTATUS             _IOR('e', 4, evid_t)
// // #define EVIOSSTATUS             _IOW('e', 5, long[2])
// // #define EVIOSEXCL               _IOW('e', 6, long[2])

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t vkey_t;
typedef int16_t vaxis_t;
typedef uint16_t evid_t;

typedef struct {

    evid_t  ev_devid;
    uint8_t ev_type;
    
    union {

        struct {
            vkey_t vkey;
            uint8_t down;
        } ev_key;

        struct {
            vaxis_t x;
            vaxis_t y;
            vaxis_t z;
        } ev_rel;

        struct {
            vaxis_t x;
            vaxis_t y;
            vaxis_t z;
        } ev_abs;

        struct {
            uint16_t raw_type;
            uint64_t raw_data;
        } ev_msc;

        struct {
            int status;
        } ev_sw;

        struct {
            int16_t ledno;
            uint8_t on;
        } ev_led;

        struct {
            int status;
        } ev_pwr;

        char ev_args[1];

    };

} __attribute__((packed)) event_t;


#ifdef __cplusplus
}
#endif

#endif