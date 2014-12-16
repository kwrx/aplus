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

#define ATK_OUT_OF_MEMORY()		\
	ATK_ERROR("Out of memory")

#define ATK_FILE_NOT_FOUND()	\
	ATK_ERROR("File not found")


#define ATK_MASK_ARGB		\
	0x00FF0000,	/* R */		\
	0x0000FF00,	/* G */		\
	0x000000FF,	/* B */		\
	0xFF000000	/* A */


/* Newlib fix */
#define HAVE_UNISTD_IO		1
#define HAVE_ALPHABLEND		1



#endif
