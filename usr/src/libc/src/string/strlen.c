#include "string.h"

size_t strlen(const char* str) {
	register size_t x = 0;
	while(*str++)
		x++;
	return x;
}
