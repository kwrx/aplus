#include <atk.h>
#include <stdint.h>
#include <assert.h>

#include "config.h"


#if HAVE_UNISTD_IO
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif


int atk_load_image(atk_t* atk, atk_image_t** image, const char* path) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!image)
		ATK_ERROR("Invalid Image context");

	if(!path)
		ATK_ERROR("Path cannot be null");


#if HAVE_UNISTD_IO
	SDL_RWops* rw = (SDL_RWops*) __RW_FromUnistd(path, O_RDONLY, 0644);
#else
	SDL_RWops* rw = SDL_RWFromFile(path, "rb");
#endif

#if HAVE_SDL_IMAGE
	atk_image_t* tmp = (atk_image_t*) IMG_Load_RW(rw, 1);
#else
	atk_image_t* tmp = (atk_image_t*) SDL_LoadBMP_RW(rw, 1);
#endif


	*image = (atk_image_t*) SDL_ConvertSurface((SDL_Surface*) tmp, atk->surface->format, 0);
	SDL_FreeSurface((SDL_Surface*) tmp);

	if(*image == NULL)
		ATK_ERROR("Loading and convert image failed");

	__atk_initialize_surface(*image);
	return 0;
}

int atk_save_image(atk_t* atk, atk_image_t* image, const char* path) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!image)
		ATK_ERROR("Invalid Image context");

	if(!path)
		ATK_ERROR("Path cannot be null");

#if HAVE_UNISTD_IO
	SDL_RWops* rw = (SDL_RWops*) __RW_FromUnistd(path, O_CREAT | O_TRUNC | O_RDWR, S_IFREG);
#else
	SDL_RWops* rw = SDL_RWFromFile(path, "wb");
#endif

	return SDL_SaveBMP_RW((SDL_Surface*) image, rw, 1);
}

