/*
 * Author(s):
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

#ifndef _APLUS_X86_SMP_H
#define _APLUS_X86_SMP_H

#ifndef __ASSEMBLY__

    #include <aplus.h>
    #include <aplus/debug.h>


typedef struct {
        uint64_t magic;
        uint64_t cpu;
        uint64_t cr3;
        uint64_t stack;
} __packed ap_header_t;


__BEGIN_DECLS

void ap_main(void);
void ap_init(void);
int ap_check(int, int);
ap_header_t* ap_get_header(void);

__END_DECLS

#endif
#endif
