/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _TMPFS_H
#define _TMPFS_H


int tmpfs_mount(struct inode* dev, struct inode* dir, struct mountinfo* info);

struct inode* tmpfs_mknod(struct inode* inode, char* name, mode_t mode);
int tmpfs_unlink(struct inode* inode, char* name);

int tmpfs_write(struct inode* inode, void* ptr, off_t pos, size_t len);
int tmpfs_read(struct inode* inode, void* ptr, off_t pos, size_t len);

#endif
