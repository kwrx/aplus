#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/ipc.h>
#include <libc.h>
#include "arm.h"



#if DEBUG
void debug_send(char value, int flags) {
	serial_send(0, value);
}
#endif


