/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>


#define CTRL_KEY(ch) \
    ((ch) & 0x1F)

#define __clamp(x, min, max)    \
    (x < min ? min : (x > max ? max : x))


struct row {
    char* chars;
    size_t size;
};

struct {
    char* filename;    
    struct termios ios;
    struct winsize ws;

    struct {
        int16_t x;
        int16_t y;
    } cursor;

    size_t rowlen;
    struct row* rows;
} editor;

struct abuf {
    void* buf;
    size_t len;
};


static void show_usage(int argc, char** argv) {
    printf(
        "Use: text [FILE]\n"
        "Lightweight text editor\n\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus sysutils) 0.1\n"
        "Copyright (c) %s Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], COMMIT, __DATE__ + 7, __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}



static void die(const char* s) {
    perror(s);
    exit(1);
}

static void disable_rawmode() {
    write(STDOUT_FILENO, "\e[2J\e[H", 7);

    if(tcsetattr(STDIN_FILENO, TCSANOW, &editor.ios) < 0)
        die("tcsetattr");
}

static void enable_rawmode() {
    if(tcgetattr(STDIN_FILENO, &editor.ios) < 0)
        die("tcgetattr");

    atexit(disable_rawmode);


    if(ioctl(STDIN_FILENO, TIOCGWINSZ, &editor.ws) < 0)
        die("ioctl(TIOCGWINSZ)");

    struct termios raw = editor.ios;
    raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP | ICRNL | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    
    if(tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0)
        die("tcsetattr");
}


static void abuf_append(struct abuf* b, const char* s, int len) {
    char* p = (char*) realloc(b->buf, b->len + len);
    if(!p)
        die("memory");

    memcpy(&p[b->len], s, len);
    b->buf = (void*) p;
    b->len += len; 
}

static void abuf_free(struct abuf* b) {
    free(b->buf);
}

static void row_append(char* s, size_t len) {
    editor.rows = (struct row*) realloc(editor.rows, sizeof(struct row) * (editor.rowlen + 1));
    if(!editor.rows)
        die("memory");

    editor.rows[editor.rowlen].size = len;
    editor.rows[editor.rowlen].chars = strdup(s);
    editor.rowlen++;
}


static void editor_open(char* filename) {
    editor.filename = filename;
    
    FILE* fp = fopen(filename, "r+");
    if(!fp)
        return;

    int n;
    char line[4096];
    memset(line, 0, sizeof(line));

    while(fgets(line, sizeof(line), fp)) {
        if(line[strlen(line) - 1] == '\n')
            line[strlen(line) - 1] = '\0';
        
        row_append(line, strlen(line));
        memset(line, 0, sizeof(line));
    }
    
    fclose(fp);
}

static void editor_refresh() {
    struct abuf b = { NULL, 0};
    abuf_append(&b, "\e[H", 3);
    abuf_append(&b, "\e[30;47m\e[2K  Text editor -- ", 29);
    abuf_append(&b, editor.filename, strlen(editor.filename));
    abuf_append(&b, "\e[0;49;39m\n", 11);

    int i;
    for(i = 1; i < editor.ws.ws_row - 1; i++) {
        if(i < editor.rowlen)
            abuf_append(&b, editor.rows[i].chars, editor.rows[i].size > editor.ws.ws_col
                                                    ? editor.ws.ws_col
                                                    : editor.rows[i].size);
        else
            abuf_append(&b, "~", 1);

        abuf_append(&b, "\e[K\n", 4);
    }

    abuf_append(&b, "~\e[K", 4);


    char buf[32];
    snprintf(buf, sizeof(buf), "\e[%d;%dH", editor.cursor.y + 1, editor.cursor.x + 1);
    abuf_append(&b, buf, strlen(buf));
    abuf_append(&b, "|", 1);

    write(STDOUT_FILENO, b.buf, b.len);
    abuf_free(&b);
}


static int editor_getch() {
    char ch;
    while(read(STDIN_FILENO, &ch, 1) != 1)
        if(errno != EAGAIN)
            die("read");

    return ch;
}

static void editor_keyinput() {
    char ch;
    switch((ch = editor_getch())) {
        case CTRL_KEY('q'):
            exit(0);
            break;

        case '\e':
            if(editor_getch() != '[')
                break;

            switch(editor_getch()) {
                case 'A':
                    editor.cursor.y--;
                    break;
                case 'B':
                    editor.cursor.y++;
                    break;
                case 'C':
                    editor.cursor.x++;
                    break;
                case 'D':
                    editor.cursor.x--;
                    break;
                case '5':
                case '6':
                    if(editor_getch() != '~')
                        break;
                    
                    for(int i = 0; i < editor.ws.ws_row; i++)
                        editor.cursor.y -= ch == '5' ? 1 : -1;

                    break;
            }

            editor.cursor.x = __clamp(editor.cursor.x, 0, editor.ws.ws_col - 1);
            editor.cursor.y = __clamp(editor.cursor.y, 1, editor.ws.ws_row - 1);
            break;
    }
}


int main(int argc, char** argv) {
    
    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "", long_options, &idx)) != -1) {
        switch(c) {
            case 'v':
                show_version(argc, argv);
                break;
            case 'h':
            case '?':
                show_usage(argc, argv);
                break;
            default:
                abort();
        }
    }


    int fd = open("/dev/log", O_WRONLY);
    if(fd >= 0)
        dup2(fd, STDERR_FILENO);



    memset(&editor, 0, sizeof(editor));

    char* filename;
    if(optind < argc)
        filename = strdup(argv[optind]);
    else
        filename = strdup(tmpnam(NULL));


    enable_rawmode();
    editor_open(filename);

    for(;;) {
        editor_refresh();
        editor_keyinput();
    }

    return 0;
}