
#include <stdint.h>
#include <pthread.h>

#include "pthread_internal.h"

PRIVATE __pthread_key_t __pthread_keys[PTHREAD_KEYS_MAX] = { };

PUBLIC int pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
	if(!key) {
		errno = EINVAL;
		return -1;
	}

	int i;
	for(i = 0; i < PTHREAD_KEYS_MAX; i++) {
		if(__pthread_keys[i].used == 0) {
			__pthread_keys[i].used = 1;
			__pthread_keys[i].dtor = destructor;
			__pthread_keys[i].value = NULL;

			*key = i;
			return 0;
		}
	}

	errno = EAGAIN;
	return -1;
}


PUBLIC int pthread_key_delete(pthread_key_t key) {
	if(key > PTHREAD_KEYS_MAX) {
		errno = EINVAL;
		return -1;
	}

	if(__pthread_keys[key].dtor)
		__pthread_keys[key].dtor(__pthread_keys[key].value);

	__pthread_keys[key].used = 0;
	__pthread_keys[key].dtor = 0;
	__pthread_keys[key].value = NULL;
	
	return 0;
}


PUBLIC int pthread_setspecific(pthread_key_t key, const void *value) {
	if(key > PTHREAD_KEYS_MAX) {
		errno = EINVAL;
		return -1;
	}

	if(__pthread_keys[key].used == 0) {
		errno = EINVAL;
		return -1;
	}

	__pthread_keys[key].value = (void*) value;
	return 0;
}


PUBLIC void *pthread_getspecific(pthread_key_t key) {
	if(key > PTHREAD_KEYS_MAX) {
		errno = EINVAL;
		return 0;
	}

	if(__pthread_keys[key].used == 0) {
		errno = EINVAL;
		return 0;
	}

	return __pthread_keys[key].value;
}
