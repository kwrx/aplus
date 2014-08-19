#ifndef _KX_H
#define _KX_H

#define KX_BITSPERPIXEL			32
#define KX_BYTESPERPIXEL		4


typedef unsigned int pixel_t;
typedef unsigned char pixel_component_t;

typedef struct {
	int width;
	int height;
	int stride;
	pixel_t* vbuf;
} kx_context_t;


/* kx_context */
kx_context_t* kx_createcontext(int width, int height, void* framebuf);
void kx_setcontext(kx_context_t* kx);
kx_context_t* kx_getcontext();


/* kx_box */
void kx_strokebox(int x, int y, int w, int h, pixel_t color);
void kx_fillbox(int x, int y, int w, int h, pixel_t color);

/* kx_line */
void kx_hline(int x, int y, int w, pixel_t color);
void kx_vline(int x, int y, int h, pixel_t color);
void kx_line(int x1, int y1, int x2, int y2, pixel_t color);



#endif
