#ifndef _MMIO_H
#define _MMIO_H

#include <aplus.h>
#include <stdint.h>


#define mmio_rxx(t, x)									\
	static inline t mmio_r##x (uintptr_t a) {			\
		return *((volatile t *) (a));					\
	}

mmio_rxx(uint8_t, 8)
mmio_rxx(uint16_t, 16)
mmio_rxx(uint32_t, 32)
mmio_rxx(uint64_t, 64)


#define mmio_wxx(t, x)									\
	static inline void mmio_w##x (uintptr_t a, t v)	{	\
		(*((volatile t *) (a))) = (v);					\
	}


mmio_wxx(uint8_t, 8)
mmio_wxx(uint16_t, 16)
mmio_wxx(uint32_t, 32)
mmio_wxx(uint64_t, 64)


#endif
