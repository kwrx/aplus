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

#ifndef _APLUS_LAUNCHER_DESKTOP_H
#define _APLUS_LAUNCHER_DESKTOP_H

#include <limits.h>
#include <stddef.h>


/**
 * @brief How long a name, a comment or a command line may be before it is truncated.
 */
#define LAUNCHER_FIELD_MAX 128


/**
 * @brief What a .desktop file says about an application, as far as showing and picking it goes.
 *
 * Running it is aplus-xopen's business, which is why the command is kept for display only
 * and never taken apart here.
 */

typedef struct {

    //? The .desktop file itself, which is what gets handed to the opener.
    char path[PATH_MAX];

    char name[LAUNCHER_FIELD_MAX];
    char comment[LAUNCHER_FIELD_MAX];
    char exec[LAUNCHER_FIELD_MAX];

    //? What the Icon key said, which is a theme name rather than a file unless it carries
    //? a '/'. Left empty by a file that named none, for the caller to fall back from.
    char icon[LAUNCHER_FIELD_MAX];

} launcher_entry_t;


/**
 * @brief Reads every application a directory describes, sorted by name.
 *
 * Anything that is not a `Type=Application`, and anything marked `NoDisplay`, is left out.
 *
 * @param path The directory to read.
 * @param out Receives the entries, allocated for the caller to free.
 * @return How many there are, which is 0 when there are none or the directory cannot be read.
 */
size_t launcher_scan(const char* path, launcher_entry_t** out);

#endif
