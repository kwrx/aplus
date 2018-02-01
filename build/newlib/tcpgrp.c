#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/termios.h>

pid_t tcgetpgrp(int fd) {
	pid_t pid;
	if(ioctl(fd, TIOCGPGRP, &pid) != 0)
		return -1;

	return pid; 
}

int tcsetpgrp(int fd, pid_t pid) {
	return ioctl(fd, TIOCSPGRP, &pid);
}
