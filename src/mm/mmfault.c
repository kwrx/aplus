#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/list.h>
#include <aplus/task.h>

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <errno.h>



int mmfault(void* address) {
	mmzone_t* mz = (mmzone_t*) mmzone_find(address);
	if(unlikely(!mz))
		return MM_ERROR;

	if(mz->handler)
		return mz->handler(address);

	return MM_ERROR;
}
