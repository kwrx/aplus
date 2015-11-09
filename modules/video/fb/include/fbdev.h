#ifndef _FBDEV_H
#define _FBDEV_H


#include <libc.h>

typedef struct {
	char* name;
	
	int (*init) (void);
	int (*dnit) (void);

	int (*setvideomode) (uint16_t width, uint16_t height, uint16_t depth, uint16_t vx, uint16_t vy, void** lfbptr);
} fbdev_t;

#endif
