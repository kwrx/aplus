#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>



int main(int argc, char** argv) {
	if(kill(getppid(), SIGKILL) != 0)
		perror(argv[0]);
	
	return 0;
}