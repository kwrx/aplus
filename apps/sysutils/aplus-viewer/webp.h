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

#ifndef _APLUS_VIEWER_WEBP_H
#define _APLUS_VIEWER_WEBP_H

#include <stdbool.h>

#include <mupdf/fitz.h>


/**
 * @brief How much of the head of a file tells a WebP picture apart.
 */
#define VIEWER_WEBP_HEAD 12


/**
 * @brief The mupdf document handler that opens a WebP picture as a one-page document, one point to a pixel.
 */
extern const fz_document_handler viewer_webp_handler;


/**
 * @brief Reports whether the head of a file is the header of a WebP picture.
 *
 * @param head The first VIEWER_WEBP_HEAD bytes of the file.
 * @return true when it is.
 */
bool viewer_webp_recognize(const unsigned char head[VIEWER_WEBP_HEAD]);

#endif
