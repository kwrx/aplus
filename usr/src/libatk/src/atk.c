#include <atk.h>
#include <stdint.h>
#include <assert.h>

#define __ATK_ERROR
#include "config.h"

char* __atk_error = NULL;


char* atk_error() {
	return __atk_error;
}

int atk_init(atk_t* atk, int width, int height, int bpp, void* framebuffer) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(width < 0 || height < 0)
		ATK_ERROR("Invalid size of screen");

	if(framebuffer == NULL)
		ATK_ERROR("Framebuffer pointer can't be null");


	assert(bpp == 32 && "Only 32Bits is supported");

	memset(atk, 0, sizeof(atk_t));
	atk->surface = SDL_CreateRGBSurfaceFrom(
		framebuffer,
		width,
		height,
		bpp,
		width * (bpp / 8),
		ATK_MASK_ARGB
	);

	atk->renderer = SDL_CreateSoftwareRenderer(atk->surface);
	atk->window.x = 0;
	atk->window.y = 0;
	atk->window.w = width;
	atk->window.h = height;

	__atk_initialize_surface(atk->surface);
	__atk_initialize_renderer(atk->renderer);

	return 0;
}

int atk_destroy(atk_t* atk) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	SDL_DestroyRenderer(atk->renderer);
	SDL_FreeSurface(atk->surface);

	memset(atk, 0, sizeof(atk_t));
	return 0;
}
