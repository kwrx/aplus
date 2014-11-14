#ifndef _ATK_H
#define _ATK_H

#ifndef __GNUC__
#error "Only GNU Compiler is supported"
#endif


typedef int atk_v2si __attribute__ ((vector_size(sizeof(int) * 2)));
typedef int atk_v3si __attribute__ ((vector_size(sizeof(int) * 3)));
typedef int atk_v4si __attribute__ ((vector_size(sizeof(int) * 4)));

typedef float atk_v2f __attribute__ ((vector_size(sizeof(float) * 2)));
typedef float atk_v3f __attribute__ ((vector_size(sizeof(float) * 3)));
typedef float atk_v4f __attribute__ ((vector_size(sizeof(float) * 4)));

typedef atk_v4f atk_color_t;
typedef void atk_list_t;



#define ATK_COLOR_BLACK			{ 1.0f, 0.0f, 0.0f, 0.0f }
#define ATK_COLOR_WHITE			{ 1.0f, 1.0f, 1.0f, 1.0f }
#define ATK_COLOR_TRANSPARENT	{ 0.0f, 0.0f, 0.0f, 0.0f }

#define ATK_COLOR_A				3
#define ATK_COLOR_R				2
#define ATK_COLOR_G				1
#define ATK_COLOR_B				0


#include <atk/widget.h>

#endif
