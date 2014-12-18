#include <atk.h>
#include <stdint.h>
#include <assert.h>

#include "config.h"


#if HAVE_UNISTD_IO
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif



int atk_open_font(atk_t* atk, atk_font_t** font, const char* path, int size) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!font)
		ATK_ERROR("Invalid Font context");

	if(!path)
		ATK_ERROR("Path cannot be null");

	if(!size)
		ATK_ERROR("Invalid size of font");

	if(!HAVE_SDL_TTF)
		ATK_ERROR("TTF Font not supported for this build");

#if HAVE_SDL_TTF
#if HAVE_UNISTD_IO
	SDL_RWops* rw = (SDL_RWops*) __RW_FromUnistd(path, O_RDONLY, 0644);
#else
	SDL_RWops* rw = SDL_RWFromFile(path, "rb");
#endif

	*font = (atk_font_t*) TTF_OpenFontRW(rw, 1, size);

	if(*font == NULL)
		ATK_ERROR("Could not load TTF font");

	return 0;
#endif
}

int atk_close_font(atk_t* atk, atk_font_t* font) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!font)
		ATK_ERROR("Invalid Font context");

	if(!HAVE_SDL_TTF)
		ATK_ERROR("TTF Font not supported for this build");

#if HAVE_SDL_TTF
	TTF_CloseFont((TTF_Font*) font);
	return 0;
#endif
}

int atk_get_font_height(atk_t* atk, atk_font_t* font, int* height) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!font)
		ATK_ERROR("Invalid Font context");

	if(!height)
		ATK_ERROR("Height Pointer cannot be null");

	if(!HAVE_SDL_TTF)
		ATK_ERROR("TTF Font not supported for this build");

#if HAVE_SDL_TTF
	*height = TTF_FontHeight((TTF_Font*) font);
	return 0;
#endif
}


int atk_set_font_style(atk_t* atk, atk_font_t* font, int style) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!font)
		ATK_ERROR("Invalid Font context");

	if(!HAVE_SDL_TTF)
		ATK_ERROR("TTF Font not supported for this build");

#if HAVE_SDL_TTF
	TTF_SetFontStyle((TTF_Font*) font, style);
	return 0;
#endif
}

int atk_get_font_style(atk_t* atk, atk_font_t* font, int* style) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!font)
		ATK_ERROR("Invalid Font context");

	if(!style)
		ATK_ERROR("Style Pointer cannot be null");

	if(!HAVE_SDL_TTF)
		ATK_ERROR("TTF Font not supported for this build");

#if HAVE_SDL_TTF
	*style = TTF_GetFontStyle((TTF_Font*) font);
	return 0;
#endif
}


int atk_get_font_lineskip(atk_t* atk, atk_font_t* font, int* lineskip) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!font)
		ATK_ERROR("Invalid Font context");

	if(!lineskip)
		ATK_ERROR("LineSkip Pointer cannot be null");

	if(!HAVE_SDL_TTF)
		ATK_ERROR("TTF Font not supported for this build");

#if HAVE_SDL_TTF
	*lineskip = TTF_FontLineSkip((TTF_Font*) font);
	return 0;
#endif
}

char* atk_get_font_stylename(atk_t* atk, atk_font_t* font) {
	if(!atk)
		ATK_ERROR_N("Invalid ATK context");

	if(!font)
		ATK_ERROR_N("Invalid Font context");

	if(!HAVE_SDL_TTF)
		ATK_ERROR_N("TTF Font not supported for this build");

#if HAVE_SDL_TTF
	return TTF_FontFaceStyleName((TTF_Font*) font);
#endif
}

char* atk_get_font_familyname(atk_t* atk, atk_font_t* font) {
	if(!atk)
		ATK_ERROR_N("Invalid ATK context");

	if(!font)
		ATK_ERROR_N("Invalid Font context");

	if(!HAVE_SDL_TTF)
		ATK_ERROR_N("TTF Font not supported for this build");

#if HAVE_SDL_TTF
	return TTF_FontFaceFamilyName((TTF_Font*) font);
#endif
}

int atk_get_font_sizetext(atk_t* atk, atk_font_t* font, const char* text, int* w, int* h) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!font)
		ATK_ERROR("Invalid Font context");

	if(!text)
		ATK_ERROR("Text pointer cannot be null");

	if(!HAVE_SDL_TTF)
		ATK_ERROR("TTF Font not supported for this build");

#if HAVE_SDL_TTF
	if(TTF_SizeText((TTF_Font*) font, text, w, h) != 0)
		ATK_ERROR("Could not get size of text for this font");

	return 0;
#endif
}


int atk_render_font(
	atk_t* atk, 
	atk_font_t* font, 
	atk_image_t* dest, 
	const char* text, 
	int x, 
	int y,
	atk_color_t color,
	int encoding) {
	
	
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!font)
		ATK_ERROR("Invalid Font context");

	if(!text)
		ATK_ERROR("Text pointer cannot be null");

	if(!dest)
		ATK_ERROR("Destination Image cannot be null");

	if(!HAVE_SDL_TTF)
		ATK_ERROR("TTF Font not supported for this build");

