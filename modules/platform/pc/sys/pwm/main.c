#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/base.h>
#include <aplus/pwm.h>
#include <libc.h>

MODULE_NAME("pc/sys/pwm");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#if defined(__i386__) || defined(__x86_64__)
#	if defined(__i386__)
#		include <arch/i386/i386.h>
#	elif defined(__x86_64__)
#		include <arch/x86_64/x86_64.h>
#	endif


static int pwm_ioctl(struct inode* inode, int req, void* buf) {
	if(!inode) {
		errno = EINVAL;
		return E_ERR;
	}

	switch(req) {
		case PWMIOCTL_POWEROFF:
			/* TODO */
		case PWMIOCTL_REBOOT:
			while(inb(0x64) & 2)
				;
			outb(0x64, 0xFE);
		case PWMIOCTL_HALT:
			__asm__ __volatile__ (
				"cli; hlt;"
			); for(;;);

		case PWMIOCTL_STANDBY:
		default:
			errno = ENOSYS;
			return E_ERR;
	}

	return E_OK;
}



int init(void) {
	inode_t* ino = vfs_mkdev("pwm", -1, S_IFCHR | 0440);
	ino->ioctl = pwm_ioctl;

	
	return E_OK;
}


#else

int init(void) {
	return E_ERR;
}

#endif


int dnit(void) {
	return E_OK;
}
