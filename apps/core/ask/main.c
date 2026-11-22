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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <aplus/ai.h>

#define ASK_NAME            "ask"
#define ASK_TIMEOUT_CONNECT 10
#define ASK_RESPONSE_MAX    (4 * 1024 * 1024)
#define ASK_CHUNK           4096
#define ASK_PARTS_MAX       64
#define ASK_FRAMES          6
#define ASK_AUDIO_SECONDS   300
#define ASK_AUDIO_RATE      16000
#define ASK_FRAME_SCALE     "scale='min(iw,768)':-2"
#define ASK_FFMPEG          "ffmpeg"
#define ASK_FFPROBE         "ffprobe"
#define ASK_MUTOOL          "mutool"
#define ASK_PAGES           6
#define ASK_PAGE_WIDTH      "768"
#define ASK_DOCUMENT_MAX    (256 * 1024)
#define ASK_DOCUMENT_WORDS  16
#define ASK_THINK_ROWS      5


static const char ask_system[] = "You are the assistant of aplus, a small Unix-like operating system that is not Linux.\n"
                                 "You are asked from a shell, so reply in plain text for a terminal: no Markdown, show code and commands as plain lines.\n"
                                 "Reply in the language of the question. Be concise and precise.\n"
                                 "When files are attached, read them carefully and base the answer on their content.\n"
                                 "Pictures and audio may be attached too: they are the content of the files described in the text.\n";


typedef struct {

    char* data;
    size_t size;
    size_t max;

} ask_text_t;


typedef struct {

    bool printed;
    char last;

    bool thinking;
    char thought;
    ask_text_t thoughts;
    size_t rows;
    double started;

    bool progress;
    bool waiting;

} ask_output_t;


typedef struct {

    size_t start;
    size_t end;

} ask_row_t;


typedef struct {

    ask_row_t rows[ASK_THINK_ROWS];
    size_t count;

} ask_window_t;


static bool ask_verbose;
static bool ask_reasoning;
static bool ask_tty;


static void ask_status(const char* format, ...) {

    if (!ask_verbose) {
        return;
    }

    va_list args;
    va_start(args, format);

    fputs(ask_tty ? "\033[2m" : "", stderr);
    vfprintf(stderr, format, args);
    fputs(ask_tty ? "\033[0m\n" : "\n", stderr);

    va_end(args);
}


typedef struct {

    ai_part_t parts[ASK_PARTS_MAX];
    ask_text_t blobs[ASK_PARTS_MAX];
    size_t count;

} ask_media_t;


typedef struct {

    bool video;
    bool audio;
    double duration;

} ask_probe_t;


static void ask_append(ask_text_t* text, const char* data, size_t size) {

    if (text->size + size + 1 > text->max) {

        size_t max = text->max ? text->max : ASK_CHUNK;

        while (max < text->size + size + 1) {
            max *= 2;
        }

        char* grown = realloc(text->data, max);

        if (!grown) {

            fprintf(stderr, ASK_NAME ": out of memory\n");
            exit(1);
        }

        text->data = grown;
        text->max  = max;
    }

    memcpy(text->data + text->size, data, size);

    text->size += size;
    text->data[text->size] = '\0';
}


static void ask_put(ask_text_t* text, const char* data) {

    ask_append(text, data, strlen(data));
}


static void ask_trim(ask_text_t* text) {

    while (text->size > 0 && isspace((unsigned char)text->data[text->size - 1])) {
        text->data[--text->size] = '\0';
    }
}


