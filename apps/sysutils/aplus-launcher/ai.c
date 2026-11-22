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

#include <dirent.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ai.h"

#if defined(CONFIG_HAVE_AI)

    #include <aplus/ai-json.h>
    #include <aplus/ai.h>
    #include <aplus/ui-control.h>


    /**
     * @brief How long a request, and the connecting part of it, may take before it is given up on, in seconds.
     */
    #define LAUNCHER_AI_TIMEOUT         60
    #define LAUNCHER_AI_TIMEOUT_CONNECT 10

    /**
     * @brief How much of a response is read before it is refused.
     */
    #define LAUNCHER_AI_RESPONSE_MAX (64 * 1024)

    /**
     * @brief What the launcher calls itself to the endpoint.
     */
    #define LAUNCHER_AI_AGENT "aplus-launcher"

    #define LAUNCHER_AI_APPS_MAX   (4 * 1024)
    #define LAUNCHER_AI_PROMPT_MAX (12 * 1024)

    #define LAUNCHER_AI_TEMPERATURE "0.2"
    #define LAUNCHER_AI_TOKENS      256
    #define LAUNCHER_AI_WARM_TOKENS 1
    #define LAUNCHER_AI_WARM_PROMPT "hi"

    #define LAUNCHER_AI_THEME_SUFFIX ".conf"

    #define LAUNCHER_AI_CACHE_DIR  "/var/run/.wm"
    #define LAUNCHER_AI_CACHE_PATH LAUNCHER_AI_CACHE_DIR "/launcher-ai.prompt"

    #define LAUNCHER_AI_BUILD_STACK (64 * 1024)

    #define LAUNCHER_AI_WARM_ATTEMPTS 12
    #define LAUNCHER_AI_WARM_DELAY    5


/**
 * @brief What the model is told it is, which is what turns a question into a command it can run.
 */

static const char launcher_ai_system[] = "You are the assistant of the aplus launcher. aplus is a small Unix-like OS. It is NOT Linux, macOS or Windows.\n"
                                         "Reply with one JSON object: {\"answer\": \"...\", \"command\": \"...\"}\n"
                                         "answer: one short sentence, always in English, even when the user writes in another language.\n"
                                         "command: one shell command line that does what was asked, using only the programs listed below, or \"\" when nothing should run.\n"
                                         "\n"
                                         "Rules:\n"
                                         "- To open a website, a URL, a file or a folder, use aplus-xopen. Websites open in the NetSurf browser. Always give a full https:// URL.\n"
                                         "- These do not exist on aplus: sudo, apt, systemctl, xdg-open, open, start, firefox, chrome, nautilus, gedit, notepad, wget, ifconfig, ip, ssh (use dbclient).\n"
                                         "- Never ask a question back. When the request is vague, pick the most likely meaning.\n"
                                         "- Questions, greetings and thanks get an answer and an empty command.\n"
                                         "- Never delete or overwrite files unless the user asked for it.\n"
                                         "\n"
                                         "Desktop programs:\n";

static const char launcher_ai_footer[] = "The user's files are under $HOME.\n";


static const struct {

    const char* program;
    const char* line;

} launcher_ai_programs[] = {

    {"aplus-xopen",      "aplus-xopen URL|FILE|DIR = open a web page, a file or a folder with its default app"                                },
    {"netsurf-fb",       "netsurf-fb [URL] = web browser"                                                                                     },
    {"aplus-explorer",   "aplus-explorer [DIR] = file manager"                                                                                },
    {"aplus-editor",     "aplus-editor [FILE] = text editor"                                                                                  },
    {"aplus-viewer",     "aplus-viewer FILE = PDF and image viewer"                                                                           },
    {"aplus-player",     "aplus-player [FILE] = music and audio player"                                                                       },
    {"aplus-terminal",   "aplus-terminal = terminal"                                                                                          },
    {"aplus-calculator", "aplus-calculator = calculator"                                                                                      },
    {"aplus-settings",   "aplus-settings [appearance|wallpaper|network|audio|software|about] = settings; network shows the IP address"       },
    {"aplus-wmctl",      "aplus-wmctl set wm.variant dark|light = dark or light mode"                                                         },
    {"aplus-wmctl",      "aplus-wmctl set-theme NAME = colour theme"                                                                          },
    {"aplus-wmctl",      "aplus-wmctl screenshot = take a screenshot"                                                                         },
    {"pkg",              "pkg search|info|install|remove|list|update|upgrade [NAME] = software packages"                                      },
    {"shutdown",         "shutdown -P = power off; shutdown -r = reboot"                                                                      },
};


