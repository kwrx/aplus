#ifndef _WC_CURSOR_H
#define _WC_CURSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <wc/wc.h>
#include <wc/wc_renderer.h>

#define WC_CURSOR_TYPE_NONE    0
#define WC_CURSOR_TYPE_POINTER 1
#define WC_CURSOR_TYPE_TEXT    2
#define WC_CURSOR_TYPE_BUSY    3
#define WC_CURSOR_TYPE_RESIZE  4
#define WC_CURSOR_TYPE_MOVE    5
#define WC_CURSOR_TYPE_HAND    6
#define WC_CURSOR_TYPE_HELP    7
#define WC_CURSOR_TYPE_CUSTOM  8
#define WC_CURSOR_TYPE_LENGTH  9


int wc_cursor_initialize(void);
int wc_cursor_set_type(uint16_t type);
int wc_cursor_set_fallback(uint16_t type);
int wc_cursor_load(uint16_t type, const char* path);
int wc_cursor_render(wc_renderer_t* renderer);



#ifdef __cplusplus
}
#endif


#endif
