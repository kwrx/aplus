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

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include <aplus/ai-json.h>
#include <aplus/ai.h>

#define AI_AGENT_DEFAULT "libai"
#define AI_BUFFER_MIN    4096


typedef struct {

    char* data;
    size_t size;
    size_t max;
    bool failed;

} ai_buffer_t;


typedef struct {

    CURL* curl;

    const ai_request_t* request;
    ai_reply_t* reply;

    ai_buffer_t body;
    ai_buffer_t line;
    ai_buffer_t scratch;
    ai_buffer_t content;

    size_t limit;
    bool overflow;

} ai_call_t;


static pthread_once_t ai_once = PTHREAD_ONCE_INIT;

static int ai_ready = -1;


static bool ai_buffer_reserve(ai_buffer_t* buffer, size_t extra) {

    if (buffer->failed) {
        return false;
    }

    if (buffer->size + extra + 1 <= buffer->max) {
        return true;
    }

    size_t max = buffer->max ? buffer->max : AI_BUFFER_MIN;

    while (max < buffer->size + extra + 1) {
        max *= 2;
    }

    char* data = realloc(buffer->data, max);

    if (!data) {

        buffer->failed = true;
        return false;
    }

    buffer->data = data;
    buffer->max  = max;

    return true;
}


static void ai_buffer_append(ai_buffer_t* buffer, const char* data, size_t size) {

    if (!ai_buffer_reserve(buffer, size)) {
        return;
    }

    memcpy(buffer->data + buffer->size, data, size);

    buffer->size += size;
    buffer->data[buffer->size] = '\0';
}


static void ai_buffer_put(ai_buffer_t* buffer, const char* text) {

    ai_buffer_append(buffer, text, strlen(text));
}


static void ai_buffer_escaped(ai_buffer_t* buffer, const char* text) {

    if (!text) {
        return;
    }

    const char* run = text;

    for (const char* p = text; *p; p++) {

        const unsigned char c = (unsigned char)*p;

        if (c >= 0x20 && c != '"' && c != '\\') {
            continue;
        }

        ai_buffer_append(buffer, run, (size_t)(p - run));

        run = p + 1;

        switch (c) {

            case '"':
                ai_buffer_put(buffer, "\\\"");
                break;

            case '\\':
                ai_buffer_put(buffer, "\\\\");
                break;

            case '\n':
                ai_buffer_put(buffer, "\\n");
                break;

            case '\r':
                ai_buffer_put(buffer, "\\r");
                break;

            case '\t':
                ai_buffer_put(buffer, "\\t");
                break;

            default: {

                char seq[8];

                snprintf(seq, sizeof(seq), "\\u%04x", c);
                ai_buffer_put(buffer, seq);

                break;
            }
        }
    }

    ai_buffer_put(buffer, run);
}


static void ai_buffer_base64(ai_buffer_t* buffer, const void* data, size_t size) {

    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    if (!ai_buffer_reserve(buffer, (size + 2) / 3 * 4)) {
        return;
    }

    const unsigned char* in = data;

    char* out = buffer->data + buffer->size;

    size_t i = 0;

    for (; i + 2 < size; i += 3) {

        const unsigned long v = ((unsigned long)in[i] << 16) | ((unsigned long)in[i + 1] << 8) | in[i + 2];

        *out++ = alphabet[(v >> 18) & 63];
        *out++ = alphabet[(v >> 12) & 63];
        *out++ = alphabet[(v >> 6) & 63];
        *out++ = alphabet[v & 63];
    }

    if (i < size) {

        const unsigned long v = ((unsigned long)in[i] << 16) | (i + 1 < size ? (unsigned long)in[i + 1] << 8 : 0);

        *out++ = alphabet[(v >> 18) & 63];
        *out++ = alphabet[(v >> 12) & 63];
        *out++ = i + 1 < size ? alphabet[(v >> 6) & 63] : '=';
        *out++ = '=';
    }

    buffer->size = (size_t)(out - buffer->data);
    buffer->data[buffer->size] = '\0';
}


