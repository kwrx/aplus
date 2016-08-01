#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/ipc.h>

#if CONFIG_IPC

int mutex_init(mutex_t* mtx, long kind, const char* name) {
	if(unlikely(!mtx))
		return E_ERR;

	spinlock_init(&mtx->lock);
	mtx->recursion = 0;
	mtx->kind = kind;
	mtx->owner = -1;
	mtx->name = name;
	
	return E_OK;
}


int mutex_lock(mutex_t* mtx) {
	if(unlikely(!mtx))
		return E_ERR;

	if(mtx->owner != sys_getpid()) {
#if CONFIG_IPC_DEBUG
		if(likely(mtx->name))
			kprintf(LOG, "[%d] %s: LOCKED from %d\n", mtx->owner, mtx->name, sys_getpid());
#endif	
		spinlock_lock(&mtx->lock);

		mtx->owner = sys_getpid();
		mtx->recursion = 0;
	} else if(mtx->kind == MTX_KIND_ERRORCHECK) {
#if CONFIG_IPC_DEBUG
		if(likely(mtx->name))
			kprintf(ERROR, "[%d] %s: DEADLOCK from %d\n", mtx->owner, mtx->name, sys_getpid());
#endif
		return E_ERR;
	}

	if(mtx->kind == MTX_KIND_RECURSIVE)
		mtx->recursion += 1;

	return E_OK;
}

int mutex_trylock(mutex_t* mtx) {
	if(unlikely(!mtx))
		return E_ERR;

	if(mtx->owner != sys_getpid()) {
		if(spinlock_trylock(&mtx->lock) == E_ERR)
			return E_ERR;

		mtx->owner = sys_getpid();
		mtx->recursion = 0;
	} else if(mtx->kind == MTX_KIND_ERRORCHECK)
		return E_ERR;

	if(mtx->kind == MTX_KIND_RECURSIVE)
		mtx->recursion += 1;

	return E_OK;
}

int mutex_unlock(mutex_t* mtx) {
	if(unlikely(!mtx))
		return E_ERR;

	if(mtx->owner == sys_getpid()) {
		if(mtx->kind == MTX_KIND_RECURSIVE)
			if(--(mtx->recursion))
				return E_OK;

#if CONFIG_IPC_DEBUG
		if(likely(mtx->name))
			kprintf(LOG, "[%d] %s: UNLOCKED from %d\n", mtx->owner, mtx->name, sys_getpid());
#endif
	
		spinlock_unlock(&mtx->lock);
		mtx->owner = -1;
	} else if(mtx->kind == MTX_KIND_ERRORCHECK)
		return E_ERR;

	return E_OK;
}

EXPORT(mutex_init);
EXPORT(mutex_lock);
EXPORT(mutex_trylock);
EXPORT(mutex_unlock);

#endif
