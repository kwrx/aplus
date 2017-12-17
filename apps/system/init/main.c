#include <unistd.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>

#include <sys/ioctl.h>
#include <aplus/base.h>
#include <aplus/input.h>
#include <aplus/kd.h>

#define SYSCONFIG_VERBOSE
#include <aplus/sysconfig.h>



static char* __envp[] = {
    "PATH=/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin:/bin",
    "LD_DEBUG=all",
    "LD_DEBUG_OUTPUT=/dev/log",
    "LD_LIBRARY_PATH=/usr/lib:/usr/local/lib:/lib",
    "TERM=linux",
    "TMPDIR=/tmp",
    NULL
};
    


static void init_fstab() {
    FILE* fp = fopen("/etc/fstab", "r");
    if(!fp) {
        fprintf(stderr, "init: no /etc/fstab found!\n");
        return;
    }
    
    int cl = 1;

    static char buf[BUFSIZ];
    for(; 
        fgets(buf, sizeof(buf), fp) > 0;
        memset(buf, 0, sizeof(buf)), cl++
    ) {
        if(strlen(buf) == 0)
            continue;

        if(buf[strlen(buf) - 1] == '\n')
            buf[strlen(buf) - 1] = '\0';

        if(buf[0] == '#' || buf[0] == '\0')
            continue;
        
        int i = 0;
        char* opt[4];

        char* p;
        for(p = strtok(buf, " "); p && i < 4; p = strtok(NULL, " "))
            opt[i++] = p;

        

        if(i < 4) {
            fprintf(stderr, "/etc/fstab: syntax error at line %d, expected three parameters\n", cl);
            fclose(fp);
            return;
        }


        int flags = 0;
        if(strcmp(opt[3], "readonly") == 0)
            flags |= MNT_RDONLY;

        
        if(unlikely(mount(opt[0], opt[1], opt[2], flags, NULL) != 0))
            fprintf(stderr, "%s: failed to mount \'%s\' with \'%s\' (%s)\n", opt[0], opt[1], opt[2], opt[3]);

        fprintf(stdout, "init: mount \'%s\' in \'%s\' with \'%s\' (%s) \e[70G[ \e[32mOK\e[37m ]\n", opt[0], opt[1], opt[2], opt[3]);
    }
   

    fclose(fp);
}



static void init_initd() {
    DIR* d = opendir("/etc/init.d");
    if(!d) {
        fprintf(stderr, "init: no /etc/init.d directory found!\n");
        return;
    }

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
        
        fprintf(stderr, "init: starting \e[36m%s\e[37m \e[70G[ %3s ]\n", ent->d_name, e == 0 ? "\e[32mOK\e[37m" : "\e[31mERR\e[37m");
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


int main(int argc, char** argv) {
    fcntl(open("/dev/stdin", O_RDONLY), F_DUPFD, STDIN_FILENO);
    fcntl(open("/dev/stdout", O_WRONLY), F_DUPFD, STDOUT_FILENO);
    fcntl(open("/dev/stderr", O_WRONLY), F_DUPFD, STDERR_FILENO);
   
    init_console();    
    init_fstab();
    init_initd();
    

    ioctl(STDIN_FILENO, TIOCLKEYMAP, sysconfig("sys.locale", SYSCONFIG_FORMAT_STRING, (uintptr_t) "en-US"));
    return 0;
}