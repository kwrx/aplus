#ifndef _KX_INTERNAL_H
#define _KX_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include "include/kx.h"


extern kx_context_t* __kx;

#define __alphablend(dest, src, alpha)	\
	(src)


#define _A(color)	(color & 0xFF000000)
#define _R(color)	(color & 0x00FF0000)
#define _G(color)	(color & 0x0000FF00)
#define _B(color)	(color & 0x000000FF)


static inline void memcpy_px(pixel_t* dst, pixel_t* src, size_t size) {
	__asm__ __volatile__ ("cli; rep movsl" : : "c"(size), "S"(src), "D"(dst));
}

static inline void memset_px(pixel_t* dst, pixel_t val, size_t size) {
	__asm__ __volatile__ ("cli; rep stosl" : : "a"(val), "c"(size), "D"(dst));
}


#define __check_and_swap(n1, n2) 		\
	{ 									\
		if(n1 > n2) {					\
			register int n1##_tmp = n2;	\
			n2 = n1;					\
			n1 = n1##_tmp;				\
		}								\
	}									\



#endif