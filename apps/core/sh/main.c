#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


int main(int argc, char** argv, char** env) {
    fprintf(stderr, "aPlus Shell\n");
    do {
        fprintf(stderr, " #> ");
        
        static char buf[BUFSIZ];
        memset(buf, 0, sizeof(buf));
        
        static char* __argv[255];
        memset(__argv, 0, sizeof(__argv));
        
        
        if(!fgets(buf, BUFSIZ, stdin))
            continue;
            
       buf[strlen(buf) - 1] = 0;

       __argv[0] = buf;
       __argv[1] = NULL;
        
        static int e = 0;
        e = fork();
        if(e == -1) {
            perror("fork");
            return e;
        }
        else if(e == 0)
            exit(execve(buf, __argv, NULL));
        else
            wait(NULL);
    } while(1);
}