#ifdef __rpi__

#include <aplus.h>
#include "rpi.h"


void arch_panic(char* msg, regs_t* r) {
	for(;;);
}

#endif
