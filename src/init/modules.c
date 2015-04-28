#include <aplus.h>
#include <aplus/task.h>
#include <aplus/mm.h>
#include <aplus/fs.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>


extern task_t* current_task;


#define MODPATH			"/dev/ramdisk/mod"

int init_modules() {
	int fd = sys_open(MODPATH, O_RDONLY, 0655);
	if(fd <= 0) {
		kprintf("mod: cannot open mod directory\n");	
		return -1;
	}

	struct dirent* ent;
	int i = 0;
	while((ent = (struct dirent*) sys_readdir(fd, i++))) {
		kprintf("mod: loading %s\n", ent->d_name);

		char name[255];
		memset(name, 0, sizeof(name));
		strcpy(name, MODPATH "/");
		strcat(name, ent->d_name);

		int fd = sys_open(name, O_RDONLY, 0644);
		if(fd <= 0) {
			kprintf("mod: cannot open %s\n", name);
			continue;
		}

		inode_t* ino = current_task->fd[fd];
		void* image = (void*) kmalloc(ino->size);

		sys_read(fd, image, ino->size);
		sys_close(fd);


		void (*init) () = NULL;
		if((init = (void (*)()) exec_load(name, image, NULL, (size_t*) &ino->size)) == NULL) {
			kprintf("mod: cannot load %s\n", name);
			continue;
		}

		sys_clone(init, NULL, CLONE_SIGHAND | CLONE_FILES | CLONE_FS, NULL);
	}

	sys_close(fd);
	return 0;
}
