#ifdef __rpi__

#include <aplus.h>


int LogPrint(const char* str) {
	return kprintf("%s", str);
}


#endif
