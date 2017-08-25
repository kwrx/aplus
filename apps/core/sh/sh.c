#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <getopt.h>
#include <glob.h>

#include "sh.h"


static char buf[BUFSIZ];
sh_alias_t* sh_aliases = NULL;


char* sh_gethostname(void) {
    CLRBUF();
    if(gethostname(buf, BUFSIZ) != 0) {
        perror("hostname");
        exit(-1);
    }

    return strdup(buf);
}


void sh_cmdline(char* cmdline) {
    if(!cmdline)
        return;

    if(cmdline[0] == '\0')
        return;

    
    char** argv = (char**) calloc(1024, sizeof(char*));
    char** trgv = (char**) calloc(1024, sizeof(char*));

   
    cmdline = strdup(cmdline);
    
    int i = 0;
    int nowait = 0;
    for(char* p = strtok(cmdline, " "); p; p = strtok(NULL, " "))
        if(strcmp(p, "&") == 0)
            nowait = 1;
        else
            trgv[i++] = p;

    trgv[i++] = NULL;



    glob_t gl;

    int j = 0;
    for(i = 0; trgv[i]; i++) {
        if(trgv[i][0] == '-')
            argv[j++] = strdup(trgv[i]);


        glob(trgv[i], GLOB_NOSORT | GLOB_TILDE, NULL, &gl);

        if(gl.gl_pathc) {
            int k;
            for(k = 0; k < gl.gl_pathc; k++)
                argv[j++] = strdup(gl.gl_pathv[k]);
        } else
            argv[j++] = strdup(trgv[i]);
            
        globfree(&gl);
    }
    
    free(trgv);



    sh_alias_t* sl;
    for(sl = sh_aliases; sl; sl = sl->next) {
        if(strcmp(argv[0], sl->key) == 0) {
            switch(sl->type) {
                case SH_ALIAS_TYPE_FUNC:
                    sl->func(argv);
                    break;
                case SH_ALIAS_TYPE_STRING:
                    sh_cmdline(sl->string);
                    break;
                default:
                    fprintf(stderr, "sh: BUG! invalid alias type %d!\n", sl->type);
                    break;
            }

            return;
        }
    }


    int e = fork();
    if(e == 0) {
        execvp(argv[0], argv);
        perror(argv[0]);
        exit(100);
    } else if(e < 0)
        perror("fork");
    else
        if(!nowait)
            wait(NULL);




    for(i = 0; argv[i]; i++)
        free(argv[i]);

    free(argv);
    return;
}

void sh_alias(char* k, void* v, int t) {
    sh_alias_t* i = (sh_alias_t*) calloc(1, sizeof(sh_alias_t));
    if(!i) {
        perror("calloc");
        exit(-1);
    }

    i->key = k;
    i->value = v;
    i->type = t;

    i->next = sh_aliases;
    sh_aliases = i;
}