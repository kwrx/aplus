#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <errno.h>

pid_t tcgetpgrp(int fd) {
	pid_t pid;
	if(ioctl(fd, TIOCGPGRP, &pid) != 0)
		return -1;

	return pid; 
}

int tcsetpgrp(int fd, pid_t pid) {
	return ioctl(fd, TIOCSPGRP, &pid);
}

int tcsetattr(int fd, int flags, const struct termios* p) {
    switch(flags) {
        case TCSANOW:
            return ioctl(fd, TIOCSETA, p);
        case TCSADRAIN:
            return ioctl(fd, TIOCSETAW, p);
        case TCSAFLUSH:
            return ioctl(fd, TIOCSETAF, p);
        default:
            break;
    }
    
    errno = EINVAL;
    return -1;
}

int tcgetattr(int fd, struct termios* p) {
    return ioctl(fd, TIOCGETA, p);
}

