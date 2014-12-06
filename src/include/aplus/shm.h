#ifndef _SHM_H
#define _SHM_H

#include <aplus.h>
#include <aplus/spinlock.h>
#include <stdint.h>

#define SHM_MAGIC		0x1DAB3992

typedef struct {
	uint32_t magic;
	uint32_t addr;
	uint32_t size;

	int refcount;
} shm_chunk_t;


shm_chunk_t* shm_acquire_chunk(uint32_t* size);
void* shm_acquire(const char* path, uint32_t* size);
int shm_release_chunk(shm_chunk_t* chunk);
int shm_release(const char* path);


#endif
