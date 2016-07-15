#ifndef _MM_H
#define _MM_H

#define KMALLOC_MAGIC						0xCAFE1234

#define __GFP_HIGH							1
#define __GFP_WAIT							2

#define GFP_ATOMIC							(__GFP_HIGH)
#define GFP_KERNEL							(__GFP_HIGH | __GFP_WAIT)
#define GFP_USER							(__GFP_WAIT)

#define HF_NULL								(0)
#define HF_ZERO								(1)

#define MM_POOL_SIZE						131072
#define MM_BLOCKSZ							4096

#define PAGE_SIZE							(0x1000)


#define P2V(x)								((x) + CONFIG_KERNEL_BASE)
#define V2P(x)								__V2P((virtaddr_t) (x))




#ifndef __ASSEMBLY__

#include <xdev.h>
#include <libc.h>

typedef uintptr_t physaddr_t;
typedef uintptr_t virtaddr_t;

typedef struct {
	uintptr_t used;
	uintptr_t total;
	uint8_t* frames;
} mm_state_t;


int slab_init(void);
int pmm_init(void);
int vmm_init(void);
int mm_init(void);

physaddr_t __V2P(virtaddr_t);

void unmap_page(virtaddr_t virtaddr);
void map_page(virtaddr_t virtaddr, physaddr_t physaddr, int user);
virtaddr_t get_free_pages(int count, int maked, int user);

physaddr_t pmm_alloc_frame(void);
physaddr_t pmm_alloc_frames(int count);
void pmm_free_frame(physaddr_t);
void pmm_free_frames(physaddr_t address, int count);
void pmm_claim(physaddr_t mstart, physaddr_t mend);



void* kmalloc(size_t, int);
void* kvalloc(size_t, int);
void* kcalloc(size_t, size_t, int);
void kfree(void*);

void* std_kmalloc(size_t);
void* std_kcalloc(size_t, size_t);
void std_kfree(void*);


mm_state_t* pmm_state(void);
mm_state_t* kvm_state(void);


#endif

#endif
