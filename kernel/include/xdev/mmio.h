#ifndef _MMIO_H
#define _MMIO_H

#ifndef __ASSEMBLY__

#define mmio_r8(x)		(*(volatile uint8_t*) (x))
#define mmio_r16(x)		(*(volatile uint16_t*) (x))
#define mmio_r32(x)		(*(volatile uint32_t*) (x))
#define mmio_r64(x)		(*(volatile uint64_t*) (x))

#define mmio_w8(x, y)		{ mmio_r8(x) = (y); }
#define mmio_w16(x, y)		{ mmio_r16(x) = (y); }
#define mmio_w32(x, y)		{ mmio_r32(x) = (y); }
#define mmio_w64(x, y)		{ mmio_r64(x) = (y); }

#endif

#endif
