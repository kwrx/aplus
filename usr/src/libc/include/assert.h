#ifndef _ASSERT_H
#define _ASSERT_H

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


#undef assert
#ifdef NDEBUG
#define assert()		((void) 0)
#else
#define assert(c)											\
	if(!(c)) {												\
		printf("assertion \"%s\" failed in %s:%d (%s)\n",	\
			#c, __FILE__, __LINE__, __func__				\
		); exit(-1);										\
	}
#endif


#define static_assert _Static_assert


#ifdef __cplusplus
}
#endif
#endif
