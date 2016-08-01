#include <xdev.h>
#include <xdev/ipc.h>

#if CONFIG_IPC


int spinlock_init(spinlock_t* lock) {
	if(unlikely(!lock))
		return E_ERR;

	*lock = 0;
	return E_OK;
}


void spinlock_lock(spinlock_t* lock) {
#if defined (__arm__)
	while(*lock)
		sys_yield();
	*lock = 1;
#else 
	while(!__sync_bool_compare_and_swap(lock, 0, 1))
		sys_yield();
#endif
}

int spinlock_trylock(spinlock_t* lock) {
#if defined (__arm__)
	if(*lock)
		return E_ERR;
	*lock = 1;
#else 
	if(unlikely(!__sync_bool_compare_and_swap(lock, 0, 1)))
		return E_ERR;
#endif

	return E_OK;
}

void spinlock_unlock(spinlock_t* lock) {
#if defined (__arm__)
	*lock = 0;
#else
	__sync_lock_release(lock);
#endif
}

EXPORT(spinlock_init);
EXPORT(spinlock_lock);
EXPORT(spinlock_trylock);
EXPORT(spinlock_unlock);

#endif
