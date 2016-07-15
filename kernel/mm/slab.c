#include <xdev.h>
#include <xdev/mm.h>
#include <xdev/ipc.h>
#include <xdev/debug.h>

static int heap_allocated = 0;

mutex_t mtx_kmalloc;
mutex_t mtx_kfree;


__section(".pool")
static uint32_t slab_bitmap[((CONFIG_HEAP_SIZE / MM_BLOCKSZ) / 8) / sizeof(uint32_t)] = { 0 };
static mm_state_t __kvm_state = {
	0LL,
	0LL,
	(uint8_t*) slab_bitmap
};

__section(".pool")
static uint8_t slab_pool[256 * 1024] = { 0 };
static uint16_t slab_ptr = 0;

#define FR_SET(x)	\
	slab_bitmap[(x) / 32] |= (1 << ((x) % 32))

#define FR_CLR(x)	\
	slab_bitmap[(x) / 32] &= ~(1 << ((x) % 32))

#define FR_TST(x)	\
	(slab_bitmap[(x) / 32] & (1 << ((x) % 32)))


static int FR_FIRST(int count) {
	register long i, j, f = 0;
	for(i = 0; i < (sizeof(slab_bitmap) / sizeof(uint32_t)); i++) {
		if(likely(slab_bitmap[i] == 0xFFFFFFFF))
			continue;

		for(j = 0; j < 32; j++) {
			if(!(slab_bitmap[i] & (1 << j)))
				f++;
			else
				f = 0;

			if(count == f)
				return (i * 32) + (j - f + 1);

		}
	}

	return E_ERR;
}


__malloc
void* kmalloc(size_t size, int gfp) {
	void* p = NULL;

retry:
	mutex_lock(&mtx_kmalloc);
	
	do {

		if(unlikely(!heap_allocated)) {
			KASSERT(slab_ptr + size < sizeof(slab_pool));

			p = &slab_pool[slab_ptr];
			slab_ptr += size;

			break;
		}

		int count = (size / MM_BLOCKSZ) + 1;

		register int fx = FR_FIRST(count);
		if(unlikely(fx == E_ERR))
			break;

		while(count--)
			FR_SET(fx + count);

		p = (void*) CONFIG_HEAP_BASE + (fx * MM_BLOCKSZ);

	} while(0);

	mutex_unlock(&mtx_kmalloc);

	if(unlikely(!p)) {
		if(gfp & __GFP_WAIT) {
			if(gfp & __GFP_HIGH)
				goto retry;
			
			sys_yield();
			goto retry;
		}

		return NULL;
	}


	struct {
		uint32_t magic;
		uint32_t size;
		uint8_t data[0];
	} *h = p;

	h->magic = KMALLOC_MAGIC;
	h->size = size;

	return h->data;
}


__malloc
void* kvalloc(size_t size, int gfp) {
	uintptr_t p = (uintptr_t) kmalloc(size + 0x1000, gfp);
	p += 0x1000 - 16;

	struct {
		uint32_t magic;
		uint32_t size;
		uint8_t data[0];
	} *h = (void*) p;

	h->magic = KMALLOC_MAGIC;
	h->size = size + 0x1000;

	return h->data;
}


__malloc
void* kcalloc(size_t x, size_t y, int gfp) {
	register void* p = kmalloc(x * y, gfp);
	if(likely(p))
		memset(p, 0, x * y);

	return p;
}


void kfree(void* p) {
	struct {
		uint32_t magic;
		uint32_t size;
		uint8_t data[0];
	} *h = (void*) ((uintptr_t) p - 8);

	if(unlikely(h->magic != KMALLOC_MAGIC))
		return;

	mutex_lock(&mtx_kfree);

	uintptr_t address = (uintptr_t) h;
	address -= CONFIG_HEAP_BASE;
	address /= MM_BLOCKSZ;

	int count = (h->size / MM_BLOCKSZ) + 1;
	while(count--)
		FR_CLR(address + count);

	mutex_unlock(&mtx_kfree);
}


__malloc
void* std_kmalloc(size_t size) {
	return kmalloc(size, GFP_KERNEL);
}

__malloc
void* std_kcalloc(size_t x, size_t y) {
	return kcalloc(x, y, GFP_KERNEL);
}

void std_kfree(void* ptr) {
	kfree(ptr);
}


__malloc
void* malloc(size_t size) {
	return kmalloc(size, GFP_KERNEL);
}

__malloc
void* calloc(size_t x, size_t y) {
	return kcalloc(x, y, GFP_KERNEL);
}

void free(void* ptr) {
	kfree(ptr);
}


int slab_init(void) {
	mutex_init(&mtx_kmalloc, MTX_KIND_DEFAULT);
	mutex_init(&mtx_kfree, MTX_KIND_DEFAULT);

	memset(slab_bitmap, 0, sizeof(slab_bitmap));



	uintptr_t frame = CONFIG_HEAP_BASE;
	uintptr_t fend = CONFIG_HEAP_BASE + CONFIG_HEAP_SIZE;

	for(; frame < fend; frame += 0x1000)
		map_page(frame, pmm_alloc_frame() << 12, 0);

	heap_allocated = 1;

	return E_OK;
}

mm_state_t* kvm_state(void) {
	__kvm_state.used = 0;
	__kvm_state.total = CONFIG_HEAP_SIZE;
	__kvm_state.frames = (uint8_t*) slab_bitmap;	

	int i, j;
	for(i = 0; i < (int)(sizeof(slab_bitmap) / sizeof(uint32_t)); i++) {
		for(j = 0; j < 32; j++)
			if((slab_bitmap[i] & (1 << j)))
				__kvm_state.used += MM_BLOCKSZ;
	}

	return &__kvm_state;
}




EXPORT(kmalloc);
EXPORT(kvalloc);
EXPORT(kcalloc);
EXPORT(kfree);
EXPORT(malloc);
EXPORT(calloc);
EXPORT(free);
EXPORT(kvm_state);
