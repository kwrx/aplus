#ifndef _FBDEV_H
#define _FBDEV_H

#include <stdint.h>

typedef struct {
	uint16_t width;
	uint16_t height;
	uint16_t bpp;
	uint16_t vx;
	uint16_t vy;
	void* lfbptr;
} fbdev_mode_t;

typedef struct {
	uint16_t x;
	uint16_t y;
	uint16_t stride;
	uint16_t height;
	void* ptr;
} fbdev_surface_t;

typedef struct fb_window {
	fbdev_surface_t surface;
	
	struct fbdev_window* next;
	struct fbdev_window* parent;
	struct fbdev_window* childrens;
} fbdev_window_t;



typedef struct {
	char* name;
	int enabled;
	
	int (*init) (void);
	int (*dnit) (void);

	int (*setvideomode) (fbdev_mode_t* mode);
	int (*getvideomode) (fbdev_mode_t* mode);
	int (*update_surface) (fbdev_surface_t* surface);
} fbdev_t;




#define FBIOCTL_SETMODE				10
#define FBIOCTL_GETMODE				11
#define FBIOCTL_UPDATESURFACE		12

#endif
