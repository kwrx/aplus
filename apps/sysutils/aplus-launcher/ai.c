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
 * @brief One chat completion over HTTPS, which is the whole of what the launcher asks of a model.
 *
 * The endpoint is OpenAI shaped, so the request is a list of messages and the answer is
 * buried in choices[0].message.content. What comes back out of that is asked to be JSON of
 * its own, so that an answer and the command it suggests arrive apart rather than having to
 * be told apart afterwards.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"

#if defined(CONFIG_HAVE_AI)

    #include <curl/curl.h>

    #include "json.h"


    /**
     * @brief Where the chat completions live.
     */
    #define LAUNCHER_AI_URL "https://router.huggingface.co/v1/chat/completions"

    /**
     * @brief How long a request, and the connecting part of it, may take before it is given up on, in seconds.
     *
     * The window is frozen for exactly this long in the worst case, which is what keeps both
     * numbers small enough to sit through.
     */
    #define LAUNCHER_AI_TIMEOUT         20
    #define LAUNCHER_AI_TIMEOUT_CONNECT 10

    /**
     * @brief How big a request may get, and how much of a response is read before it is refused.
     */
    #define LAUNCHER_AI_REQUEST_MAX  (8 * 1024)
    #define LAUNCHER_AI_RESPONSE_MAX (64 * 1024)

    /**
     * @brief What the launcher calls itself to the endpoint.
     */
    #define LAUNCHER_AI_AGENT "aplus-launcher"


/**
 * @brief What the model is told it is, which is what turns a question into a command it can run.
 */

static const char launcher_ai_system[] = "You are the assistant built into the launcher of aplus, a small Unix-like desktop system with a BusyBox-like userland. "
                                         "Answer with one JSON object and nothing else, shaped {\"answer\": \"...\", \"command\": \"...\"}. "
                                         "Keep the answer to at most three short sentences of plain text, with no markup and no code fences. "
                                         "Make the command a single POSIX shell command line that carries out what was asked, and leave the command out altogether when no one command does. ";


/**
 * @brief The response as it arrives, into a buffer that was sized once rather than grown.
 */

typedef struct {

    char* data;
    size_t size;

} launcher_ai_body_t;


/**
 * @brief Takes what curl has read, and refuses a response too big to be an answer.
 *
 * @param ptr What was read.
 * @param size The size of an item.
 * @param count How many items.
 * @param user The body being filled.
 * @return How much was taken, which is less than offered to make curl stop.
 */

static size_t launcher_ai_collect(char* ptr, size_t size, size_t count, void* user) {

    launcher_ai_body_t* body = user;

    const size_t got = size * count;

    if (body->size + got + 1 > LAUNCHER_AI_RESPONSE_MAX) {
        return 0;
    }

    memcpy(body->data + body->size, ptr, got);

    body->size += got;
    body->data[body->size] = '\0';

    return got;
}


/**
 * @brief Writes the request body, with everything that came from a person escaped into it.
 *
 * @param out Receives the body.
 * @param max The size of that buffer.
 * @param prompt What to ask.
 * @return How long the body is.
 */

static size_t launcher_ai_request(char* out, size_t max, const char* prompt) {

    char system[sizeof(launcher_ai_system) * 2];
    char model[256];
    char user[LAUNCHER_AI_REQUEST_MAX / 2];

    json_escape(system, sizeof(system), launcher_ai_system);
    json_escape(model, sizeof(model), CONFIG_HUGGINGFACE_MODEL);
    json_escape(user, sizeof(user), prompt);

    const int len = snprintf(out, max, "{\"model\":\"%s\",\"messages\":[{\"role\":\"system\",\"content\":\"%s\"},{\"role\":\"user\",\"content\":\"%s\"}]}", model, system, user);

    return len > 0 ? (size_t)len : 0;
}


/**
 * @brief Takes the complaint out of a response that carries one.
 *
 * @param json The response.
 * @param out Receives the message.
 * @param max The size of that buffer.
 * @return true when the response was an error rather than an answer.
 */

