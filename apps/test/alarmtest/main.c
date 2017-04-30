#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/sched.h>

int seconds = 1;
static int alarm_handler(int sig) {
    printf("ALARM RAISED (seconds: %d)\n", seconds);

    signal(SIGALRM, alarm_handler);
    alarm(seconds);
    return sig;
}

int main(int argc, char** argv) {
    fprintf(stderr, "Seconds: ");
    scanf("%d", &seconds);

    signal(SIGALRM, alarm_handler);
    alarm(seconds);

    for(;;)
        sched_yield();
}