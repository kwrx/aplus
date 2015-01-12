#ifndef _ATK_WINDOW_H
#define _ATK_WINDOW_H

#include <atk.h>


#define ATK_WINDOW_FLAGS_HIDDEN			1
#define ATK_WINDOW_FLAGS_BORDERLESS		2
#define ATK_WINDOW_FLAGS_RESIZABLE		4
#define ATK_WINDOW_FLAGS_MINIMIZED		8
#define ATK_WINDOW_FLAGS_MAXIMIZED		16


										/* R, G, B, A */
#define ATK_WINDOW_COLOR_BACKGROUND		0xFF, 0xFF, 0xFF, 0xFF
#define ATK_WINDOW_COLOR_FOREGROUND		0x00, 0x00, 0x00, 0xFF
#define ATK_WINDOW_COLOR_TOPBAR			0xEE, 0xEE, 0xEE, 0xFF
#define ATK_WINDOW_COLOR_TITLE			0x00, 0x00, 0x00, 0xFF
#define ATK_WINDOW_COLOR_BORDER			0x00, 0x00, 0x00, 0xFF

#define ATK_WINDOW_FONT_INDEX			0

#define ATK_WINDOW_PADDING_TOPBAR_X		3
#define ATK_WINDOW_PADDING_TOPBAR_Y		3



typedef struct {
	atk_widget_t ctx;

	/* &ctx.surface */
	atk_image_t* surface;

	/* &ctx.renderer */
	atk_render_t* renderer;
	
	char title[256];
	int flags;
} atk_window_t;

#endif
