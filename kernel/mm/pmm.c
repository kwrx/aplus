#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/debug.h>


__section(".pool")
static uint32_t frames[MM_POOL_SIZE / sizeof(uint32_t)] = { 0 };
static mm_state_t __pmm_state = {
	0LL,
	0LL,
	(uint8_t*) frames
};


#define FR_SET(x)	\
	frames[(x) / 32] |= (1 << ((x) % 32))

#define FR_CLR(x)	\
	frames[(x) / 32] &= ~(1 << ((x) % 32))

#define FR_TST(x)	\
	(frames[(x) / 32] & (1 << ((x) % 32)))


static int FR_FIRST(int count) {
	register long i, j, f = 0;
	for(i = 0; i < (sizeof(frames) / sizeof(uint32_t)); i++) {
		if(likely(frames[i] == 0xFFFFFFFF))
			continue;

		for(j = 0; j < 32; j++) {
			if(!(frames[i] & (1 << j))) {
				register int b = i * 32 + j;
				register int c = 0;
			
				for(f = 0; f <= count; f++) {
					if(!FR_TST(b + f))
						c++;
					else
						break;
						
					if(c > count)
						return b;
				}
			}
		}
	}

	return E_ERR;
}



physaddr_t pmm_alloc_frame(void) {
	register int fx = FR_FIRST(1);
	KASSERT(fx != E_ERR);

	FR_SET(fx);
	return (physaddr_t) fx;
}

physaddr_t pmm_alloc_frames(int count) {
	register int fx = FR_FIRST(count);
	KASSERT(fx != E_ERR);

	while(count--)
		FR_SET(fx + count);
	FR_SET(fx);

	return (physaddr_t) fx;
}

void pmm_free_frame(physaddr_t address) {
	FR_CLR(address);
}

void pmm_free_frames(physaddr_t address, int count) {
	while(count--)
		FR_CLR(address + count);
	FR_CLR(address);
}

void pmm_claim(physaddr_t mstart, physaddr_t mend) {
	KASSERT(mstart < mend);

#if 0
	kprintf(INFO "pmm_claim(0x%x, 0x%x)\n", mstart, mend);
#endif

	register int i = mstart / MM_BLOCKSZ;
	register int j = mend / MM_BLOCKSZ;

	while(i < j) {
		FR_SET(i);
		i++;
	}
}


mm_state_t* pmm_state(void) {

	__pmm_state.used = 0;
	__pmm_state.total = mbd->memory.size;
	__pmm_state.frames = (uint8_t*) frames;	

	int i, j;
	for(i = 0; i < (MM_POOL_SIZE / sizeof(uint32_t)); i++) {
		for(j = 0; j < 32; j++)
			if((frames[i] & (1 << j)))
				__pmm_state.used += MM_BLOCKSZ;
	}

	return &__pmm_state;
}

int pmm_init(void) {
	memset(frames, 0, sizeof(frames));

	pmm_claim(0x00000000, (physaddr_t) mbd->memory.start);
	
	return E_OK;
}

EXPORT(pmm_state);
EXPORT(pmm_alloc_frame);
EXPORT(pmm_free_frame);
EXPORT(pmm_claim);
