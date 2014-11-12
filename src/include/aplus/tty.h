#ifndef _TTY_H
#define _TTY_H

#include <aplus.h>
#include <aplus/spinlock.h>
#include <aplus/task.h>
#include <aplus/fs.h>

#include <stdio.h>
#include <stdint.h>

#include <sys/ioctl.h>
#include <sys/termios.h>


#define TTYDEF_WIN_COLS				80
#define TTYDEF_WIN_ROWS				25
#define TTYDEF_IOS_IFLAG			(BRKINT | ISTRIP | ICRNL | IMAXBEL | IXON | IXANY)
#define TTYDEF_IOS_OFLAG			(OPOST | ONLCR | OXTABS)
#define TTYDEF_IOS_LFLAG			(ECHO | ICANON | ISIG | IEXTEN | ECHOE | ECHOKE | ECHOCTL)
#define TTYDEF_IOS_CFLAG			(CREAD | CS7 | PARENB | HUPCL)
#define TTYDEF_IOS_SPEED			(B9600)


typedef struct tty {
	struct termios ios;
	struct winsize win;
	
	spinlock_t lock;
	
	char ibuffer[BUFSIZ];
	char obuffer[BUFSIZ];
	
	short isize;
	short osize;

	int exclmode;
	
	pid_t fg;
	pid_t bg;
	int id;


	void (*output) (struct tty*, void* ptr, size_t size);
} tty_t;


#endif
