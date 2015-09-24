#include <xdev.h>
#include <xdev/mm.h>
#include <xdev/ipc.h>
#include <xdev/debug.h>
#include <libc.h>


typedef struct shm_node {
	char* path;
	physaddr_t physaddr;
	uint64_t size;

	struct shm_node* next;	
} shm_node_t;

static shm_node_t* shm_queue = NULL;


static void* shm_map(shm_node_t* node) {
	void* p = (void*) get_free_pages(node->size / PAGE_SIZE, 0, 0);
	KASSERT(p);

	map_page((virtaddr_t) p, node->physaddr, node->size);
	return p;
}

static void shm_unmap(shm_node_t* node) {
	return;
}


void* shm_obtain(const char* path, size_t* size) {
	KASSERT(path);
	KASSERT(size);	

	shm_node_t* tmp;
	for(tmp = shm_queue; tmp; tmp = tmp->next) {
		if(strcmp(tmp->path, path) == 0) {
	
			*size = tmp->size;
			return shm_map(tmp);
		}
	}

	*size += 0x1000;
	*size &= ~0xFFF;

	tmp = (shm_node_t*) kmalloc(sizeof(shm_node_t), GFP_KERNEL);
	tmp->path = strdup(path);
	tmp->physaddr = pmm_alloc_frames(*size / MM_BLOCKSZ);
	tmp->size = *size;
	tmp->next = shm_queue;
	shm_queue = tmp;

	return shm_map(tmp);
}


int shm_release(const char* path) {
	KASSERT(path);

	shm_node_t* tmp, *t2;
	if(strcmp(shm_queue->path, path) == 0) {
		t2 = shm_queue;
		shm_queue = shm_queue->next;

		shm_unmap(t2);
		pmm_free_frames(t2->physaddr, t2->size / MM_BLOCKSZ);

		kfree(t2->path);
		kfree(t2);
		
		return E_OK;

	} else {
		for(tmp = shm_queue; tmp->next; tmp = tmp->next) {
			t2 = tmp->next;

			if(strcmp(t2->path, path) == 0) {
				tmp->next = t2->next;

				shm_unmap(t2);
				pmm_free_frames(t2->physaddr, t2->size / MM_BLOCKSZ);	

				kfree(t2->path);
				kfree(t2);

				return E_OK;
			}
		}
	}

	return E_ERR;
}

