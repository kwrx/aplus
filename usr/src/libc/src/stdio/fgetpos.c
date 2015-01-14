#include "stdio.h"

int fgetpos(FILE* fp, fpos_t* pos) {
	if(unlikely(!pos))
		STDIO_ERROR(fp, EINVAL);

	pos->__pos = ftell(fp);
	return 0;
}
