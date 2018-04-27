#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#include <sys/ioctl.h>
#include <sys/termio.h>
#include <sys/termios.h>


#include <sys/ioctl.h>
#include <aplus/base.h>
#include <aplus/input.h>
#include <aplus/kd.h>

#define SYSCONFIG_VERBOSE
#include <aplus/sysconfig.h>
#include <aplus/utils/unicode.h>



static char* __envp[] = {
    "PATH=/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin:/bin",
    "LD_DEBUG=all",
    "LD_DEBUG_OUTPUT=/dev/log",
    "LD_LIBRARY_PATH=/usr/lib:/usr/local/lib:/lib",
    "TERM=linux",
    "TMPDIR=/tmp",
    NULL
};
    


static void init_initd() {
    DIR* d = opendir("/etc/init.d");
    if(!d) {
        fprintf(stderr, "init: no /etc/init.d directory found!\n");
        return;
    }


    struct winsize ws;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);


    struct dirent* ent;
    while((ent = readdir(d))) {
        static char path[BUFSIZ];
        sprintf(path, "/etc/init.d/%s", ent->d_name);

        int e = -1;
        do {
            if(access(path, F_OK) != 0)
                break;
            
            pid_t pid = fork();
            if(pid == -1)
                break;
            else if(pid == 0) {
                if(execle(path, path, "start", NULL, __envp) < 0)
                    perror(path);

                exit(-1);
            }
            e = 0;
        } while(0);
        
        fprintf(stderr, "\e[37minit: starting \e[36m%s\e[37m \e[%dG[ %3s ]\n", ent->d_name, ws.ws_col - 10, e == 0 ? "\e[32mOK\e[37m" : "\e[31mERR\e[37m");
    }

    closedir(d);
}


static void init_console() {
    int fd = open("/dev/console", O_RDONLY);
    if(fd < 0) {
        perror("init: /dev/console");
        return;
    }

    if(ioctl(fd, KDSETMODE, KD_GRAPHICS) != 0)
        perror("init: console_ioctl()");
    
    close(fd);
}

static void init_welcome() {

    printf(
        "        ......                    \n"
        "   ............                   \n"
        " ...............                  \n"
        " ................     ...         \n"
        "   ......................         \n"
        "    .................             \n"
        "    ................ .            \n"
        "..............  . ... .  ..       \n"
        "..   .....  ..  ...........   .   \n"
        "     . ....... .........      . ..\n"
        "     . .  ...... ...  .       ... \n"
        "     ...             ..      ...  \n"
        "      ....        ...      ..  .  \n"
        "         ...............  ..      \n"
        "           ............ ..        \n"
        "           .. .... .. ..          \n"
        "          . ..  .....  .          \n"
        "          ........ ..             \n"
        "            .           .         \n"
    );
    printf("\n\n");

    fprintf(stdout, "\033[0;39m\e[37mWelcome to aPlus!\e[39m\n");
}


int main(int argc, char** argv) {
    fcntl(open("/dev/stdin", O_RDONLY), F_DUPFD, STDIN_FILENO);
    fcntl(open("/dev/stdout", O_WRONLY), F_DUPFD, STDOUT_FILENO);
    fcntl(open("/dev/stderr", O_WRONLY), F_DUPFD, STDERR_FILENO);
   
    init_console();
    init_welcome();
    init_initd();
    

    ioctl(STDIN_FILENO, TIOCLKEYMAP, (void*) sysconfig("sys.locale", "en-US"));
    
    for(; errno != ECHILD; )
        waitpid(-1, NULL, 0);
}