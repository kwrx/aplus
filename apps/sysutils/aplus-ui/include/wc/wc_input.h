#ifndef _WC_INPUT_H
#define _WC_INPUT_H

#include <wc/wc.h>
#include <stdint.h>
#include <stdbool.h>
#include <aplus/events.h>


#ifdef __cplusplus
extern "C" {
#endif


#define WC_INPUT_KEYPRESS_THRESHOLD     200


int wc_input_initialize(void);

bool wc_input_key_is_down(vkey_t vkey);
bool wc_input_key_is_up(vkey_t vkey);
bool wc_input_key_is_pressed(vkey_t vkey);

uint16_t wc_input_cursor_x(void);
uint16_t wc_input_cursor_y(void);
uint16_t wc_input_cursor_z(void);

#ifdef __cplusplus
}
#endif

#endif