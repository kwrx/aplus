#include <atk.h>
#include <stdint.h>

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

	if(bpp != 32)
		 ATK_ERROR("Only 32Bits is supported for now");


	if(!SDL_WasInit(SDL_INIT_VIDEO))
		if(SDL_Init(SDL_INIT_VIDEO) != 0)
			ATK_ERROR("SDL_Init failed");


#if HAVE_SDL_TTF
	if(!TTF_WasInit())
		if(TTF_Init() != 0)
			ATK_ERROR("TTF_Init failed");
#endif


#if HAVE_SDL_IMAGE
	if(IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG)
		ATK_ERROR("IMG_Init failed");
#endif

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

void atk_exit(void) {
#if HAVE_SDL_TTF
	TTF_Quit();
#endif
	SDL_Quit();
	exit(0);
}


int atk_main(atk_t* atk) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	char* pw = NULL;
	char* ph = NULL;
	char* pb = NULL;

	int w = ATK_SCREEN_WIDTH;
	int h = ATK_SCREEN_HEIGHT;
	int b = ATK_SCREEN_BPP;

	pw = getenv("SCREEN_WIDTH");
	ph = getenv("SCREEN_HEIGHT");
	pb = getenv("SCREEN_BPP");

	if(pw)
		w = atoi(pw);

	if(ph)
		h = atoi(ph);

	if(pb)
		b = atoi(pb);

	ATK_LOG("Mode %dx%dx%d\n", w, h, b);

	atexit(atk_exit);
	return atk_init(atk, w, h, b, (void*) ATK_SCREEN_BUFFER);
}
