#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <aplus/msg.h>



int main(int argc, char** argv) {
    char buf[32];
    pid_t pid;

    int e = msg_send(getpid(), "Hello World\0", 12);
    int d = msg_recv(&pid, buf, 32);

    printf("Sent %d bytes to %d\nReceived %d bytes from %d: \"%s\"\n", e, getpid(), d, pid, buf);
    return 0;
}