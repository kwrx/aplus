#include <unistd.h>
#include <sys/types.h>

uid_t getegid(void) {
	return getgid();
}
