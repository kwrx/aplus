#ifndef _APLUS_HAL_REBOOT_H
#define _APLUS_HAL_REBOOT_H

#ifndef __ASSEMBLY__
#include <sys/cdefs.h>
#include <aplus.h>



#define ARCH_REBOOT_RESTART         0
#define ARCH_REBOOT_SUSPEND         1
#define ARCH_REBOOT_POWEROFF        2
#define ARCH_REBOOT_HALT            3


__BEGIN_DECLS

void arch_reboot(int);

__END_DECLS

#endif
#endif