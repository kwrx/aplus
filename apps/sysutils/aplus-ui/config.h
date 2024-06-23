/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2022 Antonino Natale
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

#pragma once


#define APLUS_UI_CONFIG_MAX_SCREENS 16

#define APLUS_UI_RESOURCES_PATH_CURSORS    "/usr/share/cursors"
#define APLUS_UI_RESOURCES_PATH_FONTS      "/usr/share/fonts"
#define APLUS_UI_RESOURCES_PATH_ICONS      "/usr/share/icons"
#define APLUS_UI_RESOURCES_PATH_THEMES     "/usr/share/themes"
#define APLUS_UI_RESOURCES_PATH_WALLPAPERS "/usr/share/wallpapers"


#include <stdio.h>

#define LOG(a...) fprintf(stderr, "aplus-ui::" __FILE__ ": " a)
