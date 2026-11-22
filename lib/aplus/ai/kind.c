/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2026 Antonino Natale
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

#include <string.h>
#include <strings.h>

#include <aplus/ai.h>


static const struct {

    const char* extension;
    ai_kind_t kind;

} ai_kinds[] = {

    {".png",  AI_KIND_IMAGE},
    {".jpg",  AI_KIND_IMAGE},
    {".jpeg", AI_KIND_IMAGE},
    {".jpe",  AI_KIND_IMAGE},
    {".jfif", AI_KIND_IMAGE},
    {".bmp",  AI_KIND_IMAGE},
    {".gif",  AI_KIND_IMAGE},
    {".tiff", AI_KIND_IMAGE},
    {".tif",  AI_KIND_IMAGE},
    {".pnm",  AI_KIND_IMAGE},
    {".pbm",  AI_KIND_IMAGE},
    {".pgm",  AI_KIND_IMAGE},
    {".ppm",  AI_KIND_IMAGE},
    {".pam",  AI_KIND_IMAGE},
    {".webp", AI_KIND_IMAGE},

    {".wav",  AI_KIND_AUDIO},
    {".mp3",  AI_KIND_AUDIO},
    {".ogg",  AI_KIND_AUDIO},
    {".flac", AI_KIND_AUDIO},
    {".oga",  AI_KIND_AUDIO},
    {".opus", AI_KIND_AUDIO},
    {".m4a",  AI_KIND_AUDIO},
    {".aac",  AI_KIND_AUDIO},
    {".aif",  AI_KIND_AUDIO},
    {".aiff", AI_KIND_AUDIO},
    {".ape",  AI_KIND_AUDIO},
    {".wv",   AI_KIND_AUDIO},
    {".wma",  AI_KIND_AUDIO},
    {".mka",  AI_KIND_AUDIO},

    {".mp4",  AI_KIND_VIDEO},
    {".m4v",  AI_KIND_VIDEO},
    {".avi",  AI_KIND_VIDEO},
    {".mkv",  AI_KIND_VIDEO},
    {".webm", AI_KIND_VIDEO},
    {".mov",  AI_KIND_VIDEO},
    {".mpg",  AI_KIND_VIDEO},
    {".mpeg", AI_KIND_VIDEO},
    {".ogv",  AI_KIND_VIDEO},
    {".flv",  AI_KIND_VIDEO},
    {".wmv",  AI_KIND_VIDEO},

    {".pdf",  AI_KIND_DOCUMENT},
};


ai_kind_t ai_kind(const char* path) {

    const char* slash = strrchr(path, '/');
    const char* name  = slash ? slash + 1 : path;
    const char* dot   = strrchr(name, '.');

    if (!dot || dot == name) {
        return AI_KIND_TEXT;
    }

    for (size_t i = 0; i < sizeof(ai_kinds) / sizeof(ai_kinds[0]); i++) {

        if (strcasecmp(dot, ai_kinds[i].extension) == 0) {
            return ai_kinds[i].kind;
        }
    }

    return AI_KIND_TEXT;
}
