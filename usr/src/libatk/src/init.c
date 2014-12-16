#include <atk.h>
#include <stdint.h>
#include <assert.h>

#include "config.h"


void __atk_initialize_surface(SDL_Surface* s) {
	SDL_SetSurfaceBlendMode(
		s,
#if HAVE_ALPHABLEND
		SDL_BLENDMODE_BLEND
#else
		SDL_BLENDMODE_NONE
#endif
	);
}


void __atk_initialize_renderer(SDL_Renderer* r) {
	SDL_SetRenderDrawBlendMode(
		r,
#if HAVE_ALPHABLEND
		SDL_BLENDMODE_BLEND
#else
		SDL_BLENDMODE_NONE
#endif
	);
		
}


