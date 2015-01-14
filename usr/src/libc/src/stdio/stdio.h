#ifndef _LIBC_STDIO_H
#define _LIBC_STDIO_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include "../config.h"

#ifndef O_BINARY
#define O_BINARY		0
#endif


#define STDIO_FILE(fp)		((__STDIO_FILE*) fp)

#if HAVE_PTHREAD
#include <pthread.h>

#define STDIO_INIT_LOCK(x)	pthread_mutex_init(&STDIO_FILE(x)->lock, NULL)
#define STDIO_DNIT_LOCK(x)	pthread_mutex_destroy(&STDIO_FILE(x)->lock)
#define STDIO_LOCK(x)		pthread_mutex_lock(&STDIO_FILE(x)->lock)
#define STDIO_TRYLOCK(x)	pthread_mutex_trylock(&STDIO_FILE(x)->lock)
#define STDIO_UNLOCK(x)		pthread_mutex_unlock(&STDIO_FILE(x)->lock)

typedef pthread_mutex_t stdio_lock_t;

#else
#define STDIO_INIT_LOCK(x)	((void) x)
#define STDIO_DNIT_LOCK(x)	((void) x)
#define STDIO_LOCK(x)		((void) x)
#define STDIO_TRYLOCK(x)	((void) x)
#define STDIO_UNLOCK(x)		((void) x)

typedef int stdio_lock_t;

#endif


#define STDIO_CAN_READ(fp) (													\
		STDIO_FILE(fp)->eof != EOF && 											\
		(STDIO_FILE(fp)->flags & O_RDONLY || STDIO_FILE(fp)->flags & O_RDWR) &&	\
		STDIO_FILE(fp)->read													\
	)

#define STDIO_CAN_WRITE(fp)	(													\
		STDIO_FILE(fp)->eof != EOF &&											\
		(STDIO_FILE(fp)->flags & O_WRONLY || STDIO_FILE(fp)->flags & O_RDWR) &&	\
		STDIO_FILE(fp)->write													\
	)



#define STDIO_ERROR(fp, e)						\
	do {										\
		STDIO_FILE(fp)->error = e;				\
		errno = e;								\
		return -1;								\
	} while(0)

typedef struct __STDIO_FILE {
	int fd;
	int flags;
	int eof;
	int error;
	void* data;
	stdio_lock_t lock;

	size_t (*read) (struct __STDIO_FILE*, void*, size_t);
	size_t (*write) (struct __STDIO_FILE*, const void*, size_t);
	int (*close) (struct __STDIO_FILE*);
	off_t (*seek) (struct __STDIO_FILE*, off_t, int);
} __STDIO_FILE;




#endif
