#ifndef _CONFIG_H
#define _CONFIG_H

#ifndef __ATK_ERROR
extern char* __atk_error;
#endif


#define ATK_ERROR(s)			\
	{							\
		__atk_error = s;		\
		return -1;				\
	}

#define ATK_ERROR_N(s)			\
	{							\
		__atk_error = s;		\
		return NULL;			\
	}


#define ATK_LOG(s, a...)		\
	printf("atk: " s, a)

#define ATK_OUT_OF_MEMORY()		\
	ATK_ERROR("Out of memory")

#define ATK_OUT_OF_MEMORY_N()	\
	ATK_ERROR_N("Out of memory")

#define ATK_FILE_NOT_FOUND()	\
	ATK_ERROR("File not found")

#define ATK_FILE_NOT_FOUND_N()	\
	ATK_ERROR_N("File not found")


#define ATK_MASK_ARGB		\
	0x00FF0000,	/* R */		\
	0x0000FF00,	/* G */		\
	0x000000FF,	/* B */		\
	0xFF000000	/* A */


/* Newlib stdio fix */
#define HAVE_UNISTD_IO		1
#define HAVE_ALPHABLEND		1
#define HAVE_SDL_TTF		1
#define HAVE_SDL_IMAGE		1
#define HAVE_DEBUG			1


#define ATK_SCREEN_WIDTH		800
#define ATK_SCREEN_HEIGHT		600
#define ATK_SCREEN_BPP			32

/* FIXME */
#define ATK_SCREEN_BUFFER		0xFD000000


#if !HAVE_DEBUG
#undef ATK_LOG
#define ATK_LOG(x)
#endif


#if HAVE_SDL_TTF
#define FONT_PATH		"/dev/ramdisk/share/fonts/"
#endif

#endif
