#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>


int main(int argc, char** argv) {

    FILE* fp = fopen("/dev/kmsg", "r+");
    if(!fp)
        for(;;);

    fprintf(fp, "Hello World\n");
    fclose(fp);

    for(;;);
}
