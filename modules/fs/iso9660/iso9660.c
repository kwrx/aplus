#include <xdev.h>
#include <xdev/module.h>
#include <xdev/vfs.h>
#include <xdev/mm.h>
#include <xdev/debug.h>
#include <libc.h>

#include "iso9660.h"



void iso9660_checkname(char* name) {
	char* p = strchr(name, ';');
	if(likely(p)) {
		*p-- = 0;
	
		if(*p == '.')
			*p = 0;
	}

	for(int i = 0; i < strlen(name); i++)
		if(name[i] >= 'A' && name[i] <= 'Z')
			name[i] += 32;
}


uint16_t iso9660_getmsb16(uint32_t val) {
	return (val >> 16) & 0xFFFF;
}

uint32_t iso9660_getmsb32(uint64_t val) {
	return (val >> 32) & 0xFFFFFFFF;
}

uint16_t iso9660_getlsb16(uint32_t val) {
	return (val >> 0) & 0xFFFF;
}

uint32_t iso9660_getlsb32(uint64_t val) {
	return (val >> 0) & 0xFFFFFFFF;
}