static const char* const launcher_ai_tools[] = {

    "ls", "cat", "cp", "mv", "rm", "mkdir", "touch", "find", "grep", "sed", "awk", "sort",
    "uniq", "wc", "head", "tail", "less", "du", "df", "free", "ps", "top", "kill", "killall",
    "uname", "uptime", "date", "which", "man", "tar", "gzip", "unzip", "xz", "curl", "nc", "nslookup",
    "netstat", "lspci", "lsusb", "python3", "lua", "gcc", "make", "git", "vi", "nano", "sqlite3", "ffmpeg",
    "dbclient",
};


static const char* const launcher_ai_paths[] = {
    "/usr/bin",
    "/bin",
    "/usr/sbin",
    "/sbin",
};


static const struct {

    const char* user;
    const char* reply;

} launcher_ai_examples[] = {

    {"open youtube",                   "{\"answer\": \"Opening YouTube in the browser.\", \"command\": \"aplus-xopen https://www.youtube.com\"}"              },
    {"cerca gatti su google",          "{\"answer\": \"Searching Google for cats.\", \"command\": \"aplus-xopen 'https://www.google.com/search?q=gatti'\"}"   },
    {"show my files",                  "{\"answer\": \"Opening your home folder.\", \"command\": \"aplus-explorer \\\"$HOME\\\"\"}"                           },
    {"switch to dark mode",            "{\"answer\": \"Switching to dark mode.\", \"command\": \"aplus-wmctl set wm.variant dark\"}"                          },
    {"how much memory is free?",       "{\"answer\": \"This shows used and free memory.\", \"command\": \"free\"}"                                            },
    {"install sqlite",                 "{\"answer\": \"Installing the sqlite package.\", \"command\": \"pkg install sqlite\"}"                                },
    {"update everything",              "{\"answer\": \"Updating the package index and upgrading every package.\", \"command\": \"pkg update && pkg upgrade\"}"},
    {"play some music",                "{\"answer\": \"Opening the music player.\", \"command\": \"aplus-player\"}"                                           },
    {"python hello world",             "{\"answer\": \"This prints Hello, world! with Python.\", \"command\": \"python3 -c 'print(\\\"Hello, world!\\\")'\"}" },
    {"what is the capital of France?", "{\"answer\": \"Paris.\", \"command\": \"\"}"                                                                          },
    {"ciao",                           "{\"answer\": \"Hi! Ask me to open, find or run something.\", \"command\": \"\"}"                                      },
};


    #define LAUNCHER_AI_EXAMPLES (sizeof(launcher_ai_examples) / sizeof(launcher_ai_examples[0]))


static const char launcher_ai_extras[] = "\"chat_template_kwargs\":{\"enable_thinking\":false},\"response_format\":"
                                         "{\"type\":\"json_schema\",\"json_schema\":{\"name\":\"reply\",\"strict\":true,\"schema\":{\"type\":\"object\","
                                         "\"properties\":{\"answer\":{\"type\":\"string\"},\"command\":{\"type\":\"string\"}},"
                                         "\"required\":[\"answer\",\"command\"],\"additionalProperties\":false}}}";


static pthread_mutex_t launcher_ai_lock = PTHREAD_MUTEX_INITIALIZER;

static bool launcher_ai_built;
static bool launcher_ai_cached;
static bool launcher_ai_saved;

static uint64_t launcher_ai_key;

static char launcher_ai_apps[LAUNCHER_AI_APPS_MAX];
static char launcher_ai_prompt[LAUNCHER_AI_PROMPT_MAX];


typedef struct {

    char* data;
    size_t size;
    size_t max;
    bool full;

} launcher_ai_text_t;


static void launcher_ai_put(launcher_ai_text_t* text, const char* raw) {

    const size_t length = strlen(raw);

    if (text->full || text->size + length + 1 > text->max) {

        text->full = true;
        return;
    }

    memcpy(text->data + text->size, raw, length + 1);

    text->size += length;
}


