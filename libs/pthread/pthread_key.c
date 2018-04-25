#include "pthread_internal.h"


static struct pkey {
    int is_initialized;
    void* value;
    void (*dtor) (void*);
} pkeys[PKEY_MAX];


static pthread_key_t __alloc_pkey() {
    pthread_key_t k;
    for(k = 0; k < PKEY_MAX; k++)
        if(!pkeys[k].is_initialized)
            return k;

    return 0xFFFFFFFF;
}

int pthread_once(pthread_once_t* once, void (*init) (void)) {
    if(!once) {
        errno = EINVAL;
        return -1;
    }

    if(!once->is_initialized) {
        errno = EINVAL;
        return -1;
    }

    if(!once->init_executed)
        init();

    once->init_executed = 1;
    return 0;
}

int	pthread_key_create(pthread_key_t* __key, void (*__destructor)(void *)) {
    if(!__key) {
        errno = EINVAL;
        return -1;
    }

    pthread_key_t k = __alloc_pkey();
    if(k == 0xFFFFFFFF) {
        errno = EAGAIN;
        return -1;
    }

    pkeys[k].is_initialized = 1;
    pkeys[k].dtor = __destructor;

    *__key = k;
    return 0;
}

int	pthread_setspecific (pthread_key_t __key, const void *__value) {
    if(__key < 0 || __key > PKEY_MAX) {
        errno = EINVAL;
        return -1;
    }

    if(!pkeys[__key].is_initialized) {
        errno = EINVAL;
        return -1;
    }

    pkeys[__key].value = (void*) __value;
    return 0;
}

void* pthread_getspecific (pthread_key_t __key) {
    if(__key < 0 || __key > PKEY_MAX) {
        errno = EINVAL;
        return NULL;
    }

    if(!pkeys[__key].is_initialized) {
        errno = EINVAL;
        return NULL;
    }

    return pkeys[__key].value;
}

int	pthread_key_delete (pthread_key_t __key) {
    if(__key < 0 || __key > PKEY_MAX) {
        errno = EINVAL;
        return -1;
    }

    if(!pkeys[__key].is_initialized) {
        errno = EINVAL;
        return -1;
    }

    if(pkeys[__key].dtor)
        pkeys[__key].dtor(pkeys[__key].value);

    pkeys[__key].is_initialized = 0;
    return 0;
}