/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/endian.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/vfs.h>

#include "iso9660.h"



iso9660_inode_t* iso9660_cache_fetch(cache_t* cache, iso9660_t* iso9660, size_t position) {

    (void)cache;

    DEBUG_ASSERT(iso9660);
    DEBUG_ASSERT(iso9660->dev);



    iso9660_inode_t inode = {0};

    if (unlikely(vfs_read(iso9660->dev, &inode.record, position, sizeof(iso9660_directory_record_t)) != sizeof(iso9660_directory_record_t)))
        return NULL;


    DEBUG_ASSERT(inode.record.length >= sizeof(iso9660_directory_record_t));



    inode.st.st_blksize = iso9660->block_size;
    inode.st.st_blocks  = le32_to_cpu(inode.record.data_length.lsb) / iso9660->block_size;
    inode.st.st_size    = le32_to_cpu(inode.record.data_length.lsb);



    size_t limit  = position + inode.record.length;
    size_t offset = position + sizeof(iso9660_directory_record_t) + inode.record.length_of_file_identifier + !(inode.record.length_of_file_identifier & 1);

    if (limit > offset) {

        do {

            iso9660_rockridge_entry_t rr = {0};

            if (vfs_read(iso9660->dev, &rr, offset, 4) != 4)
                return NULL;


            offset += 4;

            if (rr.name[0] == '\0') {

                break;

            } else if (memcmp(rr.name, ISO9660_ROCKRIDGE_ENTRY_PX, 2) == 0) {

                if (vfs_read(iso9660->dev, &rr.px, offset, sizeof(rr.px)) != sizeof(rr.px))
                    return NULL;


                inode.st.st_uid   = rr.px.uid;
                inode.st.st_gid   = rr.px.gid;
                inode.st.st_ino   = rr.px.ino;
                inode.st.st_mode  = rr.px.mode;
                inode.st.st_nlink = rr.px.nlinks;

            }

            else if (memcmp(rr.name, ISO9660_ROCKRIDGE_ENTRY_NM, 2) == 0) {

                if (vfs_read(iso9660->dev, &rr.nm, offset, sizeof(rr.nm)) != sizeof(rr.nm))
                    return NULL;


                if (rr.nm.flags & (1 << 1)) {

                    inode.name[0] = '.';
                    inode.name[1] = '\0';

                } else if (rr.nm.flags & (1 << 2)) {

                    inode.name[0] = '.';
                    inode.name[1] = '.';
                    inode.name[2] = '\0';

                } else {

                    char buf[ISO9660_MAX_NAME + 1] = {0};

                    if (vfs_read(iso9660->dev, buf, offset + sizeof(rr.nm), rr.length - sizeof(rr.nm) - 4) != rr.length - sizeof(rr.nm) - 4)
                        return NULL;

                    strlcat(inode.name, buf, ISO9660_MAX_NAME);
                }

            } else if (memcmp(rr.name, ISO9660_ROCKRIDGE_ENTRY_SL, 2) == 0) {

                if (vfs_read(iso9660->dev, &rr.sl, offset, sizeof(rr.sl)) != sizeof(rr.sl))
                    return NULL;


                for (size_t p = 0; p < rr.length - sizeof(rr.sl) - 4;) {

                    struct {

                        uint8_t flags;
                        uint8_t length;

                    } __packed sl_component = {0};


                    if (vfs_read(iso9660->dev, &sl_component, offset + sizeof(rr.sl) + p, sizeof(sl_component)) != sizeof(sl_component))
                        return NULL;

                    p += sizeof(sl_component);


                    if (sl_component.flags & (1 << 1)) {

                        strlcat(inode.symlink, ".", ISO9660_MAX_SYMLINK);

                    } else if (sl_component.flags & (1 << 2)) {

                        strlcat(inode.symlink, "..", ISO9660_MAX_SYMLINK);

                    } else if (sl_component.flags & (1 << 3)) {

                        strlcat(inode.symlink, "/", ISO9660_MAX_SYMLINK);

                    } else {

                        char buf[ISO9660_MAX_NAME + 1] = {0};

                        if (vfs_read(iso9660->dev, buf, offset + sizeof(rr.sl) + p, sl_component.length) != sl_component.length)
                            return NULL;


                        if (inode.symlink[0] != '\0' && inode.symlink[strlen(inode.symlink) - 1] != '/')
                            strlcat(inode.symlink, "/", ISO9660_MAX_SYMLINK);

                        strlcat(inode.symlink, buf, ISO9660_MAX_SYMLINK);
                    }


                    p += sl_component.length;
                }
            }

            offset += rr.length - 4;

        } while (offset < limit);
    }


    if (inode.name[0] == '\0') {

        char buf[ISO9660_MAX_NAME + 1] = {0};

        if (inode.record.length_of_file_identifier > 0) {

            if (vfs_read(iso9660->dev, buf, position + sizeof(iso9660_directory_record_t), inode.record.length_of_file_identifier) != inode.record.length_of_file_identifier)
                return NULL;


            if (buf[0] == '\0') {

                inode.name[0] = '.';
                inode.name[1] = '\0';


            } else if (buf[0] == '\1') {

                inode.name[0] = '.';
                inode.name[1] = '.';
                inode.name[2] = '\0';

            } else {

                for (size_t i = inode.record.length_of_file_identifier; i > 0; i--) {

                    if (buf[i] == ';') {
                        buf[i] = '\0';
                    }
                }

                strlcat(inode.name, buf, ISO9660_MAX_NAME);
            }
        }
    }


    if (inode.st.st_ino == 0) {

        inode.st.st_uid   = 0;
        inode.st.st_gid   = 0;
        inode.st.st_nlink = 0;
        inode.st.st_ino   = ++iso9660->next_ino;
        inode.st.st_mode  = ((inode.record.file_flags & (1 << 1)) ? S_IFDIR : S_IFREG) | 0444;
    }


    if (S_ISLNK(inode.st.st_mode)) {

        inode.st.st_size = strlen(inode.symlink);
    }



    iso9660_inode_t* r = (iso9660_inode_t*)kmalloc(sizeof(iso9660_inode_t), GFP_USER);

    if (unlikely(!r))
        return NULL;

    memcpy(r, &inode, sizeof(iso9660_inode_t));

    return r;
}


void iso9660_cache_commit(cache_t* cache, iso9660_t* iso9660, size_t position, iso9660_inode_t* inode) {

    (void)cache;

    DEBUG_ASSERT(iso9660);
    DEBUG_ASSERT(inode);
}


void iso9660_cache_release(cache_t* cache, iso9660_t* iso9660, size_t position, iso9660_inode_t* inode) {

    (void)cache;

    DEBUG_ASSERT(iso9660);
    DEBUG_ASSERT(inode);

    kfree(inode);
}
