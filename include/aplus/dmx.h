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


#ifndef _DMX_H
#define _DMX_H

#include <stdint.h>
#include <cairo/cairo.h>

#include <aplus/base.h>
#include <aplus/utils/list.h>

#define DMX_PIPE                            "/tmp/dmxctl"

#define DMXWF_HIDE                          1
#define DMXWF_FULLSCREEN                    2
#define DMXWF_FOCUS                         4
#define DMXWF_NOBORDER                      8

#define DMX_CURSOR_ARROW                    0
#define DMX_CURSOR_CROSS                    1
#define DMX_CURSOR_FORBIDDEN                2
#define DMX_CURSOR_HAND                     3
#define DMX_CURSOR_HELP                     4
#define DMX_CURSOR_PENCIL                   5
#define DMX_CURSOR_SIZEALL                  6
#define DMX_CURSOR_SIZEBDIAG                7
#define DMX_CURSOR_SIZEFDIAG                8
#define DMX_CURSOR_SIZEHOR                  9
#define DMX_CURSOR_SIZEVER                  10
#define DMX_CURSOR_TEXT                     11
#define DMX_CURSOR_UPARROW                  12
#define DMX_CURSOR_COUNT                    13



#define DMX_FONT_TYPE_REGULAR               (0 << 3)
#define DMX_FONT_TYPE_CONDENSED             (1 << 3)
#define DMX_FONT_TYPE_MONOSPACE             (2 << 3)

#define DMX_FONT_WEIGHT_REGULAR             (0 << 1)
#define DMX_FONT_WEIGHT_LIGHT               (1 << 1)
#define DMX_FONT_WEIGHT_MEDIUM              (2 << 1)
#define DMX_FONT_WEIGHT_BOLD                (3 << 1)

#define DMX_FONT_STYLE_NORMAL               (0)
#define DMX_FONT_STYLE_ITALIC               (1)


#define DMX_PROTO_CONNECT                   0
#define DMX_PROTO_DISCONNECT                1
#define DMX_PROTO_CREATE_GC                 2
#define DMX_PROTO_DESTROY_GC                3




#ifdef __cplusplus
extern "C" {
#endif



typedef struct {
    pid_t gcid;
    pid_t pid;

    struct {
        double w;
        double h;
        double x;
        double y;
        double alpha;
        uint16_t font;
        char title[128];
        void* frame;
    } window;

    struct {
        uint16_t width;
        uint16_t height;
        uint16_t bpp;
        uint32_t stride;
        uint16_t format;
    } screen;
     

    int flags;
    int dirty;
} dmx_gc_t;




/* LOW LEVEL PROTOCOL */
typedef struct {
    pid_t pid;
} dmx_packet_connection_t;


typedef struct {
    /* DMX_PROTO_CREATE_GC */
    pid_t pid;
    double width;
    double height;

    /* DMX_PROTO_DESTROY_GC */
    /* DMX_PROTO_REDRAW_GC  */
    dmx_gc_t* gc;
} dmx_packet_gc_t;

typedef struct {
    int type;
    void* arg;
} dmx_packet_ack_t;



#ifdef __cplusplus
}
#endif

#endif



