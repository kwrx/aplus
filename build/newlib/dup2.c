#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

int dup2(int oldfd, int newfd) {
	return fcntl(oldfd, F_DUPFD, newfd);
}
