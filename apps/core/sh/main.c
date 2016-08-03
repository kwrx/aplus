#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>




int main(int argc, char** argv, char** env) {
    
    
    fprintf(stderr, "aPlus Shell\n");
    do {
        char cwd[BUFSIZ];
        getcwd(cwd, BUFSIZ);
        
        fprintf(stderr, " #%s> ", cwd);
        
        static char buf[BUFSIZ];
        memset(buf, 0, sizeof(buf));
        
        static char* __argv[255];
        memset(__argv, 0, sizeof(__argv));
        
        
        if(!fgets(buf, BUFSIZ, stdin))
            continue;
            
                
        buf[strlen(buf) - 1] = 0;
        
        if(buf[0] == '\0')
            continue;
         
        
        char* s = buf;
        char* p = s;
        int idx = 0;
        
        while((p = strchr(s, ' '))) {
            *p++ = 0;
            __argv[idx++] = s;
            s = p;
        }
        
        int no_wait = 0;
        if(strcmp(s, "&") == 0)
            no_wait = 1;
        else
            __argv[idx++] = s;
        
        __argv[0] = buf;
        __argv[idx++] = NULL;
        
        if(strcmp(buf, "cd") == 0)
            { if(chdir(__argv[1]) != 0) perror("cd"); continue; }
            
        volatile int e = 0;
        e = fork();
        if(e == -1) {
            perror("fork");
            return e;
        }
        else if(e == 0)
            exit(execvp(buf, __argv));
        else 
            if(!no_wait)
                wait(NULL);
            
    } while(1);
}