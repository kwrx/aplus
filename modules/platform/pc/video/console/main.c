#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <libc.h>

MODULE_NAME("pc/video/console");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#define CONSOLE_VRAM            0xb8000
#define CONSOLE_WIDTH           80
#define CONSOLE_HEIGHT          25


#if defined(__i386__) || defined(__x86_64__)
#	if defined(__i386__)
#		include <arch/i386/i386.h>
#	elif defined(__x86_64__)
#		include <arch/x86_64/x86_64.h>
#	endif


struct cc {
	int p;
	int escape;
	int escape_offset;
	int escape_saved_cursor;
	int style;
	char escape_buffer[32];
} __packed;




#define VRAM		((short*) CONSOLE_VRAM)

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
				case 'J':
					for(x = 0; x < CONSOLE_WIDTH * CONSOLE_HEIGHT; x++)
                        VRAM[x] = (cc->style << 8) | ' ';
                        

					cc->p = 0;
					break;
				case 'K':
					for(x = 0; x < CONSOLE_WIDTH; x++)
						VRAM[cc->p - (cc->p % CONSOLE_WIDTH) + x] = (cc->style << 8) | ' ';

					cc->p -= (cc->p % CONSOLE_WIDTH);
					break;
				case 'A':
					y = atoi(cc->escape_buffer);
					for(x = 0; x < y; x++)
						cc->p -= CONSOLE_WIDTH;
					break;
				case 'B':
					y = atoi(cc->escape_buffer);
					for(x = 0; x < y; x++)
						cc->p += CONSOLE_WIDTH;
					break;
				case 'C':
					cc->p += atoi(cc->escape_buffer);
					break;
				case 'D':
					cc->p -= atoi(cc->escape_buffer);
					break;
				case 'H':
					sscanf(cc->escape_buffer, "%d;%d", &y, &x);
					cc->p = y * CONSOLE_WIDTH + x;
					break;
				case 's':
					cc->escape_saved_cursor = cc->p;
					break;
				case 'u':
					cc->p = cc->escape_saved_cursor;
					break;
				case 'm':
					for(char* p = strtok(cc->escape_buffer, ";"); p; p = strtok(NULL, ";")) {
						char colors[] = { 0, 12, 10, 14, 9, 13, 11, 15 };

						x = atoi(p);
						switch(x) {
							case 30 ... 37:
								cc->style = (cc->style & 0xF0) | (colors[x - 30] & 0x0F);
								break;
							case 40 ... 47:
								cc->style = (cc->style & 0x0F) | (colors[x - 40] << 4);
								break;
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
				VRAM[--cc->p] = (cc->style << 8) | ' ';
				break;
			case '\e':
				cc->escape = 1;
				break;
			default:
				VRAM[cc->p++] = (cc->style << 8) | value;
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
			VRAM[cc->p + x] = (cc->style << 8) | ' ';

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
        plot_value(inode->userdata, s[i]);

    return size;
}

int init(void) {
	struct cc* cc = (void*) kmalloc(sizeof(struct cc), GFP_KERNEL);
	if(unlikely(!cc)) {
		errno = ENOMEM;
		return -1;
	}

    memset(cc, 0, sizeof(struct cc));
    cc->style = 0x07;


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
