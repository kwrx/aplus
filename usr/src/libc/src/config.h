#ifndef _CONFIG_H
#define _CONFIG_H

#define likely(x)		__builtin_expect(!!(x), 1)
#define unlikely(x)		__builtin_expect(!!(x), 0)


#ifdef HAVE_PTHREAD
#undef HAVE_PTHREAD
#endif

#define HAVE_PTHREAD				0
#define HAVE_DEV_RANDOM				0
#define HAVE_SSE					1
#define HAVE_SIGNALS				1


#define PATH_SEPARATOR				'/'

#endif
