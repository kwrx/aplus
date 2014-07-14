#ifndef VGUI_H
#define VGUI_H


#include <stdio.h>
#include <unistd.h>


#define VIDEOCONFIG_PATH	"/ramdisk/video.conf"

#define VGUI_DEFAULT_WIDTH	800
#define VGUI_DEFAULT_HEIGHT	600
#define VGUI_DEFAULT_BPP	16

#ifdef DEBUG
static inline void error(char* msg) {
	dprintf(STDERR_FILENO, "error: %s\n", msg);
	exit(-1);
}

static inline void warn(char* msg) {
	dprintf(STDERR_FILENO, "warning: %s\n", msg);
}


#define exit_on_error(res)	__exit_on_error(res, __FILE__, __LINE__)
static inline void __exit_on_error(int res, char* file, int line) {
	if(res != 0) {
		dprintf(STDERR_FILENO, "error: %s (%u) -> %d\n", file, line, res);
		exit(res);
	}
}

#define warn_on_error(res) __warn_on_error(res, __FILE__, __LINE__)
static inline void __warn_on_error(int res, char* file, int line) {
	if(res != 0)
		dprintf(STDERR_FILENO, "warning: %s (%u) -> %d\n", file, line, res);
}

#else
#define exit_on_error(res)	res
#define warn_on_error(res)	res
#define warn(m)				(void) 0
#define error(m)			(void) 0
#endif


struct videoconf_t {
	int width;
	int height;
	int bpp;
	void* framebuff;
} videoconf;

#endif
