#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>


static int __check(struct stat* st, int flags, int rdflags, int wrflags, int exflags) {
	if(flags & R_OK)
		if(!(st->st_mode & rdflags))
			return -1;

	if(flags & W_OK)
		if(!(st->st_mode & wrflags))
			return -1;

	if(flags & X_OK)
		if(!(st->st_mode & exflags))
			return -1;

	return 0;
}


int access(const char* pathname, int flags) {
	struct stat st;
	if(stat(pathname, &st) != 0)
		return -1;

	if(geteuid() == st.st_uid)
		return __check(&st, flags, S_IRUSR, S_IWUSR, S_IXUSR);
	if(getegid() == st.st_gid)
		return __check(&st, flags, S_IRGRP, S_IWGRP, S_IXGRP);

	return __check(&st, flags, S_IROTH, S_IWOTH, S_IXOTH);
}
