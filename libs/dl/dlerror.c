#include "dl.h"

static char* errors[] = {
    "success",
    "no such library",
    "invalid library",
    "unsupported library format or function",
    "undefined reference",
    "symbol already defined",
    "no memory",
    "I/O error",
    NULL
};

char *dlerror(void) {
    if(__dlerrno > (sizeof(errors) / sizeof(char*)))
        return "unknown error";

    return errors[__dlerrno];
}