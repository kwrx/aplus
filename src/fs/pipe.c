#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/mm.h>
#include <aplus/spinlock.h>
#include <aplus/task.h>
#include <aplus/shm.h>

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>


extern task_t* current_task;


typedef struct pipeinfo {
	void* stream;
	off_t read_offset;
	off_t write_offset;
	uint32_t size;
} pipeinfo_t;


int pipe_read(inode_t* inode, char* ptr, int len) {
	pipeinfo_t* pipe = inode->userdata;
	shm_chunk_t* chunk = (shm_chunk_t*) pipe->stream;

	while(pipe->read_offset + len > pipe->write_offset)
		schedule_yield();
	
	int p;
	for(p = 0; p < (len / pipe->size); p++) {
		memcpy(
			(void*) ((uint32_t) ptr + (p * pipe->size)),
			(void*) ((uint32_t) chunk->addr + (pipe->read_offset % pipe->size)),
			pipe->size
		);

		pipe->read_offset += pipe->size;
	}

	if(len % pipe->size)
		memcpy(
			(void*) ((uint32_t) ptr + (p * pipe->size)),
			(void*) ((uint32_t) chunk->addr + (pipe->read_offset % pipe->size)),
			len % pipe->size
		);


	pipe->read_offset += len % pipe->size;
	return (int) len;
}

int pipe_write(inode_t* inode, char* ptr, int len) {
	pipeinfo_t* pipe = inode->userdata;
	shm_chunk_t* chunk = (shm_chunk_t*) pipe->stream;

	int p;
	for(p = 0; p < (len / pipe->size); p++) {
		memcpy(
			(void*) ((uint32_t) chunk->addr + (pipe->write_offset % pipe->size)),
			(void*) ((uint32_t) ptr + (p * pipe->size)),
			pipe->size
		);

		pipe->write_offset += pipe->size;
	}

	if(len % pipe->size)
		memcpy(
			(void*) ((uint32_t) chunk->addr + (pipe->write_offset % pipe->size)),
			(void*) ((uint32_t) ptr + (p * pipe->size)),
			len % pipe->size
		);

	pipe->write_offset += len % pipe->size;
	return (int) len;
}

void pipe_flush(inode_t* inode) {
	pipeinfo_t* pipe = inode->userdata;
	shm_release_chunk((shm_chunk_t*) pipe->stream);
	kfree(pipe);
}



int pipe_create(inode_t inodes[2]) {

	pipeinfo_t* pipe = (pipeinfo_t*) kmalloc(sizeof(pipeinfo_t));
	memset(pipe, 0, sizeof(pipeinfo_t));

	pipe->size = BUFSIZ;
	pipe->stream = (void*) shm_acquire_chunk(&pipe->size);
	pipe->read_offset = 0;
	pipe->write_offset = 0;


	for(int i = 0; i < 2; i++) {
		memset((void*) &inodes[i], 0, sizeof(inode_t));
		
		inodes[i].atime = inodes[i].mtime = inodes[i].ctime = sys_time(NULL);
		inodes[i].read = pipe_read;
		inodes[i].write = pipe_write;
		inodes[i].flush = pipe_flush;
		inodes[i].uid = current_task->uid;
		inodes[i].gid = current_task->gid;
		inodes[i].mode = S_IFIFO;
		inodes[i].userdata = (void*) pipe;
		inodes[i].size = (size_t) pipe->size;
	}
	
	
	strcpy(inodes[0].name, "[pipe:read]");
	strcpy(inodes[1].name, "[pipe:write]");
	
	return 0;
}
