#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <aplus/fbdev.h>

#define SYSCONFIG_VERBOSE
#include <aplus/sysconfig.h>




int main(int argc, char** argv) {
    fcntl(open("/dev/stdin", O_RDONLY), F_DUPFD, STDIN_FILENO);
    fcntl(open("/dev/stdout", O_WRONLY), F_DUPFD, STDOUT_FILENO);
    fcntl(open("/dev/stderr", O_WRONLY), F_DUPFD, STDERR_FILENO);
    
    
    char* __envp[] = {
        "HOME=/home",
        "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin",
        "USER=root",
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