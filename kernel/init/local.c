#include <aplus.h>
#include <aplus/base.h>
#include <aplus/sysconfig.h>
#include <libc.h>

int local_timezone = 0;

int local_init() {
    local_timezone = (int) sysconfig("sys.timezone", 0);

    return 0;
}


EXPORT(local_timezone);
