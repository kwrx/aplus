#ifndef _FSYS_H
#define _FSYS_H

#include <aplus.h>
#include <aplus/attribute.h>

typedef struct fsys {
	char name[255];
	int (*mount) (struct inode* idev, struct inode* idir, int flags);
} fsys_t;

#define FSYS(name, mount)													\
	static fsys_t fs_##name = {												\
		#name, mount														\
	}; ATTRIBUTE("fs", fs_##name)

#endif
