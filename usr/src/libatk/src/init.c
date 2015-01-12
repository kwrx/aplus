#include <atk.h>
#include <stdint.h>
#include <assert.h>

#include "config.h"


#if HAVE_SDL_TTF
atk_font_t* __font_cache[32];
#endif


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


#if HAVE_SDL_TTF
void __atk_initialize_fonts(atk_t* atk) {
	atk_open_font(atk, &__font_cache[0], FONT_PATH "arial.ttf", 12);
}

void __atk_finalize_fonts(atk_t* atk) {
	int i;
	for(i = 0; i < 32; i++)
		if(__font_cache[i])
			atk_close_font(atk, __font_cache[i]);
}
#endif


