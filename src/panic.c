#include <aplus.h>

/**
 *	\brief Go in Kernel Panic, halt system.
 */
void panic(char* msg) {
	arch_panic(msg, NULL);
}


