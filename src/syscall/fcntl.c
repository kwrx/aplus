#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/syscall.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <dirent.h>

#include <stdio.h>

extern task_t* current_task;
extern inode_t* vfs_root;


/**
 *	\brief This function shall perform the operations described below on open files.\n
		The available values for cmd are defined in <fcntl.h> and are as follows.
	+ F_DUPFD\n
    	- Return a new file descriptor which shall be the lowest numbered available (that is,
		not already open) file descriptor greater than or equal to the third argument, arg, 
		taken as an integer of type int.\n
 		The new file descriptor shall refer to the same open file description as the original
		file descriptor, and shall share any locks.\n
		The FD_CLOEXEC flag associated with the new file descriptor shall be cleared to 
		keep the file open across calls to one of the exec functions.\n
	+ F_GETFD\n
   		- Get the file descriptor flags defined in <fcntl.h> that are associated with the file 
		descriptor fildes.\n
		File descriptor flags are associated with a single file descriptor
		and do not affect other file descriptors that refer to the same file.
	+ F_SETFD\n
    	- Set the file descriptor flags defined in <fcntl.h>, that are associated with fildes,
		to the third argument, arg, taken as type int.\n
		If the FD_CLOEXEC flag in the third	argument is 0, the file shall remain open 
		across the exec functions; otherwise, the file shall be closed upon successful
		execution of one of the exec functions.\n
	+ F_GETFL\n
    	- Get the file status flags and file access modes, defined in <fcntl.h>, for the
		file description associated with fildes. The file access modes can be extracted
		from the return value using the mask O_ACCMODE, which is defined in <fcntl.h>.\n
		File status flags and file access modes are associated with the file description
		and do not affect other file descriptors that refer to the same file with 
		different open file descriptions.\n
	+ F_SETFL\n
    	- Set the file status flags, defined in <fcntl.h>, for the file description associated
	 	with fildes from the corresponding bits in the third argument, arg, taken as type int.\n
	 	Bits corresponding to the file access mode and the file creation flags, as defined in <fcntl.h>,
		that are set in arg shall be ignored. If any bits in arg other than those mentioned here are
		changed by the application, the result is unspecified.\n
	+ F_GETOWN\n
    	- If fildes refers to a socket, get the process or process group ID specified to receive SIGURG
		signals when out-of-band data is available.\n
		Positive values indicate a process ID; negative values, other than -1, indicate a process group ID.\n
		If fildes does not refer to a socket, the results are unspecified.\n
	+ F_SETOWN\n
    	- If fildes refers to a socket, set the process or process group ID specified to receive SIGURG
		signals when out-of-band data is available, using the value of the third argument, arg, taken 
		as type int.\n
		Positive values indicate a process ID; negative values, other than -1, indicate a process group ID.\n
		If fildes does not refer to a socket, the results are unspecified.\n\n\n\n
    
 * 	\param fd File descriptor.
 * 	\param request Request operation.
 *	\param buf Optional arguments.
 *	\return Upon successful completion, the value returned shall depend on cmd.\n
    		Otherwise, -1 shall be returned and errno set to indicate the error
 */
int sys_fcntl(int fd, int request, void* buf) {
	if(unlikely(!current_task))
		return -1;
		

	if(unlikely(fd < 0 || fd > TASK_MAX_FD)) {
		errno = EBADF;
		return -1;
	}
	
	
	inode_t* ino = current_task->fd[fd];
	if(unlikely(!ino)) {
		errno = EBADF;
		return -1;
	}


	switch(request) {
		case F_DUPFD: {
			int nfd = (int) buf;
			if(nfd < 0 || nfd > TASK_MAX_FD) {
				errno = EINVAL;
				return -1;
			}

			current_task->fd[nfd] = ino;
			return nfd;
		}
		case F_GETFD:
			return 0;
		case F_SETFD:
			return 0;
		case F_GETFL:
			return 0;
		case F_SETFL:
			switch((int)buf) {
				case O_APPEND:
					ino->position = ino->size;
					return 0;
				case O_NONBLOCK:
					return 0;
				default:
					errno = EINVAL;
					return -1;
			}
			break;
	}

	return 0;
}

SYSCALL(sys_fcntl, 19);
