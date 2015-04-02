#ifdef __rpi__

#include <aplus.h>
#include <aplus/spinlock.h>
#include <stdint.h>


__weak int __sync_synchronize() {
	return;
}

__weak int __sync_bool_compare_and_swap_4(int* val, int oldval, int newval) {
	if(likely((*val) == oldval)) {
		(*val) = newval;
		return 1;
	}

	return 0;
}

__weak int __sync_lock_test_and_set_4(int* val, int newval) {
	register int tmp = (*val);
	(*val) = newval;

	return tmp;
}

#endif
