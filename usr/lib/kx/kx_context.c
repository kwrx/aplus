#include "kx_internal.h"

kx_context_t* __kx;


kx_context_t* kx_createcontext(int width, int height, void* framebuf) {
	kx_context_t* kx = malloc(sizeof(kx_context_t*));
	kx->width = width;
	kx->height = height;
	kx->stride = KX_BYTESPERPIXEL * width;
	kx->vbuf = framebuf;

	return kx;	
}

void kx_setcontext(kx_context_t* kx) {
	__kx = kx;
}

kx_context_t* kx_getcontext() {
	return __kx;
}

