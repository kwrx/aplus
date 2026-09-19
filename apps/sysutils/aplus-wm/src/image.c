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

/**
 * @brief Reading image files, which the cursor theme and the desktop picture both go through.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <webp/decode.h>

#include <wm.h>


/**
 * @brief Reads a whole file into memory, which is how the decoder wants the image.
 *
 * @param path The file to read.
 * @param size Receives the size of the file.
 * @return The contents, or NULL with errno set.
 */
void* wm_slurp(const char* path, size_t* size) {

    int fd;

    if ((fd = open(path, O_RDONLY)) < 0) {
        fprintf(stderr, "aplus-wm: warning: cannot open %s: %s\n", path, strerror(errno));
        return NULL;
    }


    struct stat st;

    if (fstat(fd, &st) < 0 || st.st_size <= 0) {
        fprintf(stderr, "aplus-wm: warning: cannot stat %s\n", path);
        close(fd);
        return NULL;
    }


    uint8_t* buffer = malloc((size_t)st.st_size);

    if (!buffer) {
        close(fd);
        return NULL;
    }


    if (ui_recv_all(fd, buffer, (size_t)st.st_size) < 0) {

        fprintf(stderr, "aplus-wm: warning: cannot read %s: %s\n", path, strerror(errno));

        free(buffer);
        close(fd);

        return NULL;
    }

    close(fd);

    *size = (size_t)st.st_size;

    return buffer;
}


/**
 * @brief Reads a webp file and reports how big the image in it is, without decoding it.
 *
 * @param path The file to read.
 * @param max_size The largest edge the caller has room for.
 * @param size Receives the size of the file.
 * @param width Receives the width of the image.
 * @param height Receives the height of the image.
 * @return The file contents, for the caller to decode and then free, or NULL.
 */
void* wm_image_probe(const char* path, int max_size, size_t* size, int* width, int* height) {

    void* file = wm_slurp(path, size);

    if (!file) {
        return NULL;
    }


    if (!WebPGetInfo(file, *size, width, height)) {
        fprintf(stderr, "aplus-wm: warning: %s is not a webp image\n", path);
        free(file);
        return NULL;
    }

    if (*width <= 0 || *height <= 0 || *width > max_size || *height > max_size) {
        fprintf(stderr, "aplus-wm: warning: %s is %dx%d, which is not a usable size\n", path, *width, *height);
        free(file);
        return NULL;
    }

    return file;
}
