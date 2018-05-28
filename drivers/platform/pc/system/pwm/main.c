/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/base.h>
#include <aplus/pwm.h>
#include <aplus/events.h>
#include <libc.h>

MODULE_NAME("pc/system/pwm");
MODULE_DEPS("arch/x86,system/events");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#if defined(__i386__) || defined(__x86_64__)
#    if defined(__i386__)
#        include <arch/i386/i386.h>
#    elif defined(__x86_64__)
#        include <arch/x86_64/x86_64.h>
#    endif


static evid_t pwid;

static int pwm_ioctl(struct inode* inode, int req, void* buf) {
    if(!inode) {
        errno = EINVAL;
        return -1;
    }

    switch(req) {
        case PWMIOCTL_HALT:
        case PWMIOCTL_POWEROFF:
        case PWMIOCTL_REBOOT:
            sys_events_raise_EV_PWR(pwid, 1);
            sys_sync();
            module_dnit();
        default:
            break;
    }

    switch(req) {
        case PWMIOCTL_POWEROFF:
            kprintf(INFO "pwm: shutdown\n");
            /* TODO */
        case PWMIOCTL_REBOOT:
            kprintf(INFO "pwm: rebooting\n");
            
            while(inb(0x64) & 2)
                ;
            outb(0x64, 0xFE);
        case PWMIOCTL_HALT:
            kprintf(INFO "pwm: system halted\n");

            __asm__ __volatile__ (
                "cli; hlt;"
            ); for(;;);

        case PWMIOCTL_STANDBY:
            kprintf(INFO "pwm: standby\n");

        default:
            errno = ENOSYS;
            return -1;
    }

    return 0;
}



int init(void) {
    inode_t* ino = vfs_mkdev("pwm", -1, S_IFCHR | 0440);
    ino->ioctl = pwm_ioctl;

    pwid = sys_events_device_add("system-power-manager", EC_PWR);
    sys_events_device_set_enabled(pwid, 1);
    return 0;
}


#else

int init(void) {
    return -1;
}

#endif


int dnit(void) {
    return 0;
}
