#include <stdlib.h>
#include <stdio.h>
#include "dl.h"

void* (*__dl_realloc) (void* ptr, size_t size) = realloc;
int (*__dl_printf) (char* fmt, ...) = printf;

void* __dl_malloc(size_t size) {
	return __dl_realloc(NULL, size);
} 

void __dl_free(void* ptr) {
	__dl_realloc(ptr, 0);
}


