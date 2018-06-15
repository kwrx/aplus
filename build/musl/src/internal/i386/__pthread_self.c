#include "../pthread_impl.h"

struct pthread *__pthread_self() {
	struct pthread *self;
	__asm__ __volatile__ ("movl %%gs:0,%0" : "=r" (self));
	return self;
}
