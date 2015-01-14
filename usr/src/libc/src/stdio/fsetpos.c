#include "stdio.h"

int fsetpos(FILE* fp, const fpos_t* pos) {
	if(unlikely(!pos))
		STDIO_ERROR(fp, EINVAL);

	return fseek(fp, (long int) pos->__pos, SEEK_SET);
}
