#ifndef _DEBUG_H
#define _DEBUG_H

#include <xdev.h>
#include <libc.h>



#define LOG					0
#define WARN				1
#define INFO				2
#define ERROR				3

#ifndef __ASSEMBLY__
void debug_send(char value, int flags);


#if !DEBUG
#define debug_send(a)
#define KASSERT(x)
#define kprintf(f, a, b...)
#else

int kprintf(int flags, const char* fmt, ...);
int std_kprintf(const char* fmt, ...);

#define KASSERT(x)																											\
	if(unlikely(!(x)))																										\
		{ kprintf(ERROR, "%s(): Assertion \"%s\" failed in %s:%d\n", __func__, #x, __FILE__, __LINE__); for(;;); }


#endif



#endif







#endif
