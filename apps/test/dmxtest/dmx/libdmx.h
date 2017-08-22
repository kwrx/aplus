#ifndef _LIBDMX_H
#define _LIBDMX_H

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include <aplus/base.h>
#include <aplus/dmx.h>
#include "lib/list.h"

#define LIBDMX_TIMEOUT          5
#define LIBDMX_ERROR(x, y)      \
    {                           \
        dmx_errno = x;          \
                                \
        if(y != 0)              \
            errno = y;          \
    }

extern list(dmx_context_t*, contexts);
extern char* dmx_errno;

#endif