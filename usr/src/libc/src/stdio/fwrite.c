#include "stdio.h"


size_t fwrite(const void* ptr, size_t size, size_t count, FILE* fp) {
	if(unlikely(!ptr || !fp))
		STDIO_ERROR(fp, EINVAL);

	if(unlikely(!STDIO_CAN_WRITE(fp)))
		STDIO_ERROR(fp, EINVAL);

	if(unlikely(size * count == 0))
		return 0;

	STDIO_LOCK(fp);

	int r = 0;
	int s = size * count;
	while((r = STDIO_FILE(fp)->write(STDIO_FILE(fp), ptr, s)) < s) {
		if(errno != EINTR) {
			STDIO_FILE(fp)->eof = EOF;
			break;
		}
	}

	STDIO_UNLOCK(fp);

	return r;
}


