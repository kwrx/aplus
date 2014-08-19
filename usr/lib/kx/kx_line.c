#include "kx_internal.h"


void kx_hline(int x, int y, int w, pixel_t color) {
	if(_A(color) == 0)
		return;

	if(_A(color) == 0xFF)
		return memset_px(&__kx->vbuf[x + y * __kx->width], color, w);

	for(int i = 0; i < w; i++)
		kx_plot(x + i, y, color);
}


void kx_vline(int x, int y, int h, pixel_t color) {
	if(_A(color) == 0)
		return;

	for(int i = 0; i < h; i++)
		kx_plot(x, y + i, color);
}


void kx_line(int x1, int y1, int x2, int y2, pixel_t color) {
	if(_A(color) == 0)
		return;

	if(x1 == x2)
		return kx_vline(x1, y1, (y2 - y1), color);

	if(y1 == y2)
		return kx_hline(x1, y1, (x2 - x1), color);

	__check_and_swap(x1, x2);
	__check_and_swap(y1, y2);

	int dx = x2 - x1;
	int dy = y2 - y1;
	for(int x = x1; x < x2; x++)
		kx_plot(x, y1 + dy * (x - x1) / dx, color);
}