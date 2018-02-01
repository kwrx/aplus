#include <unistd.h>
#include <sys/wait.h>


pid_t vfork(void) {
	pid_t p = fork();
	if(p > 0)
		waitpid(p, NULL, 0);

	return p;
}
