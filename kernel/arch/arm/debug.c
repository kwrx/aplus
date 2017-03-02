#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <libc.h>
#include "arm.h"



#if DEBUG
void debug_send(char value, int flags) {
	serial_send(0, value);
}
#endif


