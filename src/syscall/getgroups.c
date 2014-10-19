#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/fs.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <dirent.h>

extern task_t* current_task;
static gid_t __groups[] = {
	GID_SUPERUSER, 0
};

int sys_getgroups(int length, gid_t* list) {
	if(length < 0) {
		errno = EINVAL;
		return -1;
	}

	if(list == NULL) {
		errno = EINVAL;
		return -1;
	}

	if(length == 0)
		return sizeof(__groups);

	int i = 0;
	for(i = 0; i < sizeof(__groups); i++) {
		if(i >= length)
			break;

		list[i] = __groups[i];
	}

	return i;
}


SYSCALL(sys_getgroups, 81);