static void ai_buffer_clear(ai_buffer_t* buffer) {

    buffer->size = 0;

    if (buffer->data) {
        buffer->data[0] = '\0';
    }
}


static void ai_compose_content(ai_buffer_t* payload, const ai_message_t* message) {

    if (!message->parts || message->count == 0) {

        ai_buffer_put(payload, "\"");
        ai_buffer_escaped(payload, message->content);
        ai_buffer_put(payload, "\"");

        return;
    }

    ai_buffer_put(payload, "[");

    for (size_t i = 0; i < message->count; i++) {

        const ai_part_t* part = &message->parts[i];

        if (i > 0) {
            ai_buffer_put(payload, ",");
        }

        if (part->type == AI_PART_AUDIO) {

            ai_buffer_put(payload, "{\"type\":\"input_audio\",\"input_audio\":{\"data\":\"");
            ai_buffer_base64(payload, part->data, part->size);
            ai_buffer_put(payload, "\",\"format\":\"");
            ai_buffer_escaped(payload, part->format ? part->format : "wav");
            ai_buffer_put(payload, "\"}}");

        } else {

            ai_buffer_put(payload, "{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:");
            ai_buffer_escaped(payload, part->format ? part->format : "image/jpeg");
            ai_buffer_put(payload, ";base64,");
            ai_buffer_base64(payload, part->data, part->size);
            ai_buffer_put(payload, "\"}}");
        }
    }

    if (message->content && *message->content) {

        ai_buffer_put(payload, ",{\"type\":\"text\",\"text\":\"");
        ai_buffer_escaped(payload, message->content);
        ai_buffer_put(payload, "\"}");
    }

    ai_buffer_put(payload, "]");
}


static void ai_compose(ai_buffer_t* payload, const ai_request_t* request, bool extras) {

    ai_buffer_clear(payload);

    ai_buffer_put(payload, "{\"model\":\"");
    ai_buffer_escaped(payload, request->model);
    ai_buffer_put(payload, "\"");

    if (request->temperature && *request->temperature) {

        ai_buffer_put(payload, ",\"temperature\":");
        ai_buffer_put(payload, request->temperature);
    }

    if (request->tokens > 0) {

        char tokens[32];

        snprintf(tokens, sizeof(tokens), ",\"max_tokens\":%d", request->tokens);
        ai_buffer_put(payload, tokens);
    }

    if (request->stream) {
        ai_buffer_put(payload, ",\"stream\":true");
    }

    if (request->stream && request->progress) {
        ai_buffer_put(payload, ",\"return_progress\":true");
    }

    if (extras) {

        ai_buffer_put(payload, ",");
        ai_buffer_put(payload, request->extras);
    }

    ai_buffer_put(payload, ",\"messages\":[");

    for (size_t i = 0; i < request->count; i++) {

        ai_buffer_put(payload, i > 0 ? ",{\"role\":\"" : "{\"role\":\"");
        ai_buffer_escaped(payload, request->messages[i].role);
        ai_buffer_put(payload, "\",\"content\":");
        ai_compose_content(payload, &request->messages[i]);
        ai_buffer_put(payload, "}");
    }

    ai_buffer_put(payload, "]}");
}


static bool ai_failed(const char* json, char* out, size_t max) {

    const char* error = ai_json_member(json, "error");

    if (!error) {
        return false;
    }

    if (ai_json_string(error, out, max) > 0) {
        return true;
    }

    if (ai_json_string(ai_json_member(error, "message"), out, max) > 0) {
        return true;
    }

    snprintf(out, max, "The endpoint refused the request.");

    return true;
}


static size_t ai_decode(ai_call_t* call, const char* value) {

    if (!value) {
        return 0;
    }

    ai_buffer_clear(&call->scratch);

    if (!ai_buffer_reserve(&call->scratch, strlen(value))) {
        return 0;
    }

    return ai_json_string(value, call->scratch.data, call->scratch.max);
}


