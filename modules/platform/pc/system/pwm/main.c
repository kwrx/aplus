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