static bool launcher_ai_failed(const char* json, char* out, size_t max) {

    const char* error = json_member(json, "error");

    if (!error) {
        return false;
    }

    if (json_string(error, out, max) > 0) {
        return true;
    }

    if (json_string(json_member(error, "message"), out, max) > 0) {
        return true;
    }

    snprintf(out, max, "The endpoint refused the request.");

    return true;
}


/**
 * @brief Takes the answer apart, whether or not the model did as it was told.
 *
 * Anything that is not the object it was asked for is shown as it stands: a model that
 * answered in prose still answered.
 *
 * @param content What the model said.
 * @param out Receives the answer and the command.
 */

static void launcher_ai_split(const char* content, launcher_ai_reply_t* out) {

    const char* object  = strchr(content, '{');
    const char* answer  = json_member(object, "answer");
    const char* command = json_member(object, "command");

    if (!answer && !command) {

        strncpy(out->answer, content, sizeof(out->answer) - 1);
        return;
    }

    json_string(answer, out->answer, sizeof(out->answer));
    json_string(command, out->command, sizeof(out->command));
}


/**
 * @brief Finds the answer in a response that has one.
 *
 * @param json The response.
 * @param status What the endpoint answered with, which is all there is to say when the body says nothing.
 * @param out Receives the answer, or the reason there is none.
 * @return 0 when the model answered, -1 otherwise.
 */

static int launcher_ai_parse(const char* json, long status, launcher_ai_reply_t* out) {

    if (launcher_ai_failed(json, out->error, sizeof(out->error))) {
        return -1;
    }


    const char* choice  = json_element(json_member(json, "choices"), 0);
    const char* content = json_member(json_member(choice, "message"), "content");

    char text[LAUNCHER_AI_ANSWER_MAX];

    if (json_string(content, text, sizeof(text)) == 0) {

        if (status != 200) {
            snprintf(out->error, sizeof(out->error), "The endpoint answered %ld.", status);
        } else {
            snprintf(out->error, sizeof(out->error), "The model answered with nothing at all.");
        }

        return -1;
    }

    launcher_ai_split(text, out);

    return 0;
}


int launcher_ai_ask(const char* prompt, launcher_ai_reply_t* out) {

    static bool ready = false;

    memset(out, 0, sizeof(*out));

    if (!prompt || !*prompt) {

        snprintf(out->error, sizeof(out->error), "There is nothing to ask.");
        return -1;
    }

    if (!*CONFIG_HUGGINGFACE_API_KEY) {

        snprintf(out->error, sizeof(out->error), "No API key: set CONFIG_HUGGINGFACE_API_KEY and build again.");
        return -1;
    }


    if (!ready) {

        curl_global_init(CURL_GLOBAL_DEFAULT);
        ready = true;
    }


    char* request = malloc(LAUNCHER_AI_REQUEST_MAX);

    launcher_ai_body_t body = {.data = malloc(LAUNCHER_AI_RESPONSE_MAX), .size = 0};

    CURL* curl = curl_easy_init();

    if (!request || !body.data || !curl) {

        snprintf(out->error, sizeof(out->error), "Out of memory.");

        free(request);
        free(body.data);

        if (curl) {
            curl_easy_cleanup(curl);
        }

        return -1;
    }

    body.data[0] = '\0';


    const size_t len = launcher_ai_request(request, LAUNCHER_AI_REQUEST_MAX, prompt);

    char auth[512];

    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", CONFIG_HUGGINGFACE_API_KEY);

    struct curl_slist* headers = NULL;

    headers = curl_slist_append(headers, auth);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, LAUNCHER_AI_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)len);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, launcher_ai_collect);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)LAUNCHER_AI_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, (long)LAUNCHER_AI_TIMEOUT_CONNECT);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, LAUNCHER_AI_AGENT);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    const CURLcode code = curl_easy_perform(curl);

    long status = 0;

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);


    int result = 0;

    if (code != CURLE_OK) {

        snprintf(out->error, sizeof(out->error), "%s.", curl_easy_strerror(code));
        result = -1;

    } else if (launcher_ai_parse(body.data, status, out) < 0) {

        result = -1;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    free(request);
    free(body.data);

    return result;
}

#endif