static void ai_take(ai_call_t* call, size_t size) {

    if (size == 0) {
        return;
    }

    if (call->content.size + size > call->limit) {

        call->overflow = true;
        return;
    }

    ai_buffer_append(&call->content, call->scratch.data, size);

    if (call->request->stream && !call->content.failed) {
        call->request->stream(call->scratch.data, size, call->request->user);
    }
}


static void ai_think(ai_call_t* call, size_t size) {

    if (size == 0 || !call->request->reasoning) {
        return;
    }

    call->request->reasoning(call->scratch.data, size, call->request->user);
}


static void ai_progress(ai_call_t* call, const char* progress) {

    if (!progress || !call->request->progress) {
        return;
    }

    const char* total     = ai_json_member(progress, "total");
    const char* processed = ai_json_member(progress, "processed");

    if (!total || !processed) {
        return;
    }

    const double done = strtod(processed, NULL);
    const double all  = strtod(total, NULL);

    if (done < 0 || all <= 0) {
        return;
    }

    call->request->progress((size_t)done, (size_t)all, call->request->user);
}


static void ai_event(ai_call_t* call, char* line) {

    const size_t length = strlen(line);

    if (length > 0 && line[length - 1] == '\r') {
        line[length - 1] = '\0';
    }

    if (strncmp(line, "data:", 5) != 0) {

        ai_buffer_put(&call->body, line);
        ai_buffer_put(&call->body, "\n");

        return;
    }

    const char* data = line + 5;

    while (*data == ' ') {
        data++;
    }

    if (strcmp(data, "[DONE]") == 0) {
        return;
    }

    if (ai_failed(data, call->reply->error, sizeof(call->reply->error))) {
        return;
    }

    ai_progress(call, ai_json_member(data, "prompt_progress"));

    const char* choice = ai_json_element(ai_json_member(data, "choices"), 0);
    const char* delta  = ai_json_member(choice, "delta");

    ai_think(call, ai_decode(call, ai_json_member(delta, "reasoning_content")));
    ai_take(call, ai_decode(call, ai_json_member(delta, "content")));
}


static size_t ai_collect(char* ptr, size_t size, size_t count, void* user) {

    ai_call_t* call = user;

    const size_t got = size * count;

    long status = 0;

    curl_easy_getinfo(call->curl, CURLINFO_RESPONSE_CODE, &status);

    if (!call->request->stream || status != 200) {

        if (call->body.size + got > call->limit) {

            call->overflow = true;
            return 0;
        }

        ai_buffer_append(&call->body, ptr, got);

        return call->body.failed ? 0 : got;
    }


    ai_buffer_append(&call->line, ptr, got);

    if (call->line.failed) {
        return 0;
    }

    char* start = call->line.data;
    char* end;

    while ((end = memchr(start, '\n', call->line.size - (size_t)(start - call->line.data))) != NULL) {

        *end = '\0';

        ai_event(call, start);

        start = end + 1;
    }

    const size_t rest = call->line.size - (size_t)(start - call->line.data);

    memmove(call->line.data, start, rest);

    call->line.size       = rest;
    call->line.data[rest] = '\0';

    if (rest > call->limit || call->body.size > call->limit) {
        call->overflow = true;
    }

    if (call->overflow || call->body.failed || call->scratch.failed || call->content.failed) {
        return 0;
    }

    return got;
}


static void ai_parse(ai_call_t* call) {

    if (call->body.size == 0) {
        return;
    }

    if (ai_failed(call->body.data, call->reply->error, sizeof(call->reply->error))) {
        return;
    }

    const char* choice = ai_json_element(ai_json_member(call->body.data, "choices"), 0);

    ai_take(call, ai_decode(call, ai_json_member(ai_json_member(choice, "message"), "content")));
}


