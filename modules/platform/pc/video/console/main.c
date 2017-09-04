#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <libc.h>

MODULE_NAME("pc/video/console");
MODULE_DEPS("arch/x86");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#define CONSOLE_VRAM            0xb8000
#define CONSOLE_WIDTH           80
#define CONSOLE_HEIGHT          25


#if defined(__i386__) || defined(__x86_64__)
#    if defined(__i386__)
#        include <arch/i386/i386.h>
#    elif defined(__x86_64__)
#        include <arch/x86_64/x86_64.h>
#    endif


struct cc {
    int p;
    int escape;
    int escape_offset;
    int escape_saved_cursor;
    int style;
    int colors;
    char escape_buffer[32];
} __packed;




#define VRAM            ((short*) CONSOLE_VRAM)


__fastcall
static inline uint32_t __C(uint32_t p) {
    if(unlikely(p > CONSOLE_WIDTH * CONSOLE_HEIGHT))
        p = CONSOLE_WIDTH * CONSOLE_HEIGHT;
 
    return p;
}


static void plot_value(struct cc* cc, char value) {
    if(cc->escape) {
        if(likely(value == '[')) {
            cc->escape++;
            return;
        }

        if(cc->escape != 2) {
            cc->escape = 0;
            return;
        }


        if(isdigit(value) || value == ';')
            cc->escape_buffer[cc->escape_offset++] = value;
        else {
            int x, y;
            switch(value) {
                case '@':
                    y = atoi(cc->escape_buffer);
                    for(x = 0; x < y; x++)
                        VRAM[__C(cc->p + x)] = (cc->style << 8) | ' ';
                    break;
                case 'A':
                    y = atoi(cc->escape_buffer);
                    for(x = 0; x < y; x++)
                        cc->p -= CONSOLE_WIDTH;
                    break;
                case 'B':
                case 'e':
                    y = atoi(cc->escape_buffer);
                    for(x = 0; x < y; x++)
                        cc->p += CONSOLE_WIDTH;
                    break;
                case 'C':
                case 'a':
                    cc->p += atoi(cc->escape_buffer);
                    break;
                case 'D':
                    cc->p -= atoi(cc->escape_buffer);
                    break;
                case 'E':
                    y = atoi(cc->escape_buffer);
                    for(x = 0; x < y; x++)
                        cc->p += CONSOLE_WIDTH - (cc->p % CONSOLE_WIDTH);
                    break;
                case 'F':
                    y = atoi(cc->escape_buffer);
                    for(x = 0; x < y; x++) 
                        cc->p -= CONSOLE_WIDTH + (cc->p % CONSOLE_WIDTH);
                    break;
                case 'G':
                    cc->p -= (cc->p % CONSOLE_WIDTH);
                    cc->p += atoi(cc->escape_buffer);
                    break;
                case 'H':
                case 'f':
                    sscanf(cc->escape_buffer, "%d;%d", &y, &x);
                    cc->p = y * CONSOLE_WIDTH + x;
                    break;
                case 'J':
                    y = atoi(cc->escape_buffer);
                    switch(y) {
                        case 1:
                            x = cc->p;
                            while(x < CONSOLE_WIDTH * CONSOLE_HEIGHT)
                                VRAM[x++] = (cc->style << 8) | ' ';
                            break;
                        default:
                            for(x = 0; x < CONSOLE_WIDTH * CONSOLE_HEIGHT; x++)
                                VRAM[x] = (cc->style << 8) | ' ';

                            cc->p = 0;
                            break;
                    }
                    break;
                case 'K':
                    y = atoi(cc->escape_buffer);
                    switch(y) {
                        case 1:
                            x = cc->p - (cc->p % CONSOLE_WIDTH);
                            while(x < cc->p)
                                VRAM[__C(x++)] = (cc->style << 8) | ' ';
                            break;
                        case 2:
                            for(x = 0; x < CONSOLE_WIDTH; x++)
                                VRAM[__C(cc->p - (cc->p % CONSOLE_WIDTH) + x)] = (cc->style << 8) | ' ';
                            break;
                    }
                    break;
                case 'L':
                    y = atoi(cc->escape_buffer) * CONSOLE_WIDTH;
                    y -= (cc->p % CONSOLE_WIDTH);

                    x = cc->p - (cc->p % CONSOLE_WIDTH);
                    while(x < y)
                        VRAM[__C(x++)] = (cc->style << 8) | ' ';
                    
                    break;
                case 'M':
                    y = atoi(cc->escape_buffer) * CONSOLE_WIDTH;
                    y += (cc->p % CONSOLE_WIDTH);

                    x = cc->p + (CONSOLE_WIDTH - (cc->p % CONSOLE_WIDTH));
                    while(x > y)
                        VRAM[__C(x--)] = (cc->style << 8) | ' ';
                    
                    break;
                case 'P':
                    y = atoi(cc->escape_buffer);
                    while(y--)
                        VRAM[__C(cc->p--)] = (cc->style << 8) | ' ';
                    break;
                case 'X':
                    y = atoi(cc->escape_buffer);
                    while(y)
                        VRAM[__C(cc->p - y--)] = (cc->style << 8) | ' ';
                    break;
                case 'd':
                    y = atoi(cc->escape_buffer);
                    cc->p = (CONSOLE_WIDTH * y) + (cc->p % CONSOLE_WIDTH);
                    break;
                case 's':
                    cc->escape_saved_cursor = cc->p;
                    break;
                case 'u':
                    cc->p = cc->escape_saved_cursor;
                    break;
                case 'm':
                    for(char* p = strtok(cc->escape_buffer, ";"); p; p = strtok(NULL, ";")) {
                        char colors[2][8] = {
                            { 0x0, 0x4, 0x2, 0x6, 0x1, 0x5, 0x3, 0x7 },
                            { 0x8, 0xC, 0xA, 0xE, 0x9, 0xD, 0xB, 0xF }
                        };

                        x = atoi(p);
                        switch(x) {
                            case 0:
                                cc->style = 0x07;
                                break;
                            case 2:
                                cc->colors = 0;
                                break;
                            case 22:
                                cc->colors = 1;
                                break; 
                            case 30 ... 37:
                                cc->style = (cc->style & 0xF0) | (colors[cc->colors][x - 30] & 0x0F);
                                break;
                            case 40 ... 47:
                                cc->style = (cc->style & 0x0F) | (colors[cc->colors][x - 40] << 4);
                                break;
                            case 38:
                            case 39:
                                cc->style = (cc->style & 0xF0) | 0x07;
                                break;
                            case 49:
                                cc->style = (cc->style & 0x0F);
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                default:
                    break;
            }

            cc->escape = 0;
            cc->escape_offset = 0;

            memset(cc->escape_buffer, 0, 32);
        }

    } else {
        switch(value) {
            case '\n':
                cc->p += CONSOLE_WIDTH - (cc->p % CONSOLE_WIDTH);
                break;
            case '\v':
                cc->p += CONSOLE_WIDTH;
                break;
            case '\r':
                cc->p -= (cc->p % CONSOLE_WIDTH);
                break;
            case '\t':
                cc->p += 4 - ((cc->p % CONSOLE_WIDTH) % 4);
                break;
            case '\b':
                VRAM[__C(--cc->p)] = (cc->style << 8) | ' ';
                break;
            case '\e':
                cc->escape = 1;
                break;
            default:
                VRAM[__C(cc->p++)] = (cc->style << 8) | value;
                break;
        }
    }



    if(cc->p >= (CONSOLE_WIDTH * CONSOLE_HEIGHT)) {
        int x, y;
        for(y = 1; y < CONSOLE_HEIGHT; y++)
            for(x = 0; x < CONSOLE_WIDTH; x++)
                VRAM[(y - 1) * CONSOLE_WIDTH + x] = VRAM[y * CONSOLE_WIDTH + x];

        cc->p -= CONSOLE_WIDTH;

        for(x = 0; x < CONSOLE_WIDTH; x++)
            VRAM[__C(cc->p + x)] = (cc->style << 8) | ' ';

    }


    outb(0x3D4, 0x0F);
    outb(0x3D5, cc->p & 0xFF);
    outb(0x3D4, 0x0E);
    outb(0x3D5, (cc->p >> 8) & 0xFF);
}

static int console_write(struct inode* inode, void* buf, size_t size) {
    if(unlikely(!inode || !buf || !inode->userdata)) {
        errno = EINVAL;
        return -1;
    }

    if(unlikely(!size))
        return 0;


    struct cc* cc = inode->userdata;

    char* s = buf;
    for(int i = 0; i < size; i++)
        plot_value(cc, s[i]);

    return size;
}

int init(void) {
    struct cc* cc = (void*) kmalloc(sizeof(struct cc), GFP_KERNEL);
    if(unlikely(!cc)) {
        errno = ENOMEM;
        return -1;
    }

    memset(cc, 0, sizeof(struct cc));
    cc->style = 0x0F;
    cc->colors = 1;


    int i;
    for(i = 0; i < CONSOLE_WIDTH * CONSOLE_HEIGHT; i++)
        VRAM[i] = (cc->style << 8) | ' ';


    inode_t* ino = vfs_mkdev("console", -1, S_IFCHR | 0222);
    ino->write = console_write;
    ino->userdata = cc;

    return E_OK;
}


#else

int init(void) {
    return E_ERR;
}

#endif


int dnit(void) {
    return E_OK;
}
