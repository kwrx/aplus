#ifndef _APLUS_CONFIG_H
#define _APLUS_CONFIG_H

#define DEBUG
//#define ARP_DEBUG
//#define RTL8139_DEBUG
//#define NETIF_DEBUG
//#define PCI_DEBUG
//#define SYSCALL_DEBUG
//#define ELF_DEBUG
//#define SCHED_DEBUG
//#define SCHED_TIMING_DEBUG
//#define SHM_DEBUG
//#define SBRK_DEBUG
//#define IO_DEBUG

//#define CHUNKS_CHECKING				1


#define HAVE_SSE					1
//#define HAVE_NETWORK				1



#ifndef ARCH
#define ARCH						unknown
#endif




#define KERNEL_NAME					"aplus"
#define KERNEL_HOSTNAME				"warex"

#define KERNEL_RELEASE_FORMAT		"%d.%d.%d-%s"
#define KERNEL_RELEASE_MAJOR		0
#define KERNEL_RELEASE_MINOR		1
#define KERNEL_RELEASE_LOWER		0
#define KERNEL_RELEASE_SUFFIX		"generic"

#define KERNEL_MACHINE				ARCH

#define KERNEL_VERSION_FORMAT		"%s - %s %s"
#define KERNEL_VERSION_CODENAME		"TheRev"
#define KERNEL_VERSION_DATE			__DATE__
#define KERNEL_VERSION_TIME			__TIME__
#define KERNEL_VERSION_COMPILER		"gcc " __VERSION__


#endif
