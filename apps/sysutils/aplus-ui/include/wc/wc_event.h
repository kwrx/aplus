#ifndef _WC_EVENT_H
#define _WC_EVENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WC_EVENT_TYPE_REDRAW 0x0001
#define WC_EVENT_TYPE_INPUT  0x0002

typedef uint8_t wc_event_t;

int wc_event_initialize(void);
int wc_event_wait(wc_event_t *type);
int wc_event_post(wc_event_t type);


#ifdef __cplusplus
}
#endif

#endif
