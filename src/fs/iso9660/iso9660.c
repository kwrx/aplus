#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/task.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>

#include "iso9660.h"

uint32_t iso9660_getroot(inode_t* dev) {
	iso9660_pvd_t* pvd = (iso9660_pvd_t*) kmalloc(ISO9660_VOLDESC_SIZE);
	memset(pvd, 0, ISO9660_VOLDESC_SIZE);

	dev->position = ISO9660_PVD;
	if(fs_read(dev, pvd, ISO9660_VOLDESC_SIZE) != ISO9660_VOLDESC_SIZE) {
		kfree(pvd);
		return 0;
	}

	uint32_t ret = (uint32_t) pvd->rootdir;
	kfree(pvd);

	return ret;
}


int iso9660_check(inode_t* dev) {
	iso9660_pvd_t* pvd = (iso9660_pvd_t*) kmalloc(ISO9660_VOLDESC_SIZE);
	memset(pvd, 0, ISO9660_VOLDESC_SIZE);

	dev->position = ISO9660_PVD;
	if(fs_read(dev, pvd, ISO9660_VOLDESC_SIZE) != ISO9660_VOLDESC_SIZE) {
		kfree(pvd);
		return 0;
	}

	int ret = strncmp(pvd->id, ISO9660_ID, 5);
	kfree(pvd);

	return ret;
}

void iso9660_checkname(char* name) {
	char* p = strchr(name, ';');
	if(p) {
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
