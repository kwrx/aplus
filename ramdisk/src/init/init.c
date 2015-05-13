#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>


static void die(int unused) {
	(void) unused;

	exit(-1);
}


int main(int argc, char** argv) {



	char* rootdev = (char*) getenv("ROOTDEV");
	char* fstype = (char*) getenv("ROOTFS");
	char* shell = (char*) getenv("SHELL");

	if(!rootdev || !fstype)
		die(printf("init: cannot found root device!\n"));
	else
		if(mount(rootdev, "/dev/root", fstype, 0, NULL) != 0)
			die(printf("init: cannot mount %s in \"/dev/root\" (%s) - %s\n", rootdev, fstype, strerror(errno)));
		
	
	symlink("/dev", "/dev/root/dev");
	symlink("/tmp", "/dev/root/tmp");
	symlink("/proc", "/dev/root/proc");

	chroot("/dev/root");



#if 0
	if(fork() == 0)
		execl("/bin/session", NULL);
	else
		wait(NULL);
#endif

	if(execlp(shell, NULL))
		perror(shell);

	return 0;
}
