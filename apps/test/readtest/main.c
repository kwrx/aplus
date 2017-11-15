#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>


void* pt_read(void* arg) {
    int fd = (int) arg;
    
    char buf[64];
    while(read(fd, buf, sizeof(buf)))
        fprintf(stderr, "%d ", getpid());

    return NULL;
}

int main(int argc, char** argv) {
    int fd = open("/dev/sda", O_RDONLY);
    if(fd < 0) {
        perror("/dev/sda");
        return -1;
    }

    pthread_t pt1, pt2;
    pthread_create(&pt1, NULL, pt_read, (void*) fd);
    pthread_create(&pt2, NULL, pt_read, (void*) fd);
    pthread_join(pt1, NULL);
    pthread_join(pt2, NULL);

    close(fd);
    return 0;
}