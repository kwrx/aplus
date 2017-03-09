#ifndef _APLUS_INPUT_H
#define _APLUS_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct mouse {
	uint8_t buttons[5];
	uint16_t speed;

	uint16_t x;
	uint16_t y;
	uint16_t z;

	int16_t dx;
	int16_t dy;
	int16_t dz;

	struct {
		uint16_t left;
		uint16_t top;
		uint16_t right;
		uint16_t bottom;
	} clip;

	uint8_t pack[4];
	uint8_t cycle;
} __attribute__((packed)) mouse_t;



#define VK_CAPSLOCK		(0x3A)
#define VK_NUMLOCK		(0x45)
#define VK_SCORRLOCK	(0x46)
#define VK_LSHIFT		(0x2A)
#define VK_RSHIFT		(0x36)
#define VK_LCTRL		(0x1D)
#define VK_RCTRL		(0x1D + 128)
#define VK_LALT			(0x38)
#define VK_RALT			(0x38 + 128)


#ifdef __cplusplus
}
#endif

#endif
