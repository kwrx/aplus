#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>


static int new_thread(void* arg) {
    pthread_exit(0);
}

int main(int argc, char** argv) {
    pthread_t p;
    pthread_create(&p, NULL, new_thread, NULL);
    pthread_join(p, NULL);

}