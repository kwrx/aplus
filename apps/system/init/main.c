#include <unistd.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <aplus/base.h>
#include <aplus/fbdev.h>

#define SYSCONFIG_VERBOSE
#include <aplus/sysconfig.h>



static void parse_fstab() {
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

        fprintf(stdout, "mount: \'%s\' in \'%s\' with \'%s\' (%s)\n", opt[0], opt[1], opt[2], opt[3]);
    }
   

    fclose(fp);
}


int main(int argc, char** argv) {
    fcntl(open("/dev/stdin", O_RDONLY), F_DUPFD, STDIN_FILENO);
    fcntl(open("/dev/stdout", O_WRONLY), F_DUPFD, STDOUT_FILENO);
    fcntl(open("/dev/stderr", O_WRONLY), F_DUPFD, STDERR_FILENO);

    parse_fstab();

   
    char* __envp[] = {
        "HOME=/home/root",
        "PATH=/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin",
        "USER=root",
        "LOGNAME=root",
        "TERM=linux",
        "SHELL=/usr/bin/sh",
        "TMPDIR=/tmp",
        NULL
    };
    
    char* __argv[] = { NULL, NULL };

    
    __argv[0] = (char*)
        sysconfig("sys.startup", SYSCONFIG_FORMAT_STRING,     // OR
        sysconfig("sys.shell", SYSCONFIG_FORMAT_STRING,       // OR
        0
    ));
    
    return execve(__argv[0], __argv, __envp);
}