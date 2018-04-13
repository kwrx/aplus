#include <unistd.h>
#include <sys/types.h>

pid_t getpgrp(void) {
	return getpgid(0);
}

pid_t setpgrp(void) {
	setpgid(0, 0);
}
