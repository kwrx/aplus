#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <getopt.h>

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


char* sh_login(char* hostname) {
    char* username;
    char* passwd;

    do {
        fprintf(stdout, "\n%s login: ", hostname);
        fflush(stdout);

        CLRBUF();
        if(fgets(buf, BUFSIZ, stdin) <= 0)
            continue;

        username = strdup(buf);
        username[strlen(username) - 1] = '\0';

        CLRBUF();
        sprintf(buf, "/home/%s/.passwd", username);

        FILE* fp = fopen(buf, "rb");
        if(!fp) {
            fprintf(stderr, "Error: invalid username!\n");
            free(username);
            continue;
        }

        fread(buf, 16, 1, fp);
        fclose(fp);

        passwd = strdup(buf);
        passwd[strlen(passwd) - 1] = '\0';
        CLRBUF();

        fprintf(stdout, "password: ");
        fflush(stdout);

        if(fgets(buf, BUFSIZ, stdin) <= 0) {
            free(username);
            free(passwd);
            continue;
        }

        md5(buf, buf, strlen(buf));

        fprintf(stdout, "buf: %s\npasswd: %s\n", buf, passwd);

        if(strcmp(buf, passwd) == 0)
            break;
    
        fprintf(stderr, "Error: invalid password!\n");
        free(username);
        free(passwd);
    } while(1);

    free(passwd);
    return username;
}

void sh_cmdline(char* cmdline) {
    if(!cmdline)
        return;
    
    while(cmdline[0] == ' ')
        cmdline = &cmdline[1];

    if(cmdline[0] == '\0')
        return;


    static char* argv[127];
    memset(argv, 0, sizeof(argv));

    void add_argv(int s, int e, int i) {
        if(e <= s) {
            fprintf(stderr, "sh: BUG! %s (%d): invalid expr 'e <= s' : %d <= %d\n", __FILE__, __LINE__, e, s);
            exit(-1);
        }

        argv[i] = (char*) calloc(1, e - s + 1);
        strncpy(argv[i], &cmdline[s], e - s);
    }

    int i, j, k;
    for(j = 0, i = 0, k = 0; cmdline[j] != '\0'; j++) {
        switch(cmdline[j]) {
            case '\'':
            case '\"':
                k = ++j;
                
                while(cmdline[j] != '\"') {
                    if(cmdline[j] == '\0') {
                        fprintf(stderr, "sh: syntax error: expected \'\"\' before end of line\n");
                        return;
                    }

                    j++;
                }
                
                add_argv(k, j, i++);
                j += 1;
                k = j;
                break;
            case ' ':
                add_argv(k, j - 1, i++);
                while(cmdline[j] == ' ')
                    j++;

                k = j;
                break;
            default:
                continue;
        }
    }



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

    if(fork() == 0)
        exit(execvp(argv[0], argv));
    else
        wait(NULL);

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