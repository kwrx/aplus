#ifndef _WC_H
#define _WC_H

#define _POSIX_SOURCE
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wc_ref {
        int refcount;
        int (*dtor)(void*);
        void* object;
} wc_ref_t;


#define wc_ref_init(ref, func, data)                 \
    do {                                             \
        (ref)->refcount = 1;                         \
        (ref)->dtor     = (int (*)(void*)) & (func); \
        (ref)->object   = (void*)(data);             \
    } while (0)

#define wc_ref_inc(ref) (++(ref)->refcount)

#define wc_ref_dec(ref)                 \
    do {                                \
        if (--(ref)->refcount == 0) {   \
            (ref)->dtor((ref)->object); \
        }                               \
    } while (0)


#define wc_clamp(x, min, max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))

#define wc_max(x, y) ((x) > (y) ? (x) : (y))

#define wc_min(x, y) ((x) < (y) ? (x) : (y))


static inline uint64_t wc_time() {

    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        return 0;
    }

    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

#ifdef __cplusplus
}
#endif

#endif
