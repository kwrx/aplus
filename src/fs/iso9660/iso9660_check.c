
//
//  iso9660_check.c
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.


#include <aplus.h>
#include <aplus/vfs.h>

#include "iso9660.h"

int iso9660_check(inode_t* dev) {
	iso9660_pvd_t* pvd = kmalloc(ISO9660_VOLDESC_SIZE);
	memset(pvd, 0, ISO9660_VOLDESC_SIZE);
	
	dev->position = ISO9660_PVD;
	if(fs_read(dev, ISO9660_VOLDESC_SIZE, pvd) != ISO9660_VOLDESC_SIZE) {
		kfree(pvd);
		return -1;
	}
		
	int ret = strncmp(pvd->id, ISO9660_ID, 5);
	
	kfree(pvd);
	return ret;
}