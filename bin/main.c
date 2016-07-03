#include <stdio.h>
#include <signal.h>
#include <unistd.h>


int main(int argc, char** argv, char** env) {
	FILE* fd = fopen("/cdrom/main.c", "r");
	if(!fd)
		perror("file");
		
	char* buf = (char*) malloc(0x1000);
	int r = fread(buf, 1000, 1, fd);
	fclose(fd);
	printf("Bytes readed: %d\n%s", r, buf);
	
	return 0;
}