#ifndef _CONFIG_H
#define _CONFIG_h

#include <stddef.h>

#define TEST

#ifdef TEST
#define DEBUG
#include <assert.h>
#endif

#include <stdlib.h>
#define dl_malloc(x)			malloc(x)
#define dl_free(x)				free(x)


#include <string.h>
#define dl_memcpy(a, b, c)		memcpy(a, b, c)
#define dl_memmove(a, b, c)		memmove(a, b, c)
#define dl_memset(a, b, c)		memset(a, b, c)
#define dl_strcmp(a, b)			strcmp(a, b)
#define dl_strcpy(a, b)			strcpy(a, b)


#ifdef DEBUG
#include <stdio.h>
#define dl_printf(a, b...)		printf(a, b)
#endif


#include <unistd.h>
#include <fcntl.h>
#define dl_open(a, b, c)		open(a, b, c)
#define dl_close(a)				close(a)
#define dl_read(a, b, c)		read(a, b, c)
#define dl_seek(a, b, c)		lseek(a, b, c)

#include <errno.h>




#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)



#endif
