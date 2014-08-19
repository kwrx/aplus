#include "kx_internal.h"


void kx_plot(int x, int y, pixel_t color) {
	if(_A(color) == 0)
		return;

	if(_A(color) == 0xFF) {
		__kx->vbuf[x + y * __kx->width] = color;
		return;
	}

	pixel_t* dst = &__kx->vbuf[x + y * __kx->width];
	
	pixel_component_t pr = __alphablend(_R(*dst), _R(color), _A(color));
	pixel_component_t pg = __alphablend(_G(*dst), _G(color), _A(color));
	pixel_component_t pb = __alphablend(_B(*dst), _B(color), _A(color));

	*dst = 	((0xFF << 24)	|
			 (pr << 16)		|
			 (pg << 8)		|
			 (pb << 0));
}