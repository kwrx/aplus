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

#ifndef _DEV_VIRTIO_VIRTIO_CONSOLE_H
#define _DEV_VIRTIO_VIRTIO_CONSOLE_H



// Features
#define VIRTIO_CONSOLE_F_SIZE           (1 << 0)
#define VIRTIO_CONSOLE_F_MULTIPORT      (1 << 1)
#define VIRTIO_CONSOLE_F_EMERG_WRITE    (1 << 2)

#define VIRTIO_CONSOLE_PORT_RX(p)       ((p) * 2 + 0)
#define VIRTIO_CONSOLE_PORT_TX(p)       ((p) * 2 + 1)



#ifndef __ASSEMBLY__


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <stdint.h>

__BEGIN_DECLS


__END_DECLS

#endif

#endif