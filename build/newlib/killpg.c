#include <unistd.h>
#include <sys/types.h>
#include <signal.h>


int killpg(pid_t pid, int signal) {
	return kill(-pid, signal);
}
