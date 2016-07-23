#include <avm.h>
#include "../ops.h"

int avm_spinlock_init(avm_spinlock_t* lock) {
	if(unlikely(!lock))
		return J_ERR;

	*lock = 0;
	return J_OK;
}


void avm_spinlock_lock(avm_spinlock_t* lock) {
#if defined (__arm__)
	while(*lock)
		avm->yield();
	*lock = 1;
#else 
	while(!__sync_bool_compare_and_swap(lock, 0, 1))
		avm->yield();
#endif
}

int avm_spinlock_trylock(avm_spinlock_t* lock) {
#if defined (__arm__)
	if(*lock)
		return J_ERR;
	*lock = 1;
#else 
	if(unlikely(!__sync_bool_compare_and_swap(lock, 0, 1)))
		return J_ERR;
#endif

	return J_OK;
}

void avm_spinlock_unlock(avm_spinlock_t* lock) {
#if defined (__arm__)
	*lock = 0;
#else
	__sync_lock_release(lock);
#endif
}

