#ifndef _ATK_FONT_H
#define _ATK_FONT_H

#include <atk.h>
#include <atk/bitmap.h>
#include <stdint.h>


typedef struct atk_font {
	void* data;
	void* face;
	void* library;

	uint32_t size;
	uint32_t dpi; 
} atk_font_t;


#endif
