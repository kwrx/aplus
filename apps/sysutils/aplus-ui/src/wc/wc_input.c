
#include <wc/wc.h>
#include <wc/wc_input.h>
#include <wc/wc_display.h>
#include <wc/wc_event.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <math.h>

#include <pthread.h>

#include <aplus/events.h>
#include <aplus/input.h>



static pthread_t thread_mouse;
static pthread_t thread_keyboard;


static int64_t input_state[KEY_MAX];
static int16_t cursor_x = 0;
static int16_t cursor_y = 0;
static int16_t cursor_z = 0;



static void* thread_event_fn(const char* device) {


    int fd = open(device, O_RDONLY);

    if(fd < 0) {
        return LOG("FATAL! failed to open %s: %s\n", device, strerror(errno)), NULL;
    }


    do {

        event_t ev = { 0 };

        if(read(fd, &ev, sizeof(ev)) != sizeof(ev))
            continue;


        switch(ev.ev_type) {

            case EV_REL:

                ev.ev_rel.x *= (log10(abs(ev.ev_rel.x) + abs(ev.ev_rel.y)) * 0.5) + 1;
                ev.ev_rel.y *= (log10(abs(ev.ev_rel.x) + abs(ev.ev_rel.y)) * 0.5) + 1;
                
                if(wc_display_at_position(cursor_x + ev.ev_rel.x, cursor_y - ev.ev_rel.y) != NULL) {

                    cursor_x += ev.ev_rel.x;
                    cursor_y -= ev.ev_rel.y;

                } else {

                    const wc_display_t* display = wc_display_at_position(cursor_x, cursor_y);

                    if(display) {

                        cursor_x = wc_clamp(cursor_x + ev.ev_rel.x, display->offset_x, display->offset_x + display->var.xres);
                        cursor_y = wc_clamp(cursor_y - ev.ev_rel.y, display->offset_y, display->offset_y + display->var.yres);

                    }

                }

                cursor_z += ev.ev_rel.z;

                break;

            case EV_ABS:

                if(wc_display_at_position(ev.ev_abs.x, ev.ev_abs.y) != NULL) {

                    cursor_x = ev.ev_abs.x;
                    cursor_y = ev.ev_abs.y;

                }

                cursor_z = ev.ev_abs.z;

                break;

            case EV_KEY:
                
                if(ev.ev_key.down) {
                    input_state[ev.ev_key.vkey] = wc_time();
                } else {
                    input_state[ev.ev_key.vkey] = 0;
                }

                break;

            default:
                break;

        }


        wc_event_post(WC_EVENT_TYPE_INPUT);
        

    } while(1);

}




int wc_input_initialize(void) {

    memset(input_state, 0, sizeof(input_state));


    if(pthread_create(&thread_mouse, NULL, (void* (*)(void*)) &thread_event_fn, "/dev/mouse") < 0)
        return -1;

    if(pthread_create(&thread_keyboard, NULL, (void* (*)(void*)) &thread_event_fn, "/dev/kbd") < 0)
        return -1;


    LOG("input subsystem initialized\n");

    return 0;

}


bool wc_input_key_is_down(vkey_t vkey) {
    return input_state[vkey] > 0;
}

bool wc_input_key_is_up(vkey_t vkey) {
    return input_state[vkey] == 0;
}

bool wc_input_key_is_pressed(vkey_t vkey) {
    return input_state[vkey] > 0 && (wc_time() - input_state[vkey]) > WC_INPUT_KEYPRESS_THRESHOLD;
}

uint16_t wc_input_cursor_x(void) {
    return cursor_x;
}

uint16_t wc_input_cursor_y(void) {
    return cursor_y;
}

uint16_t wc_input_cursor_z(void) {
    return cursor_z;
}