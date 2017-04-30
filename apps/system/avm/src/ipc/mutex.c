#include <avm.h>
#include "../ops.h"


int avm_mutex_init(avm_mutex_t* mtx, long kind) {
	if(unlikely(!mtx))
		return J_ERR;

	avm_spinlock_init(&mtx->lock);
	mtx->recursion = 0;
	mtx->kind = kind;
	mtx->owner = -1;
	
	return J_OK;
}


int avm_mutex_lock(avm_mutex_t* mtx) {
	if(unlikely(!mtx))
		return J_ERR;

	if(mtx->owner != avm->getpid()) {
		avm_spinlock_lock(&mtx->lock);

		mtx->owner = avm->getpid();
		mtx->recursion = 0;
	} else if(mtx->kind == AVM_MTX_KIND_ERRORCHECK)	
		return J_ERR;
	

	if(mtx->kind == AVM_MTX_KIND_RECURSIVE)
		mtx->recursion += 1;

	return J_OK;
}

int avm_mutex_trylock(avm_mutex_t* mtx) {
	if(unlikely(!mtx))
		return J_ERR;

	if(mtx->owner != avm->getpid()) {
		if(avm_spinlock_trylock(&mtx->lock) == J_ERR)
			return J_ERR;

		mtx->owner = avm->getpid();
		mtx->recursion = 0;
	} else if(mtx->kind == AVM_MTX_KIND_ERRORCHECK)
		return J_ERR;

	if(mtx->kind == AVM_MTX_KIND_RECURSIVE)
		mtx->recursion += 1;

	return J_OK;
}

int avm_mutex_unlock(avm_mutex_t* mtx) {
	if(unlikely(!mtx))
		return J_ERR;

	if(mtx->owner == avm->getpid()) {
		if(mtx->kind == AVM_MTX_KIND_RECURSIVE)
			if(--(mtx->recursion))
				return J_OK;

	
		avm_spinlock_unlock(&mtx->lock);
		mtx->owner = -1;
	} else if(mtx->kind == AVM_MTX_KIND_ERRORCHECK)
		return J_ERR;

	return J_OK;
}

