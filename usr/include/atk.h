#ifndef _ATK_H
#define _ATK_H

#ifndef __GNUC__
#error "Only GNU Compiler is supported"
#endif


#if 1
typedef int atk_v2si __attribute__ ((vector_size(sizeof(int) * 2)));
typedef int atk_v3si __attribute__ ((vector_size(sizeof(int) * 4)));
typedef int atk_v4si __attribute__ ((vector_size(sizeof(int) * 4)));

typedef float atk_v2f __attribute__ ((vector_size(sizeof(float) * 2)));
typedef float atk_v3f __attribute__ ((vector_size(sizeof(float) * 4)));
typedef float atk_v4f __attribute__ ((vector_size(sizeof(float) * 4)));
#else
typedef int atk_v2si[2];
typedef int atk_v3si[3];
typedef int atk_v4si[4];

typedef float atk_v2f[2];
typedef float atk_v3f[3];
typedef float atk_v4f[4];
#endif

typedef atk_v4f atk_color_t;
typedef atk_v4si atk_rect_t;
typedef atk_v2si atk_size_t;
typedef atk_v2si atk_pos_t;
typedef void atk_list_t;



#define ATK_COLOR_BLACK			{ 1.0f, 0.0f, 0.0f, 0.0f }
#define ATK_COLOR_WHITE			{ 1.0f, 1.0f, 1.0f, 1.0f }
#define ATK_COLOR_TRANSPARENT	{ 0.0f, 0.0f, 0.0f, 0.0f }

#define ATK_COLOR_A				0
#define ATK_COLOR_R				1
#define ATK_COLOR_G				2
#define ATK_COLOR_B				3

#define ATK_RECT_X				0
#define ATK_RECT_Y				1
#define ATK_RECT_W				2
#define ATK_RECT_H				3

#define ATK_SIZE_W				0
#define ATK_SIZE_H				1

#define ATK_POS_X				0
#define ATK_POS_Y				1


#define ATK_RECT_EMPTY			{ 0, 0, 0, 0 }
#define ATK_SIZE_EMPTY			{ 0, 0 }
#define ATK_POS_EMPTY			{ 0, 0 }


#include <atk/widget.h>
#include <atk/gfx.h>
#include <atk/bitmap.h>

#ifdef __cplusplus
extern "C" {
#endif

atk_gfx_t* atk_gfx_create(short width, short height, short bpp, void* buffer);
void atk_set_gfx(atk_gfx_t* gfx);
atk_gfx_t* atk_get_gfx();
void atk_gfx_set_color(atk_color_t color);
void atk_gfx_set_color_argb(float a, float r, float g, float b);
void atk_gfx_set_color_rgb(float r, float g, float b);
void atk_gfx_set_bitmap(atk_bitmap_t* bitmap);
int atk_gfx_clear();
int atk_gfx_hline(int x0, int y0, int x1);
int atk_gfx_vline(int x0, int y0, int y1);
int atk_gfx_line(int ix0, int iy0, int ix1, int iy1);
int atk_gfx_fill_rectangle(int x, int y, int w, int h);
int atk_gfx_stroke_rectangle(int x, int y, int w, int h);

#ifdef __cplusplus
}
#endif
#endif
