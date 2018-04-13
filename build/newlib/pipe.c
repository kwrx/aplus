#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>


int pipe(int fd[2]) {
    if(!fd) {
        errno = EINVAL;
        return -1;
    }
    
    char pathname[256];
    tmpnam(pathname);
    
    if(mkfifo(pathname, 0666) != 0)
        return -1;
        
    fd[0] = open(pathname, O_RDONLY);
    fd[1] = open(pathname, O_WRONLY);
    
    return 0;
}