static ask_text_t ask_read(const char* path) {

    FILE* file = path ? fopen(path, "r") : stdin;

    const char* name = path ? path : "standard input";

    if (!file) {

        fprintf(stderr, ASK_NAME ": %s: %s\n", name, strerror(errno));
        exit(1);
    }

    ask_text_t text = {0};

    char chunk[ASK_CHUNK];
    size_t got;

    while ((got = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        ask_append(&text, chunk, got);
    }

    const bool failed = ferror(file);
    const int error   = errno;

    if (path) {
        fclose(file);
    }

    if (failed) {

        fprintf(stderr, ASK_NAME ": %s: %s\n", name, strerror(error));
        exit(1);
    }

    if (text.size > 0 && memchr(text.data, '\0', text.size)) {

        fprintf(stderr, ASK_NAME ": %s: not a text file\n", name);
        exit(1);
    }

    return text;
}


static void ask_section(ask_text_t* text, const char* name, const ask_text_t* body) {

    ask_put(text, "\n=== ");
    ask_put(text, name);
    ask_put(text, " ===\n");

    if (body->size > 0) {
        ask_append(text, body->data, body->size);
    }

    if (body->size == 0 || body->data[body->size - 1] != '\n') {
        ask_put(text, "\n");
    }

    ask_put(text, "=== end of ");
    ask_put(text, name);
    ask_put(text, " ===\n");
}


static int ask_run(char* const argv[], ask_text_t* out) {

    int fds[2];

    if (pipe(fds) < 0) {
        return -1;
    }

    const pid_t pid = fork();

    if (pid < 0) {

        close(fds[0]);
        close(fds[1]);

        return -1;
    }

    if (pid == 0) {

        const int null = open("/dev/null", O_RDONLY);

        if (null >= 0) {
            dup2(null, STDIN_FILENO);
            close(null);
        }

        dup2(fds[1], STDOUT_FILENO);

        close(fds[0]);
        close(fds[1]);

        execvp(argv[0], argv);
        _exit(127);
    }

    close(fds[1]);

    char chunk[ASK_CHUNK];
    ssize_t got;

    while ((got = read(fds[0], chunk, sizeof(chunk))) != 0) {

        if (got < 0) {

            if (errno == EINTR) {
                continue;
            }

            break;
        }

        ask_append(out, chunk, (size_t)got);
    }

    close(fds[0]);

    int status;

    while (waitpid(pid, &status, 0) < 0) {

        if (errno != EINTR) {
            return -1;
        }
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {

        fprintf(stderr, ASK_NAME ": cannot run %s\n", argv[0]);
        return -1;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}


static void ask_access(const char* path) {

    if (access(path, R_OK) < 0) {

        fprintf(stderr, ASK_NAME ": %s: %s\n", path, strerror(errno));
        exit(1);
    }
}


static ask_probe_t ask_probe(const char* path) {

    char* argv[] = {(char*)ASK_FFPROBE, (char*)"-v", (char*)"error", (char*)"-show_entries", (char*)"format=duration:stream=codec_type", (char*)"-of", (char*)"default=nw=1", (char*)path, NULL};

    ask_probe_t probe = {0};
    ask_text_t text   = {0};

    if (ask_run(argv, &text) < 0 || text.size == 0) {

        fprintf(stderr, ASK_NAME ": %s: cannot read it as media\n", path);
        exit(1);
    }

    for (char* line = strtok(text.data, "\n"); line; line = strtok(NULL, "\n")) {

        if (strcmp(line, "codec_type=video") == 0) {
            probe.video = true;
        } else if (strcmp(line, "codec_type=audio") == 0) {
            probe.audio = true;
        } else if (strncmp(line, "duration=", 9) == 0) {
            probe.duration = strtod(line + 9, NULL);
        }
    }

    free(text.data);

    return probe;
}


static ask_text_t* ask_media_add(ask_media_t* media, ai_part_type_t type, const char* format) {

    if (media->count == ASK_PARTS_MAX) {

        fprintf(stderr, ASK_NAME ": too many pictures and recordings\n");
        exit(1);
    }

    ask_text_t* blob = &media->blobs[media->count];

    media->parts[media->count] = (ai_part_t){.type = type, .format = format};

    *blob = (ask_text_t){0};

    return blob;
}


static void ask_media_commit(ask_media_t* media) {

    ask_text_t* blob = &media->blobs[media->count];

    media->parts[media->count].data = blob->data;
    media->parts[media->count].size = blob->size;

    media->count++;
}


static void ask_put_le(ask_text_t* text, uint32_t value, size_t size) {

    char bytes[4];

    for (size_t i = 0; i < size; i++) {
        bytes[i] = (char)((value >> (8 * i)) & 0xFF);
    }

    ask_append(text, bytes, size);
}


static double ask_audio(ask_media_t* media, const char* path) {

    char seconds[16];

    snprintf(seconds, sizeof(seconds), "%d", ASK_AUDIO_SECONDS);

    char rate[16];

    snprintf(rate, sizeof(rate), "%d", ASK_AUDIO_RATE);

    char* argv[] = {(char*)ASK_FFMPEG, (char*)"-nostdin", (char*)"-v", (char*)"error", (char*)"-i", (char*)path, (char*)"-map", (char*)"0:a:0", (char*)"-t", seconds, (char*)"-ac", (char*)"1", (char*)"-ar", rate, (char*)"-c:a", (char*)"pcm_s16le", (char*)"-f", (char*)"s16le", (char*)"-", NULL};

    ask_text_t pcm = {0};

    if (ask_run(argv, &pcm) < 0 || pcm.size == 0) {

        fprintf(stderr, ASK_NAME ": %s: cannot decode its audio\n", path);
        exit(1);
    }

    ask_text_t* wav = ask_media_add(media, AI_PART_AUDIO, "wav");

    ask_put(wav, "RIFF");
    ask_put_le(wav, (uint32_t)(36 + pcm.size), 4);
    ask_put(wav, "WAVEfmt ");
    ask_put_le(wav, 16, 4);
    ask_put_le(wav, 1, 2);
    ask_put_le(wav, 1, 2);
    ask_put_le(wav, ASK_AUDIO_RATE, 4);
    ask_put_le(wav, ASK_AUDIO_RATE * 2, 4);
    ask_put_le(wav, 2, 2);
    ask_put_le(wav, 16, 2);
    ask_put(wav, "data");
    ask_put_le(wav, (uint32_t)pcm.size, 4);
    ask_append(wav, pcm.data, pcm.size);

    ask_media_commit(media);

    const double length = (double)pcm.size / (ASK_AUDIO_RATE * 2);

    free(pcm.data);

    return length;
}


static void ask_picture(ask_media_t* media, const char* path, double at) {

    char seek[32];

    snprintf(seek, sizeof(seek), "%.3f", at);

    char* argv[] = {(char*)ASK_FFMPEG, (char*)"-nostdin", (char*)"-v", (char*)"error", (char*)"-ss", seek, (char*)"-i", (char*)path, (char*)"-map", (char*)"0:v:0", (char*)"-frames:v", (char*)"1", (char*)"-vf", (char*)ASK_FRAME_SCALE, (char*)"-c:v", (char*)"mjpeg", (char*)"-q:v", (char*)"3", (char*)"-f", (char*)"image2pipe", (char*)"-", NULL};

    ask_text_t* jpeg = ask_media_add(media, AI_PART_IMAGE, "image/jpeg");

    if (ask_run(argv, jpeg) < 0 || jpeg->size == 0) {

        fprintf(stderr, ASK_NAME ": %s: cannot decode its picture\n", path);
        exit(1);
    }

    ask_media_commit(media);
}


static int ask_document_pages(const char* path) {

    char* argv[] = {(char*)ASK_MUTOOL, (char*)"show", (char*)path, (char*)"trailer/Root/Pages/Count", NULL};

    ask_text_t text = {0};

    int pages = 0;

    if (ask_run(argv, &text) == 0 && text.size > 0) {
        pages = atoi(text.data);
    }

    free(text.data);

    return pages;
}


static size_t ask_document_words(const ask_text_t* text) {

    size_t words = 0;
    bool inside  = false;

    for (size_t i = 0; i < text->size; i++) {

        const bool space = isspace((unsigned char)text->data[i]);

        if (!space && !inside) {
            words++;
        }

        inside = !space;
    }

    return words;
}


static void ask_page(ask_media_t* media, const char* path, int page) {

    char number[16];

    snprintf(number, sizeof(number), "%d", page);

    char* argv[] = {(char*)ASK_MUTOOL, (char*)"draw", (char*)"-q", (char*)"-F", (char*)"png", (char*)"-c", (char*)"rgb", (char*)"-w", (char*)ASK_PAGE_WIDTH, (char*)"-o", (char*)"-", (char*)path, number, NULL};

    ask_text_t* png = ask_media_add(media, AI_PART_IMAGE, "image/png");

    if (ask_run(argv, png) < 0 || png->size == 0) {

        fprintf(stderr, ASK_NAME ": %s: cannot draw page %d\n", path, page);
        exit(1);
    }

    ask_media_commit(media);
}


static void ask_document(ask_text_t* body, ask_media_t* media, const char* path) {

    char line[512];

    ask_status("Counting the pages of %s", path);

    const int pages = ask_document_pages(path);

    ask_status("Reading the text of %s", path);

    char* argv[] = {(char*)ASK_MUTOOL, (char*)"draw", (char*)"-q", (char*)"-F", (char*)"txt", (char*)"-o", (char*)"-", (char*)path, NULL};

    ask_text_t text = {0};

    if (ask_run(argv, &text) < 0) {

        fprintf(stderr, ASK_NAME ": %s: cannot read it as a PDF\n", path);
        exit(1);
    }

    if (pages > 0) {
        snprintf(line, sizeof(line), "A PDF document of %d pages.\n", pages);
    } else {
        snprintf(line, sizeof(line), "A PDF document.\n");
    }

    ask_put(body, line);

    if (ask_document_words(&text) >= ASK_DOCUMENT_WORDS) {

        if (text.size > ASK_DOCUMENT_MAX) {

            text.size                   = ASK_DOCUMENT_MAX;
            text.data[ASK_DOCUMENT_MAX] = '\0';

            ask_put(body, "Its text is too long: only its beginning follows.\n");
        }

        ask_put(body, "Its text follows.\n\n");
        ask_append(body, text.data, text.size);

        free(text.data);

        return;
    }

    free(text.data);

    const int count = pages <= 0 ? 1 : pages < ASK_PAGES ? pages : ASK_PAGES;

    if (pages != count) {
        snprintf(line, sizeof(line), "It has no text to extract, so only its first %d pages are attached as images:\n", count);
    } else {
        snprintf(line, sizeof(line), "It has no text to extract, so its pages are attached as images:\n");
    }

    ask_put(body, line);

    for (int page = 1; page <= count; page++) {

        ask_status("Drawing page %d of %d", page, count);
        ask_page(media, path, page);

        snprintf(line, sizeof(line), "  image %zu: page %d\n", media->count, page);
        ask_put(body, line);
    }
}


static void ask_attach(ask_text_t* text, ask_media_t* media, const char* path) {

    const ai_kind_t kind = ai_kind(path);

    if (kind == AI_KIND_TEXT) {

        ask_status("Reading %s", path);

        ask_text_t body = ask_read(path);

        ask_section(text, path, &body);

        free(body.data);

        return;
    }

    ask_access(path);

    char line[512];

    ask_text_t body = {0};

    if (kind == AI_KIND_IMAGE) {

        ask_status("Converting the picture %s", path);
        ask_picture(media, path, 0.0);

        snprintf(line, sizeof(line), "A picture, attached as image %zu.\n", media->count);
        ask_put(&body, line);

        ask_section(text, path, &body);

        free(body.data);

        return;
    }

    if (kind == AI_KIND_DOCUMENT) {

        ask_document(&body, media, path);
        ask_section(text, path, &body);

        free(body.data);

        return;
    }

    ask_status("Probing %s", path);

    const ask_probe_t probe = ask_probe(path);

    if (kind == AI_KIND_VIDEO && probe.video) {

        const int frames = probe.duration > 0.0 ? ASK_FRAMES : 1;

        snprintf(line, sizeof(line), "A video, %.0f seconds long. Frames taken at these times are attached as images:\n", probe.duration);
        ask_put(&body, line);

        for (int i = 0; i < frames; i++) {

            const double at = probe.duration > 0.0 ? probe.duration * (2 * i + 1) / (2 * frames) : 0.0;

            ask_status("Taking frame %d of %d (at %.1f s)", i + 1, frames, at);
            ask_picture(media, path, at);

            snprintf(line, sizeof(line), "  image %zu: at %.1f s\n", media->count, at);
            ask_put(&body, line);
        }
    }

    if (probe.audio) {

        if (probe.duration > ASK_AUDIO_SECONDS) {
            ask_status("Decoding the first %d seconds of the sound", ASK_AUDIO_SECONDS);
        } else {
            ask_status("Decoding the sound");
        }

        const double length = ask_audio(media, path);

        if (probe.duration > length + 1.0) {
            snprintf(line, sizeof(line), "%s, attached as audio %zu: only its first %.0f seconds out of %.0f.\n", kind == AI_KIND_VIDEO ? "Its sound track" : "A recording", media->count, length, probe.duration);
        } else {
            snprintf(line, sizeof(line), "%s, %.0f seconds long, attached as audio %zu.\n", kind == AI_KIND_VIDEO ? "Its sound track" : "A recording", length, media->count);
        }

        ask_put(&body, line);

    } else if (kind == AI_KIND_VIDEO) {

        ask_put(&body, "It has no sound.\n");
    }

    if (body.size == 0) {

        fprintf(stderr, ASK_NAME ": %s: no audio or video in it\n", path);
        exit(1);
    }

    ask_section(text, path, &body);

    free(body.data);
}


#if defined(CONFIG_HAVE_AI)

static void ask_settle(ask_output_t* output) {

    if (output->progress) {

        fputs(ask_tty ? "\r\033[K" : "\n", stderr);
        output->progress = false;
    }

    if (output->waiting) {

        fputs(ask_tty ? "\r\033[K" : "", stderr);
        output->waiting = false;
    }
}


static void ask_progress(size_t done, size_t total, void* user) {

    ask_output_t* output = user;

    if (!ask_verbose || output->printed || output->thinking) {
        return;
    }

    const unsigned percent = (unsigned)(done * 100 / total);

    if (ask_tty) {

        fprintf(stderr, "\r\033[K\033[2mReading the question: %u%% (%zu of %zu tokens)\033[0m", percent, done, total);

    } else if (!output->progress) {

        fprintf(stderr, "Reading the question: %zu tokens", total);
    }

    output->progress = true;

    if (done == total) {

        ask_settle(output);

        if (ask_tty) {
            fputs(ask_reasoning ? "\033[2mThinking...\033[0m" : "\033[2mWriting the answer...\033[0m", stderr);
            output->waiting = true;
        }
    }

    fflush(stderr);
}


static bool ask_blank(const char* data, size_t start, size_t end) {

    for (size_t i = start; i < end; i++) {

        if (!isspace((unsigned char)data[i])) {
            return false;
        }
    }

    return true;
}


static bool ask_invisible(unsigned char c) {

    return (c < 0x20 && c != '\t') || c == 0x7F;
}


static double ask_now(void) {

    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);

    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
}


static size_t ask_columns(void) {

    struct winsize size;

    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 10) {
        return size.ws_col - 1;
    }

    return 79;
}


static void ask_window_push(ask_window_t* window, size_t start, size_t end) {

    if (window->count == ASK_THINK_ROWS) {

        memmove(&window->rows[0], &window->rows[1], sizeof(window->rows[0]) * (ASK_THINK_ROWS - 1));
        window->count--;
    }

    window->rows[window->count++] = (ask_row_t){.start = start, .end = end};
}


static void ask_window_layout(ask_window_t* window, const char* data, size_t size, size_t width) {

    size_t start = 0;

    while (start < size) {

        size_t end = start;

        while (end < size && data[end] != '\n') {
            end++;
        }

        if (!ask_blank(data, start, end)) {

            size_t row    = start;
            size_t column = 0;

            for (size_t i = start; i < end; i++) {

                const unsigned char c = (unsigned char)data[i];

                if ((c & 0xC0) == 0x80 || ask_invisible(c)) {
                    continue;
                }

                if (column == width) {

                    ask_window_push(window, row, i);

                    row    = i;
                    column = 0;
                }

                column++;
            }

            ask_window_push(window, row, end);
        }

        start = end + 1;
    }
}


static void ask_window_draw(ask_output_t* output) {

    ask_text_t* thoughts = &output->thoughts;

    size_t from = thoughts->size;

    for (size_t lines = 0; from > 0 && lines < ASK_THINK_ROWS;) {

        size_t start = from;

        while (start > 0 && thoughts->data[start - 1] != '\n') {
            start--;
        }

        if (!ask_blank(thoughts->data, start, from)) {
            lines++;
        }

        from = start > 0 ? start - 1 : 0;
    }

    if (from > 0) {

        thoughts->size -= from;

        memmove(thoughts->data, thoughts->data + from, thoughts->size + 1);
    }

    ask_window_t window = {0};

    ask_window_layout(&window, thoughts->data, thoughts->size, ask_columns());

    ask_text_t frame = {0};

    char move[48];

    if (output->rows > 1) {
        snprintf(move, sizeof(move), "\r\033[%zuA\033[J\033[2;3m", output->rows - 1);
    } else {
        snprintf(move, sizeof(move), "\r\033[J\033[2;3m");
    }

    ask_put(&frame, move);

    for (size_t r = 0; r < window.count; r++) {

        if (r > 0) {
            ask_put(&frame, "\n");
        }

        for (size_t i = window.rows[r].start; i < window.rows[r].end; i++) {

            const unsigned char c = (unsigned char)thoughts->data[i];

            if (c == '\t') {
                ask_put(&frame, " ");
            } else if (!ask_invisible(c)) {
                ask_append(&frame, (const char*)&c, 1);
            }
        }
    }

    char status[64];

    snprintf(status, sizeof(status), "%s\033[0;2mThinking... %.0fs", window.count > 0 ? "\n\n" : "", ask_now() - output->started);

    ask_put(&frame, status);

    fwrite(frame.data, 1, frame.size, stderr);
    fflush(stderr);

    free(frame.data);

    output->rows = window.count > 0 ? window.count + 2 : 1;
}


static void ask_reason(const char* text, size_t size, void* user) {

    ask_output_t* output = user;

    if (!ask_verbose || output->printed) {
        return;
    }

    ask_settle(output);

    if (!output->thinking) {
        output->started = ask_now();
    }

    output->thinking = true;
    output->thought  = text[size - 1];

    if (ask_tty) {

        ask_append(&output->thoughts, text, size);
        ask_window_draw(output);

        return;
    }

    fwrite(text, 1, size, stderr);
    fflush(stderr);
}


static void ask_unthink(ask_output_t* output, bool gap) {

    if (!output->thinking) {
        return;
    }

    if (ask_tty) {

        fputs("\r\033[K\033[0m", stderr);

    } else {

        if (output->thought != '\n') {
            fputc('\n', stderr);
        }

        if (gap) {
            fputc('\n', stderr);
        }
    }

    fflush(stderr);

    free(output->thoughts.data);

    output->thoughts = (ask_text_t){0};
    output->thinking = false;
    output->rows     = 0;
}


static void ask_print(const char* text, size_t size, void* user) {

    ask_output_t* output = user;

    if (ask_verbose && !output->printed) {

        ask_settle(output);
        ask_unthink(output, true);
    }

    fwrite(text, 1, size, stdout);
    fflush(stdout);

    output->printed = true;
    output->last    = text[size - 1];
}

#endif


static int ask_chat(const char* model, const char* system, const char* prompt, const ask_media_t* media) {

#if defined(CONFIG_HAVE_AI)

    if (!*CONFIG_AI_PROVIDER_URL) {

        fprintf(stderr, ASK_NAME ": no provider, set CONFIG_AI_PROVIDER_URL and build again\n");
        return 1;
    }

    const ai_message_t messages[] = {
        {.role = "system", .content = system                                         },
        {.role = "user",   .content = prompt, .parts = media->parts, .count = media->count},
    };

    ask_output_t output = {0};

    const ai_request_t request = {
        .url             = CONFIG_AI_PROVIDER_URL,
        .key             = CONFIG_AI_API_KEY,
        .model           = model ? model : CONFIG_AI_API_MODEL,
        .agent           = ASK_NAME,
        .messages        = messages,
        .count           = sizeof(messages) / sizeof(messages[0]),
        .timeout_connect = ASK_TIMEOUT_CONNECT,
        .response_max    = ASK_RESPONSE_MAX,
        .extras          = ask_reasoning ? "\"chat_template_kwargs\":{\"enable_thinking\":true}" : "\"chat_template_kwargs\":{\"enable_thinking\":false}",
        .stream          = ask_print,
        .reasoning       = ask_reason,
        .progress        = ask_progress,
        .user            = &output,
    };

    ask_status("Asking %s", request.model);

    ai_reply_t reply;

    const int result = ai_chat(&request, &reply);

    ask_settle(&output);
    ask_unthink(&output, false);

    if (output.printed && output.last != '\n') {
        putchar('\n');
    }

    if (result < 0) {
        fprintf(stderr, ASK_NAME ": %s\n", reply.error);
    }

    ai_reply_release(&reply);

    return result < 0 ? 1 : 0;

#else

    (void)model;
    (void)system;
    (void)prompt;
    (void)media;

    fprintf(stderr, ASK_NAME ": built without the assistant, enable CONFIG_HAVE_AI\n");

    return 1;

#endif
}


static void ask_usage(FILE* fp) {

    fprintf(fp, "Usage: " ASK_NAME " [options] [question...]\n"
                "Ask the AI assistant a question and print its answer.\n\n"
                "The question is taken from the arguments, or from standard input when there are none.\n"
                "With both, standard input is attached to the question as a file.\n\n"
                "Options:\n"
                "  -f, --file FILE      attach a file to the question (repeatable): text, a picture,\n"
                "                       a recording or a video, read with " ASK_FFMPEG ",\n"
                "                       or a PDF, read with " ASK_MUTOOL "\n"
                "  -c, --context FILE   give the assistant a file as background context (repeatable)\n"
                "  -m, --model MODEL    ask MODEL instead of the configured one\n"
                "  -r, --reasoning      let the model reason before answering: slower, but better on hard questions\n"
                "  -t, --think          show the progress, and the reasoning with -r, on standard error\n"
                "  -h, --help           show this help\n\n"
                "Examples:\n"
                "  echo \"Ciao\" | " ASK_NAME "\n"
                "  " ASK_NAME " -f main.c \"why does this crash?\"\n"
                "  dmesg | " ASK_NAME " -c notes.txt \"what went wrong?\"\n");
}


int main(int argc, char** argv) {

    static struct option options[] = {
        {"file",    required_argument, NULL, 'f'},
        {"context", required_argument, NULL, 'c'},
        {"model",   required_argument, NULL, 'm'},
        {"reasoning", no_argument,     NULL, 'r'},
        {"think",   no_argument,       NULL, 't'},
        {"help",    no_argument,       NULL, 'h'},
        {NULL,      0,                 NULL, 0  },
    };


    const char** files    = calloc((size_t)argc, sizeof(char*));
    const char** contexts = calloc((size_t)argc, sizeof(char*));

    if (!files || !contexts) {

        fprintf(stderr, ASK_NAME ": out of memory\n");
        return 1;
    }

    size_t nfiles    = 0;
    size_t ncontexts = 0;

    const char* model = NULL;

    int opt;

    while ((opt = getopt_long(argc, argv, "f:c:m:rth", options, NULL)) != -1) {

        switch (opt) {

            case 'f':
                files[nfiles++] = optarg;
                break;

            case 'c':
                contexts[ncontexts++] = optarg;
                break;

            case 'm':
                model = optarg;
                break;

            case 'r':
                ask_reasoning = true;
                break;

            case 't':
                ask_verbose = true;
                break;

            case 'h':
                ask_usage(stdout);
                return 0;

            default:
                ask_usage(stderr);
                return 2;
        }
    }


    ask_tty = isatty(STDERR_FILENO);

    ask_text_t question = {0};

    for (int i = optind; i < argc; i++) {

        if (i > optind) {
            ask_put(&question, " ");
        }

        ask_put(&question, argv[i]);
    }

    ask_text_t input = {0};

    if (!isatty(STDIN_FILENO)) {
        input = ask_read(NULL);
    }

    if (question.size == 0) {

        free(question.data);

        question = input;
        input    = (ask_text_t){0};
    }

    ask_trim(&question);

    if (question.size == 0) {

        fprintf(stderr, ASK_NAME ": nothing to ask\n");
        ask_usage(stderr);

        return 2;
    }


    static ask_media_t media;

    ask_text_t system = {0};

    ask_put(&system, ask_system);

    if (ncontexts > 0) {

        ask_put(&system, "\nThe user gave this background context:\n");

        for (size_t i = 0; i < ncontexts; i++) {
            ask_attach(&system, &media, contexts[i]);
        }
    }


    ask_text_t prompt = {0};

    if (nfiles > 0 || input.size > 0) {

        ask_put(&prompt, "Attached files:\n");

        for (size_t i = 0; i < nfiles; i++) {
            ask_attach(&prompt, &media, files[i]);
        }

        if (input.size > 0) {
            ask_section(&prompt, "standard input", &input);
        }

        ask_put(&prompt, "\n");
    }

    ask_append(&prompt, question.data, question.size);


    const int status = ask_chat(model, system.data, prompt.data, &media);

    for (size_t i = 0; i < media.count; i++) {
        free(media.blobs[i].data);
    }

    free(prompt.data);
    free(system.data);
    free(question.data);
    free(input.data);
    free(contexts);
    free(files);

    return status;
}
