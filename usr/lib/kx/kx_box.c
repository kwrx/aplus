#include "kx_internal.h"



void kx_strokebox(int x, int y, int w, int h, pixel_t color) {
	if(_A(color) == 0)
		return;

	kx_hline(x, y, w, color);
	kx_hline(x, y + h, w, color);
	kx_vline(x, y, h, color);
	kx_vline(x + w, y, h, color);
}


void kx_fillbox(int x, int y, int w, int h, pixel_t color) {
	if(_A(color) == 0)
		return;

	while(h)
		kx_hline(x, y + h--, w, color);
}