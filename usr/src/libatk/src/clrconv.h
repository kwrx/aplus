#ifndef _CLRCONV_H
#define _CLRCONV_H

#include <atk.h>
#include <stdint.h>

/*
 *
 *	V4F:	atk_color_t cl = { 1.0f, 1.0f, 1.0f, 1.0f };
 *	ARGB:	atk_color_t cl = 0xFFFFFFFF;
 *
 */

#define V4F_TO_ARGB(c)										\
	(														\
		((int32_t)(c[ATK_COLOR_A] * 0xFF) << 24) 	|		\
		((int32_t)(c[ATK_COLOR_R] * 0xFF) << 16) 	|		\
		((int32_t)(c[ATK_COLOR_G] * 0xFF) << 8) 	|		\
		((int32_t)(c[ATK_COLOR_B] * 0xFF)) 					\
	)

#define V4F_TO_RGB(c)										\
	(														\
		((int32_t)(c[ATK_COLOR_R] * 0xFF) << 16) 	|		\
		((int32_t)(c[ATK_COLOR_G] * 0xFF) << 8) 	|		\
		((int32_t)(c[ATK_COLOR_B] * 0xFF)) 					\
	)

#define V4F_TO_R5G6B5(c)									\
	(														\
		((int16_t)(c[ATK_COLOR_R] * 0x1F) << 11)	|		\
		((int16_t)(c[ATK_COLOR_G] * 0x3F) << 5)		|		\
		((int16_t)(c[ATK_COLOR_B] * 0x1F))					\
	)


#define ARGB_TO_V4F(c)							\
	{											\
		((float) ((c >> 24) & 0xFF)) / 0xFF, 	\
		((float) ((c >> 16) & 0xFF)) / 0xFF,	\
		((float) ((c >> 8) & 0xFF)) / 0xFF,		\
		((float) (c & 0xFF)) / 0xFF				\
	}

#define RGB_TO_V4F(c)							\
	{											\
		1.0f, 									\
		((float) ((c >> 16) & 0xFF)) / 0xFF,	\
		((float) ((c >> 8) & 0xFF)) / 0xFF,		\
		((float) (c & 0xFF)) / 0xFF				\
	}

#define R5G6B5_TO_V4F(c)						\
	{											\
		1.0f, 									\
		((float) ((c >> 11) & 0x1F)) / 0x1F,	\
		((float) ((c >> 5) & 0x3F)) / 0x3F,		\
		((float) (c & 0x1F)) / 0x1F				\
	}




static inline atk_color_t __alphablend(atk_color_t d, atk_color_t s, float a) {
	if(a == 0.0f)
		return d;

	if(a == 1.0f)
		return s;

	float ia = 1.0f - a;
	
	atk_color_t r = ATK_COLOR_BLACK;
	r[ATK_COLOR_A] = 1.0f;
	r[ATK_COLOR_R] = (s[ATK_COLOR_R] * a + d[ATK_COLOR_R] * ia);
	r[ATK_COLOR_G] = (s[ATK_COLOR_G] * a + d[ATK_COLOR_G] * ia);
	r[ATK_COLOR_B] = (s[ATK_COLOR_B] * a + d[ATK_COLOR_B] * ia);
	

	return r;
}


#define __SWAP(x, y)		\
	{						\
		x ^= y;				\
		y ^= x;				\
		x ^= y;				\
	}


#endif
