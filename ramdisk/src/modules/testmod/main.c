#include <stdio.h>
#include <stdlib.h>
#include <math.h>


int main(int argc, char** argv) {
	printf("Hello World\n");
	
	for(;;)
		sched_yield();
}
