#ifndef _FBDEV_H
#define _FBDEV_H

#include <aplus.h>
#include <aplus/fb.h>

typedef struct {
	char* name;	
	int hw_isavail;
	
	int (*init) (void);
	int (*dnit) (void);

	void (*hw_fillrect) (fb_rect_t* rect);
	void (*hw_copyrect) (fb_rect_t** rects);
	void (*hw_blit) (fb_image_t* image);

	void (*hw_surface_alloc) (fb_surface_t* surface);
	void (*hw_surface_destroy) (fb_surface_t* surface);

	int (*setvideomode) (fb_videomode_t* vm);
} fbdev_gfx_t;

#endif
