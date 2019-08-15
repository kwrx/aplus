#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>


//__thread int key;
//__thread int obj = 32;
//__thread int arr[64];

int _start(void* unused) {
    __asm__("movl $404, %eax; int $0xFE"); 
    __asm__("movl $404, %eax; int $0xFE");
   
    for(;;);
}