static uint64_t launcher_ai_hash(uint64_t hash, const void* data, size_t size) {

    const unsigned char* bytes = data;

    for (size_t i = 0; i < size; i++) {

        hash ^= bytes[i];
        hash *= 0x100000001b3ULL;
    }

    return hash;
}


static uint64_t launcher_ai_hash_text(uint64_t hash, const char* text) {

    return launcher_ai_hash(hash, text, strlen(text) + 1);
}


static uint64_t launcher_ai_hash_dir(uint64_t hash, const char* path) {

    struct stat st;

    if (stat(path, &st) < 0) {
        memset(&st, 0, sizeof(st));
    }

    const int64_t stamp[2] = {(int64_t)st.st_ino, (int64_t)st.st_mtime};

    hash = launcher_ai_hash_text(hash, path);

    return launcher_ai_hash(hash, stamp, sizeof(stamp));
}


static uint64_t launcher_ai_fingerprint(void) {

    uint64_t hash = 0xcbf29ce484222325ULL;

    hash = launcher_ai_hash_text(hash, CONFIG_AI_PROVIDER_URL);
    hash = launcher_ai_hash_text(hash, CONFIG_AI_API_MODEL);
    hash = launcher_ai_hash_text(hash, launcher_ai_system);
    hash = launcher_ai_hash_text(hash, launcher_ai_footer);
    hash = launcher_ai_hash_text(hash, launcher_ai_extras);
    hash = launcher_ai_hash_text(hash, launcher_ai_apps);

    for (size_t i = 0; i < sizeof(launcher_ai_programs) / sizeof(launcher_ai_programs[0]); i++) {
        hash = launcher_ai_hash_text(hash, launcher_ai_programs[i].line);
    }

    for (size_t i = 0; i < sizeof(launcher_ai_tools) / sizeof(launcher_ai_tools[0]); i++) {
        hash = launcher_ai_hash_text(hash, launcher_ai_tools[i]);
    }

    for (size_t i = 0; i < sizeof(launcher_ai_examples) / sizeof(launcher_ai_examples[0]); i++) {

        hash = launcher_ai_hash_text(hash, launcher_ai_examples[i].user);
        hash = launcher_ai_hash_text(hash, launcher_ai_examples[i].reply);
    }

    for (size_t i = 0; i < sizeof(launcher_ai_paths) / sizeof(launcher_ai_paths[0]); i++) {
        hash = launcher_ai_hash_dir(hash, launcher_ai_paths[i]);
    }

    return launcher_ai_hash_dir(hash, UI_THEME_DIR);
}


static bool launcher_ai_installed(const char* program) {

    char path[PATH_MAX];

    for (size_t i = 0; i < sizeof(launcher_ai_paths) / sizeof(launcher_ai_paths[0]); i++) {

        snprintf(path, sizeof(path), "%s/%s", launcher_ai_paths[i], program);

        if (access(path, X_OK) == 0) {
            return true;
        }
    }

    return false;
}


static void launcher_ai_themes(launcher_ai_text_t* text) {

    DIR* dir = opendir(UI_THEME_DIR);

    if (!dir) {
        return;
    }

    const size_t suffix = strlen(LAUNCHER_AI_THEME_SUFFIX);

    bool first = true;

    struct dirent* it;

    while ((it = readdir(dir)) != NULL) {

        const size_t length = strlen(it->d_name);

        if (length <= suffix || strcmp(it->d_name + length - suffix, LAUNCHER_AI_THEME_SUFFIX) != 0) {
            continue;
        }

        char name[LAUNCHER_FIELD_MAX];

        snprintf(name, sizeof(name), "%s%.*s", first ? "\nColour themes: " : ", ", (int)(length - suffix), it->d_name);

        launcher_ai_put(text, name);

        first = false;
    }

    closedir(dir);

    if (!first) {
        launcher_ai_put(text, "\n");
    }
}