#if HAVE_SDL_TTF

	SDL_Surface* (*handler) (TTF_Font* font, void* text, SDL_Color fg) = NULL;

	#define ___C(a)	\
		((SDL_Surface* (*) (TTF_Font*, void*, SDL_Color)) a)

	switch(encoding) {
		case ATK_TTF_ENCODING_ASCII:
			handler = ___C(TTF_RenderText_Blended);
			break;
		case ATK_TTF_ENCODING_UTF8:
			handler = ___C(TTF_RenderUTF8_Blended);
			break;
		case ATK_TTF_ENCODING_UNICODE:
			handler = ___C(TTF_RenderUNICODE_Blended);
			break;
		default:
			ATK_ERROR("Invalid encoding for this text");
	}

	#undef ___C

	if(handler == NULL)
		ATK_ERROR("Invalid handler (Bug?)");

	SDL_Surface* surface = handler((TTF_Font*) font, (void*) text, (SDL_Color) color);
	if(surface == NULL)
		ATK_ERROR("Cannot render text on surface");


	SDL_Rect R;
	R.x = x;
	R.y = y;
	R.w = 0;
	R.h = 0;
	if(SDL_BlitSurface(surface, NULL, (SDL_Surface*) dest, &R) != 0)
		ATK_ERROR("Cannot blit text to surface");

	SDL_FreeSurface(surface);
	return 0;
#endif
}

int atk_render_font_fast(
	atk_t* atk, 
	atk_font_t* font, 
	atk_image_t* dest, 
	const char* text, 
	int x, 
	int y,
	atk_color_t color,
	int encoding) {
	
	
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!font)
		ATK_ERROR("Invalid Font context");

	if(!text)
		ATK_ERROR("Text pointer cannot be null");

	if(!dest)
		ATK_ERROR("Destination Image cannot be null");

	if(!HAVE_SDL_TTF)
		ATK_ERROR("TTF Font not supported for this build");

#if HAVE_SDL_TTF

	SDL_Surface* (*handler) (TTF_Font* font, void* text, SDL_Color fg) = NULL;

	#define ___C(a)	\
		((SDL_Surface* (*) (TTF_Font*, void*, SDL_Color)) a)

	switch(encoding) {
		case ATK_TTF_ENCODING_ASCII:
			handler = ___C(TTF_RenderText_Solid);
			break;
		case ATK_TTF_ENCODING_UTF8:
			handler = ___C(TTF_RenderUTF8_Solid);
			break;
		case ATK_TTF_ENCODING_UNICODE:
			handler = ___C(TTF_RenderUNICODE_Solid);
			break;
		default:
			ATK_ERROR("Invalid encoding for this text");
	}

	#undef ___C

	if(handler == NULL)
		ATK_ERROR("Invalid handler (Bug?)");

	SDL_Surface* surface = handler((TTF_Font*) font, (void*) text, (SDL_Color) color);
	if(surface == NULL)
		ATK_ERROR("Cannot render text on surface");

	SDL_Rect R;
	R.x = x;
	R.y = y;
	R.w = 0;
	R.h = 0;
	if(SDL_BlitSurface(surface, NULL, (SDL_Surface*) dest, &R) != 0)
		ATK_ERROR("Cannot blit text to surface");

	SDL_FreeSurface(surface);
	return 0;
#endif
}

int atk_render_font_shaded(
	atk_t* atk, 
	atk_font_t* font, 
	atk_image_t* dest, 
	const char* text, 
	int x, 
	int y,
	atk_color_t color,
	atk_color_t background,
	int encoding) {
	
	
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!font)
		ATK_ERROR("Invalid Font context");

	if(!text)
		ATK_ERROR("Text pointer cannot be null");

	if(!dest)
		ATK_ERROR("Destination Image cannot be null");

	if(!HAVE_SDL_TTF)
		ATK_ERROR("TTF Font not supported for this build");

#if HAVE_SDL_TTF

	SDL_Surface* (*handler) (TTF_Font* font, void* text, SDL_Color fg, SDL_Color bg) = NULL;

	#define ___C(a)	\
		((SDL_Surface* (*) (TTF_Font*, void*, SDL_Color, SDL_Color)) a)

	switch(encoding) {
		case ATK_TTF_ENCODING_ASCII:
			handler = ___C(TTF_RenderText_Shaded);
			break;
		case ATK_TTF_ENCODING_UTF8:
			handler = ___C(TTF_RenderUTF8_Shaded);
			break;
		case ATK_TTF_ENCODING_UNICODE:
			handler = ___C(TTF_RenderUNICODE_Shaded);
			break;
		default:
			ATK_ERROR("Invalid encoding for this text");
	}

	#undef ___C

	if(handler == NULL)
		ATK_ERROR("Invalid handler (Bug?)");

	SDL_Surface* surface = handler((TTF_Font*) font, (void*) text, (SDL_Color) color, (SDL_Color) background);
	if(surface == NULL)
		ATK_ERROR("Cannot render text on surface");

	SDL_Rect R;
	R.x = x;
	R.y = y;
	R.w = 0;
	R.h = 0;
	if(SDL_BlitSurface(surface, NULL, (SDL_Surface*) dest, &R) != 0)
		ATK_ERROR("Cannot blit text to surface");

	SDL_FreeSurface(surface);
	return 0;
#endif
}
