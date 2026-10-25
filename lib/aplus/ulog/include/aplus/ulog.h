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

#ifndef _APLUS_ULOG_H
#define _APLUS_ULOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ulog_level {
    ULOG_ERROR,
    ULOG_WARNING,
    ULOG_INFO,
    ULOG_DEBUG,
} ulog_level_t;

int ulog_init(const char* path, const ulog_level_t level);

__attribute__((format(printf, 2, 3)))
int ulogf(const ulog_level_t level, const char* fmt, ...);

#define ulog_errorf(fmt, a...)  ulogf(ULOG_ERROR, fmt, ##a)
#define ulog_warnf(fmt, a...)   ulogf(ULOG_WARNING, fmt, ##a)
#define ulog_infof(fmt, a...)   ulogf(ULOG_INFO, fmt, ##a)
#define ulog_debugf(fmt, a...)  ulogf(ULOG_DEBUG, fmt, ##a)

#ifdef __cplusplus
}
#endif

#endif
