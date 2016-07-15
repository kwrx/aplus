#include <xdev.h>
#include <libc.h>

static char __hostname[] = KERNEL_NAME;
char* hostname = __hostname;


EXPORT(hostname);