static void launcher_ai_compose(void) {

    launcher_ai_text_t text = {.data = launcher_ai_prompt, .size = 0, .max = sizeof(launcher_ai_prompt)};

    launcher_ai_prompt[0] = '\0';

    launcher_ai_put(&text, launcher_ai_system);

    for (size_t i = 0; i < sizeof(launcher_ai_programs) / sizeof(launcher_ai_programs[0]); i++) {

        if (!launcher_ai_installed(launcher_ai_programs[i].program)) {
            continue;
        }

        launcher_ai_put(&text, launcher_ai_programs[i].line);
        launcher_ai_put(&text, "\n");
    }

    launcher_ai_put(&text, "\nShell is dash. Tools:");

    for (size_t i = 0; i < sizeof(launcher_ai_tools) / sizeof(launcher_ai_tools[0]); i++) {

        if (!launcher_ai_installed(launcher_ai_tools[i])) {
            continue;
        }

        launcher_ai_put(&text, " ");
        launcher_ai_put(&text, launcher_ai_tools[i]);

        if (strcmp(launcher_ai_tools[i], "dbclient") == 0) {
            launcher_ai_put(&text, " (ssh client)");
        }
    }

    launcher_ai_put(&text, ".\n");
    launcher_ai_put(&text, launcher_ai_footer);
    launcher_ai_put(&text, launcher_ai_apps);

    launcher_ai_themes(&text);
}


static bool launcher_ai_load(uint64_t key) {

    FILE* file = fopen(LAUNCHER_AI_CACHE_PATH, "r");

    if (!file) {
        return false;
    }

    unsigned long long stored = 0;

    bool hit = fscanf(file, "%16llx", &stored) == 1 && fgetc(file) == '\n' && stored == key;

    if (hit) {

        const size_t got = fread(launcher_ai_prompt, 1, sizeof(launcher_ai_prompt) - 1, file);

        launcher_ai_prompt[got] = '\0';

        hit = got > 0 && !ferror(file);
    }

    fclose(file);

    return hit;
}


static void launcher_ai_save(void) {

    pthread_mutex_lock(&launcher_ai_lock);

    const bool skip = launcher_ai_cached || launcher_ai_saved;

    launcher_ai_saved = true;

    pthread_mutex_unlock(&launcher_ai_lock);

    if (skip) {
        return;
    }

    char temporary[PATH_MAX];

    snprintf(temporary, sizeof(temporary), "%s.%d", LAUNCHER_AI_CACHE_PATH, (int)getpid());

    mkdir(LAUNCHER_AI_CACHE_DIR, 0755);

    FILE* file = fopen(temporary, "w");

    if (!file) {
        return;
    }

    const bool written = fprintf(file, "%016llx\n", (unsigned long long)launcher_ai_key) > 0 && fputs(launcher_ai_prompt, file) >= 0;

    if (fclose(file) == 0 && written && rename(temporary, LAUNCHER_AI_CACHE_PATH) == 0) {
        return;
    }

    unlink(temporary);
}


static bool launcher_ai_build(void) {

    pthread_mutex_lock(&launcher_ai_lock);

    if (!launcher_ai_built) {

        launcher_ai_key    = launcher_ai_fingerprint();
        launcher_ai_cached = launcher_ai_load(launcher_ai_key);

        if (!launcher_ai_cached) {
            launcher_ai_compose();
        }

        launcher_ai_built = true;
    }

    const bool cached = launcher_ai_cached;

    pthread_mutex_unlock(&launcher_ai_lock);

    return cached;
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
    const char* answer  = ai_json_member(object, "answer");
    const char* command = ai_json_member(object, "command");

    if (!answer && !command) {
        strncpy(out->answer, content, sizeof(out->answer) - 1);
        return;
    }

    ai_json_string(answer, out->answer, sizeof(out->answer));
    ai_json_string(command, out->command, sizeof(out->command));
}


int launcher_ai_init(void) {

    return ai_init();
}


static int launcher_ai_post(const char* prompt, int tokens, ai_reply_t* reply) {

    ai_message_t messages[2 + 2 * LAUNCHER_AI_EXAMPLES];

    size_t count = 0;

    messages[count++] = (ai_message_t){.role = "system", .content = launcher_ai_prompt};

    for (size_t i = 0; i < LAUNCHER_AI_EXAMPLES; i++) {

        messages[count++] = (ai_message_t){.role = "user", .content = launcher_ai_examples[i].user};
        messages[count++] = (ai_message_t){.role = "assistant", .content = launcher_ai_examples[i].reply};
    }

    messages[count++] = (ai_message_t){.role = "user", .content = prompt};

    const ai_request_t request = {
        .url             = CONFIG_AI_PROVIDER_URL,
        .key             = CONFIG_AI_API_KEY,
        .model           = CONFIG_AI_API_MODEL,
        .agent           = LAUNCHER_AI_AGENT,
        .temperature     = LAUNCHER_AI_TEMPERATURE,
        .extras          = launcher_ai_extras,
        .messages        = messages,
        .count           = count,
        .tokens          = tokens,
        .timeout         = LAUNCHER_AI_TIMEOUT,
        .timeout_connect = LAUNCHER_AI_TIMEOUT_CONNECT,
        .response_max    = LAUNCHER_AI_RESPONSE_MAX,
    };

    return ai_chat(&request, reply);
}


