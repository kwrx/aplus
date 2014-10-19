#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/bufio.h>
#include <aplus/mm.h>
#include <aplus/spinlock.h>
#include <aplus/task.h>

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>


extern task_t* current_task;


typedef struct pipeinfo {
	bufio_t* stream;
	off_t read_offset;
	off_t write_offset;
} pipeinfo_t;


int pipe_read(inode_t* inode, char* ptr, int len) {
	pipeinfo_t* pipe = inode->userdata;
	
	spinlock_waiton(pipe->read_offset + len > pipe->write_offset);
	
	bufio_seek(pipe->stream, pipe->read_offset % pipe->stream->size, SEEK_SET);
	
	size_t tolen = 0;
	while((tolen = bufio_read(pipe->stream, (void*) ((off_t) ptr + (off_t) tolen), len)) < len)
		bufio_seek(pipe->stream, 0, SEEK_SET);
		
	pipe->write_offset = bufio_tell(pipe->stream);
	return (int) len;
}

int pipe_write(inode_t* inode, char* ptr, int len) {
	pipeinfo_t* pipe = inode->userdata;
	bufio_seek(pipe->stream, pipe->write_offset % pipe->stream->size, SEEK_SET);
	
	size_t tolen = 0;
	while((tolen = bufio_write(pipe->stream, (void*) ((off_t) ptr + (off_t) tolen), len)) < len)
		bufio_seek(pipe->stream, 0, SEEK_SET);
		
	pipe->write_offset = bufio_tell(pipe->stream);
	return (int) len;
}

void pipe_flush(inode_t* inode) {
	pipeinfo_t* pipe = inode->userdata;
	
	bufio_free(pipe->stream);
	kfree(pipe);
}



int pipe_create(inode_t inodes[2]) {

	pipeinfo_t* pipe = (pipeinfo_t*) kmalloc(sizeof(pipeinfo_t));
	pipe->stream = (bufio_t*) bufio_alloc(BUFSIZ);
	pipe->read_offset = 0;
	pipe->write_offset = 0;


	for(int i = 0; i < 2; i++) {
		memset((void*) &inodes[i], 0, sizeof(inode_t));
		
		//inodes[i].atime = inodes[i].mtime = inodes[i].ctime = time(NULL);
		inodes[i].read = pipe_read;
		inodes[i].write = pipe_write;
		inodes[i].flush = pipe_flush;
		inodes[i].uid = current_task->uid;
		inodes[i].gid = current_task->gid;
		inodes[i].mode = S_IFIFO;
		inodes[i].userdata = (void*) pipe;
		inodes[i].size = (size_t) BUFSIZ;
	}
	
	
	strcpy(inodes[0].name, "[pipe:read]");
	strcpy(inodes[1].name, "[pipe:write]");
	
	return 0;
}