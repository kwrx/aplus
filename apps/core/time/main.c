#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/times.h>
#include <sys/wait.h>



int main(int argc, char** argv) {
    
    void show_time(struct tms* s, struct tms* e, clock_t cs, clock_t ce) {
        printf(
            "real   %gs\n"
            "user   %gs\n"
            "sys    %gs\n",
            (float) (ce - cs) / CLOCKS_PER_SEC,
            (float) (e->tms_cutime - s->tms_cutime) / CLOCKS_PER_SEC,
            (float) (e->tms_cstime - s->tms_cstime) / CLOCKS_PER_SEC
        );
        
        exit(0);
    }
    
    struct tms ts, te;
    clock_t cs, ce;
    
    cs = times(&ts);
    
    if(argc < 2)
        show_time(&ts, &ts, cs, cs);
    
    
    int e = fork();
    if(e == 0)
        execvp(argv[1], &argv[1]);
    else if(e == -1) {
        perror("fork");
        exit(-1);
    } else {
        wait(NULL);
        
        ce = times(&te);
        show_time(&ts, &te, cs, ce);
    }
    
    
    return 0;
}