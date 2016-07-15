#ifndef _IPC_H
#define _IPC_H





#define MTX_KIND_DEFAULT			0
#define MTX_KIND_ERRORCHECK			1
#define MTX_KIND_RECURSIVE			2


#ifndef __ASSEMBLY__

#define MTX_INIT(t)					\
	{								\
		0,							\
		0,							\
		t,							\
		-1							\
	}

typedef volatile long spinlock_t;

typedef struct {
	spinlock_t lock;
	long recursion;
	long kind;
	long owner;
} __packed mutex_t;





#if CONFIG_IPC
int spinlock_init(spinlock_t* lock);
void spinlock_lock(spinlock_t* lock);
int spinlock_trylock(spinlock_t* lock);
void spinlock_unlock(spinlock_t* lock);

int mutex_init(mutex_t* mtx, long kind);
int mutex_lock(mutex_t* mtx);
int mutex_trylock(mutex_t* mtx);
int mutex_unlock(mutex_t* mtx);
#elif !CONFIG_IPC
#define spinlock_init(x)
#define spinlock_lock(x)
#define spinlock_trylock(x)
#define spinlock_unlock(x)

#define mutex_init(x, y)
#define mutex_lock(x)
#define mutex_trylock(x)
#define mutex_unlock(x)
#endif


#endif

#endif
