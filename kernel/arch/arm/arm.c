#include <aplus.h>
#include "arm.h"


void arm_init() {
    serial_init();
    lfb_init();
    atags_init();
}


void arch_loop_idle() {
    for(;;)
        ;
}