static void launcher_ai_list(const launcher_entry_t* entries, size_t count) {

    launcher_ai_text_t text = {.data = launcher_ai_apps, .size = 0, .max = sizeof(launcher_ai_apps)};

    launcher_ai_apps[0] = '\0';

    if (count > 0) {
        launcher_ai_put(&text, "\nInstalled applications (name = command):\n");
    }

    for (size_t i = 0; i < count && !text.full; i++) {

        char line[4 * LAUNCHER_FIELD_MAX];

        if (entries[i].comment[0]) {
            snprintf(line, sizeof(line), "%s = %s (%s)\n", entries[i].name, entries[i].exec, entries[i].comment);
        } else {
            snprintf(line, sizeof(line), "%s = %s\n", entries[i].name, entries[i].exec);
        }

        launcher_ai_put(&text, line);
    }
}


static void* launcher_ai_background(void* arg) {

    (void)arg;

    launcher_ai_build();

    return NULL;
}


void launcher_ai_prepare(const launcher_entry_t* entries, size_t count) {

    launcher_ai_list(entries, count);


    pthread_attr_t attr;
    pthread_t thread;

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, LAUNCHER_AI_BUILD_STACK);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&thread, &attr, launcher_ai_background, NULL);

    pthread_attr_destroy(&attr);
}


int launcher_ai_warm(const launcher_entry_t* entries, size_t count, char* error, size_t max) {

    error[0] = '\0';

    if (!*CONFIG_AI_PROVIDER_URL || !*CONFIG_AI_API_KEY) {

        snprintf(error, max, "no provider or API key configured");
        return -1;
    }

    if (launcher_ai_init() < 0) {

        snprintf(error, max, "cannot initialise the HTTPS library");
        return -1;
    }

    launcher_ai_list(entries, count);
    launcher_ai_build();


    int result = -1;

    for (int attempt = 0; attempt < LAUNCHER_AI_WARM_ATTEMPTS; attempt++) {

        if (attempt > 0) {
            sleep(LAUNCHER_AI_WARM_DELAY);
        }

        ai_reply_t reply;

        launcher_ai_post(LAUNCHER_AI_WARM_PROMPT, LAUNCHER_AI_WARM_TOKENS, &reply);
        ai_reply_release(&reply);

        if (reply.status == 200) {

            launcher_ai_save();

            result = 0;
            break;
        }

        if (reply.status != 0) {
            snprintf(error, max, "%s answered %ld", CONFIG_AI_PROVIDER_URL, reply.status);
        } else {
            snprintf(error, max, "%s: %s", CONFIG_AI_PROVIDER_URL, reply.error);
        }

        if (reply.status != 0 && reply.status < 500 && reply.status != 429) {
            break;
        }
    }

    return result;
}


int launcher_ai_ask(const char* prompt, launcher_ai_reply_t* out) {

    memset(out, 0, sizeof(*out));

    if (!prompt || !*prompt) {

        snprintf(out->error, sizeof(out->error), "There is nothing to ask.");
        return -1;
    }

    if (!*CONFIG_AI_PROVIDER_URL) {

        snprintf(out->error, sizeof(out->error), "No provider: set CONFIG_AI_PROVIDER_URL and build again.");
        return -1;
    }

    if (!*CONFIG_AI_API_KEY) {

        snprintf(out->error, sizeof(out->error), "No API key: set CONFIG_AI_API_KEY and build again.");
        return -1;
    }

    launcher_ai_build();


    ai_reply_t reply;

    if (launcher_ai_post(prompt, LAUNCHER_AI_TOKENS, &reply) < 0) {

        snprintf(out->error, sizeof(out->error), "%s", reply.error);
        return -1;
    }

    launcher_ai_split(reply.content, out);

    if (reply.status == 200) {
        launcher_ai_save();
    }

    ai_reply_release(&reply);

    return 0;
}

#endif
