#ifndef _CONFIG_H
#define _CONFIG_H

#define CONFIG_HAVE_LIBC			1
#define CONFIG_SMP				0
#define CONFIG_IPC				1
#define CONFIG_VMM				1
#define CONFIG_NETWORK				1
#define CONFIG_BOCHS				1
#define CONFIG_CLOCKS_PER_SEC			1000

#define DEBUG					1





#define KERNEL_NAME				"aplus"
#define KERNEL_VERSION				"0.1"
#define KERNEL_CODENAME				"generic"
#define KERNEL_DATE				__DATE__
#define KERNEL_TIME				__TIME__

#if DEBUG
#	undef KERNEL_CODENAME
#	define KERNEL_CODENAME			"debug"
#endif


#if defined(__i386__)
#	define CONFIG_BITS				32
#	define CONFIG_KERNEL_BASE			0xC0000000L
#	define CONFIG_HEAP_BASE				0xD0000000L
#	define CONFIG_STACK_BASE			0xFFC00000L
#	define CONFIG_HEAP_SIZE				0x01000000ULL
#	define CONFIG_STACK_SIZE			0x00001000ULL
#	define KERNEL_PLATFORM				"i386"
#elif defined(__x86_64__)
#	define CONFIG_BITS				64
#	define CONFIG_KERNEL_BASE			0xFFFFFFFFC0000000L
#	define CONFIG_HEAP_BASE				0xFFFFFFFFD0000000L
#	define CONFIG_STACK_BASE			0xFFFFFFFFF0000000L
#	define CONFIG_HEAP_SIZE				0x01000000ULL
#	define CONFIG_STACK_SIZE			0x00004000ULL
#	define KERNEL_PLATFORM				"x86_64"
#elif defined(__arm__)
#	define CONFIG_BITS				32
#	if defined (__rpi__)
		/* TODO */
#		define KERNEL_PLATFORM			"armv6-rpi"
#	else
#		define CONFIG_KERNEL_BASE		0x80010000L
#		define CONFIG_HEAP_BASE			0x8F000000L
#		define CONFIG_STACK_BASE		0x8A000000L
#		define CONFIG_HEAP_SIZE			0x00100000ULL
#		define CONFIG_STACK_SIZE		0x00004000ULL
#		define KERNEL_PLATFORM			"armv7"
#	endif
#endif

#ifndef KERNEL_PLATFORM
#define KERNEL_PLATFORM					"unknown"
#endif


#ifdef __GNUC__
#define likely(x)				__builtin_expect(!!(x), 1)
#define unlikely(x)				__builtin_expect(!!(x), 0)
#else
#define likely(x)				(!!(x))
#define unlikely(x)				(!!(x))
#endif

#ifndef __weak
#define __weak					__attribute__((weak))
#endif

#ifndef __packed
#define __packed				__attribute__((packed))
#endif

#ifndef __section
#define __section(x)				__attribute__((section(x)))
#endif

#ifndef __align
#define __align(x)				__attribute__((align(x)))
#endif

#ifndef __malloc
#define __malloc				__attribute__((malloc))
#endif

#ifndef __optimize
#define __optimize(x)				__attribute__((optimize("O" #x)))
#endif

#ifdef __GNUC__
#define __PRAGMA(x)				_Pragma(#x)
#define WARNING(x)				__PRAGMA(GCC diagnostic ignored x)
#else
#define WARNING(x)
#endif



#endif

