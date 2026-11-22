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

#ifndef _APLUS_AI_H
#define _APLUS_AI_H

#include <stddef.h>

#define AI_ERROR_MAX 256

#define AI_RESPONSE_MAX_DEFAULT (1024 * 1024)

typedef enum {

    AI_KIND_TEXT,
    AI_KIND_IMAGE,
    AI_KIND_AUDIO,
    AI_KIND_VIDEO,
    AI_KIND_DOCUMENT,

} ai_kind_t;

typedef enum {

    AI_PART_IMAGE = 1,
    AI_PART_AUDIO,

} ai_part_type_t;

typedef struct {

    ai_part_type_t type;
    const char* format;
    const void* data;
    size_t size;

} ai_part_t;

typedef struct {

    const char* role;
    const char* content;

    const ai_part_t* parts;
    size_t count;

} ai_message_t;

typedef void (*ai_stream_t)(const char* text, size_t size, void* user);
typedef void (*ai_progress_t)(size_t done, size_t total, void* user);

typedef struct {

    const char* url;
    const char* key;
    const char* model;
    const char* agent;
    const char* temperature;
    const char* extras;

    const ai_message_t* messages;
    size_t count;

    int tokens;

    long timeout;
    long timeout_connect;

    size_t response_max;

    ai_stream_t stream;
    ai_stream_t reasoning;
    ai_progress_t progress;
    void* user;

} ai_request_t;

typedef struct {

    long status;
    char* content;
    size_t size;

    char error[AI_ERROR_MAX];

} ai_reply_t;

int ai_init(void);
int ai_chat(const ai_request_t* request, ai_reply_t* reply);
void ai_reply_release(ai_reply_t* reply);
ai_kind_t ai_kind(const char* path);

#endif
