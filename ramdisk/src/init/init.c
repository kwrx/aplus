#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>

int main(int argc, char** argv) {



	char* rootdev = (char*) getenv("ROOTDEV");
	char* fstype = (char*) getenv("ROOTFS");

	
	

	if(!rootdev || !fstype)
		printf("init: cannot found root device!\n");
	else
		if(mount(rootdev, "/dev/root", fstype, 0, NULL) != 0)
			printf("init: cannot mount %s in \"/dev/root\" (%s) - %s\n", rootdev, fstype, strerror(errno));
		else
			symlink("/dev/root", "/root");


	DIR* d = opendir("/root");
	if(!d)
		exit(printf("errno - %s\n", strerror(errno)));

	struct dirent* e;
	while((e = readdir(d)))
		printf(" - %s\n", e->d_name);
	
	closedir(d); 


	return 0;
}
