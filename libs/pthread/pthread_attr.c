
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "pthread_internal.h"


#define STACK_DEFAULT_SIZE			PTHREAD_STACK_MIN

PUBLIC int pthread_attr_init(pthread_attr_t* attr) {
	if(!attr) {
		errno = EINVAL;
		return -1;
	}

	attr->stackaddr = 0;
	attr->stacksize = PTHREAD_STACK_MIN;
	attr->detachstate = 0;
	attr->param.sched_priority = 0;
	attr->inheritsched = 0;
	attr->contentionscope = 0;
	
	return 0;
}

PUBLIC int pthread_attr_destroy(pthread_attr_t* attr) {
	if(!attr) {
		errno = EINVAL;
		return -1;
	}

	memset(attr, 0, sizeof(pthread_attr_t));
	return 0;
}

PUBLIC int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* detachstate) {
	if(!attr) {
		errno = EINVAL;
		return -1;
	}

	*detachstate = attr->detachstate;
	return 0;
}

PUBLIC int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate) {
	if(!attr) {
		errno = EINVAL;
		return -1;
	}

	attr->detachstate = detachstate;
	return 0;
}

PUBLIC int pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr) {
	if(!attr) {
		errno = EINVAL;
		return -1;
	}

	*stackaddr = attr->stackaddr;
	return 0;
}

PUBLIC int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr) {
	if(!attr) {
		errno = EINVAL;
		return -1;
	}

	attr->stackaddr = stackaddr;
	return 0;
}


PUBLIC int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize) {
	if(!attr) {
		errno = EINVAL;
		return -1;
	}

	*stacksize = attr->stacksize;
	return 0;
}

PUBLIC int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize) {
	if(!attr) {
		errno = EINVAL;
		return -1;
	}

	attr->stacksize = stacksize;
	return 0;
}


PUBLIC int pthread_attr_getschedparam(const pthread_attr_t *attr, struct sched_param *param) {
	if(!attr) {
		errno = EINVAL;
		return -1;
	}

	memcpy(param, &attr->param, sizeof(struct sched_param));
	return 0; 
}



PUBLIC int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param) {
	if(!attr) {
		errno = EINVAL;
		return -1;
	}

	memcpy(&attr->param, param, sizeof(struct sched_param));
	return 0; 
}


PUBLIC int pthread_attr_getschedpolicy(pthread_attr_t *attr, int *policy) {
	errno = ENOSYS;
	return -1;
}

PUBLIC int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy) {
	errno = ENOSYS;
	return -1;
}

PUBLIC int pthread_attr_getinheritsched(pthread_attr_t *attr, int *inheritsched) {
	if(!attr) {
		errno = EINVAL;
		return -1;
	}

	*inheritsched = attr->inheritsched;
	return 0;
}

PUBLIC int pthread_attr_setinheritsched(pthread_attr_t *attr, int inheritsched) {
	if(!attr) {
		errno = EINVAL;
		return -1;
	}

	attr->inheritsched = inheritsched;
	return 0;
}

PUBLIC int pthread_attr_getscope(const pthread_attr_t *attr, int *contentionscope) {
	if(!attr) {
		errno = EINVAL;
		return -1;
	}

	*contentionscope = attr->contentionscope;
	return 0;
}

PUBLIC int pthread_attr_setscope(pthread_attr_t *attr, int contentionscope) {
	if(!attr) {
		errno = EINVAL;
		return -1;
	}

	attr->contentionscope = contentionscope;
	return 0;
}
