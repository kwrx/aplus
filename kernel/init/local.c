#include <aplus.h>
#include <aplus/base.h>
#include <aplus/sysconfig.h>
#include <libc.h>

int local_timezone = 0;

int local_init() {
    local_timezone = sysconfig("sys.timezone", SYSCONFIG_FORMAT_INT, 0);

    return E_OK;
}


EXPORT(local_timezone);
