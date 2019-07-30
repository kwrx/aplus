/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argz.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef _POSIX_SOURCE
#undef _POSIX_SOURCE
#endif
#include <sys/wait.h>


int __aplus_crt__ = 1;
int __last_signo = -1;
char __progname[BUFSIZ];

unsigned long __tls_start = 0;
unsigned long __tls_end = 0;


extern char** environ;
extern int __bss_start;
extern int end;


extern int __sigtramp(int);
extern void __libc_init_array();
extern void __libc_fini_array();
extern void __exit(int);

extern void exit(int);
extern int main(int, char**, char**);



static void __init_traps() {

    int i;
    for(i = 0; i < NSIG; i++)
        signal(i, SIG_DFL);

}


static void tzinit() {

    int fd = open("/etc/localtime", O_RDONLY);
    if(fd < 0)
        return;
    
    struct {
        char magic[4];
        char version;
    } head;

    if(read(fd, &head, sizeof(head)) != sizeof(head))
        goto done;

    if(strncmp(head.magic, "TZif", 4) != 0)
        goto done;

    if(head.version < '2')
        goto done;


    lseek(fd, -2, SEEK_END);

    do {

        char ch;
        if(read(fd, &ch, 1) != 1)
            break;


        if(ch == '\n') {

            char buf[64];
            memset(buf, 0, sizeof(buf));

            if(read(fd, buf, sizeof(buf)) <= 0)
                break;

            
            buf[strlen(buf) - 1] = '\0';
            setenv("TZ", buf, 1);

            break;
        }

    } while(lseek(fd, -2, SEEK_CUR) > 44);

done:
    close(fd);

}



static void __arg_add(char*** argv, int* argc, const char* s) {

    if(*argv == NULL)
        *argv = calloc(1, sizeof(char*)),
        *argc = 0;


    (*argc)++;
    (*argv) = realloc(*argv, *argc * sizeof(char*));

    *argv[*argc - 1] = calloc(strlen(s) + 1, 1);
    *argv[*argc] = NULL; 

    strcpy(*argv[*argc - 1], s);

}


static void __init_tls() {

#if 0
    if(!__tls_start || __tls_end)
        return;

    if((__tls_end - __tls_start) == 0)
        return;


    struct {
        uintptr_t ptr;
        uintptr_t top;
        uintptr_t base;
    } __attribute__((packed)) *tls = (void*) malloc(sizeof(uintptr_t) * 3);


    tls->base = (uintptr_t) malloc(__tls_end - __tls_start);
    tls->top  = tls->base + (__tls_end - __tls_start);
    tls->ptr  = tls->top;


    //set_thread_address(tls);
#endif
}


static void __init(uintptr_t* brk) {

    long i;
    for(i = (long) &__bss_start; i < (long) &end; i++)
        *(unsigned char*) i = 0;

    __asm__("int $0xFE" :: "a"(404));

    __libc_init_array();
    __init_traps();
    __init_tls();



    int argc = 0;
    char** argv = NULL;
        

    char* s;
    for(s = strtok((char*) brk, " "); s; s = strtok(NULL, " "))
        strcpy(__progname, s);


    for(; s; s = strtok(NULL, " ")) {

        if(strcmp(s, "\e$$$") == 0)
            break;

        __arg_add(&argv, &argc, s);

    }


    for(; s; s = strtok(NULL, " ")) {

        if(strcmp(s, "\e$$$") == 0)
            break;
        
        putenv(s);

    }



    tzinit();
    tzset();

    atexit(__libc_fini_array);
    exit(main(argc, argv, environ));

}


void _start(uintptr_t* brk) {

    __tls_start = *brk++;
    __tls_end   = *brk++;

    __init(brk);

}