static void ai_reset(ai_call_t* call) {

    ai_buffer_clear(&call->body);
    ai_buffer_clear(&call->line);
    ai_buffer_clear(&call->content);

    call->overflow = false;

    call->reply->error[0] = '\0';
}


static int ai_finish(ai_call_t* call, CURLcode code) {

    ai_reply_t* reply = call->reply;

    if (code == CURLE_OK) {

        if (call->request->stream && reply->status == 200) {

            if (call->line.size > 0) {
                ai_event(call, call->line.data);
            }

            if (!reply->error[0] && call->content.size == 0) {
                ai_parse(call);
            }

        } else {

            ai_parse(call);
        }
    }

    if (call->overflow) {
        snprintf(reply->error, sizeof(reply->error), "The answer is too long.");
    } else if (call->body.failed || call->line.failed || call->scratch.failed || call->content.failed) {
        snprintf(reply->error, sizeof(reply->error), "Out of memory.");
    } else if (code != CURLE_OK) {
        snprintf(reply->error, sizeof(reply->error), "%s.", curl_easy_strerror(code));
    }

    if (code != CURLE_OK) {
        reply->status = 0;
    }

    if (reply->error[0]) {
        return -1;
    }

    if (call->content.size == 0) {

        if (reply->status != 200) {
            snprintf(reply->error, sizeof(reply->error), "The endpoint answered %ld.", reply->status);
        } else {
            snprintf(reply->error, sizeof(reply->error), "The model answered with nothing at all.");
        }

        return -1;
    }

    reply->content = call->content.data;
    reply->size    = call->content.size;

    call->content.data = NULL;

    return 0;
}


static void ai_global(void) {

    ai_ready = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK ? 0 : -1;
}


int ai_init(void) {

    pthread_once(&ai_once, ai_global);

    return ai_ready;
}


int ai_chat(const ai_request_t* request, ai_reply_t* reply) {

    memset(reply, 0, sizeof(*reply));

    if (!request->url || !*request->url) {

        snprintf(reply->error, sizeof(reply->error), "No endpoint to ask.");
        return -1;
    }

    if (ai_init() < 0) {

        snprintf(reply->error, sizeof(reply->error), "Cannot initialise the HTTPS library.");
        return -1;
    }

    CURL* curl = curl_easy_init();

    if (!curl) {

        snprintf(reply->error, sizeof(reply->error), "Out of memory.");
        return -1;
    }


    ai_call_t call = {
        .curl    = curl,
        .request = request,
        .reply   = reply,
        .limit   = request->response_max ? request->response_max : AI_RESPONSE_MAX_DEFAULT,
    };

    ai_buffer_t payload = {0};

    char auth[512];

    struct curl_slist* headers = NULL;

    if (request->key && *request->key) {

        snprintf(auth, sizeof(auth), "Authorization: Bearer %s", request->key);
        headers = curl_slist_append(headers, auth);
    }

    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, request->url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ai_collect);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &call);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, request->agent ? request->agent : AI_AGENT_DEFAULT);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    if (request->timeout > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, request->timeout);
    }

    if (request->timeout_connect > 0) {
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, request->timeout_connect);
    }


    bool extras = request->extras && *request->extras;

    CURLcode code;

    for (;;) {

        ai_compose(&payload, request, extras);

        if (payload.failed) {

            code = CURLE_OUT_OF_MEMORY;
            break;
        }

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.data);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload.size);

        ai_reset(&call);

        code = curl_easy_perform(curl);

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &reply->status);

        if (code == CURLE_OK && extras && (reply->status == 400 || reply->status == 422)) {

            extras = false;
            continue;
        }

        break;
    }

    const int result = ai_finish(&call, code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    free(payload.data);
    free(call.body.data);
    free(call.line.data);
    free(call.scratch.data);
    free(call.content.data);

    return result;
}


void ai_reply_release(ai_reply_t* reply) {

    free(reply->content);

    reply->content = NULL;
    reply->size    = 0;
}
