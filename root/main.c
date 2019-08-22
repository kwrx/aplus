#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>


//__thread int key;
//__thread int obj = 32;
//__thread int arr[64];

int main(int argc, char** argv) {
for(;;);
    FILE* fp = fopen("/dev/kmsg", "w+");
    if(!fp)
        for(;;);

    fprintf(fp, "Hello World!\n");
    fclose(fp);

    for(;;);
}
