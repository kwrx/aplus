#include <aplus.h>
#include <aplus/task.h>
#include <aplus/fs.h>
#include <aplus/fsys.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include "procfs.h"

extern task_t* current_task;
extern list_t* task_queue;



struct dirent* procfs_readdir (struct inode* inode, int index) {
	if(!inode)
		return NULL;
	
	if(index < 0)
		return NULL;

	task_t* tk = (task_t*) list_at_index(task_queue, index);
	if(!tk)
		return NULL;

	struct dirent* ent = kmalloc(sizeof(struct dirent));
	memset(ent, 0, sizeof(struct dirent));

	ksprintf(ent->d_name, "%d", tk->pid);
	ent->d_ino = PROCFS_INO_START + tk->pid;

	return ent;
}
