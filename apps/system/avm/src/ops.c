#include "ops.h"


#if !HAVE_ERRNO_H
__weak
int errno = 0;
#endif

#if !HAVE_STDLIB_H
__weak
void* calloc(size_t x, size_t y) {
	return NULL;
}

__weak
void free(void* ptr) {
	return;
}

__weak
void abort(void) {
	for(;;);
}

void exit(int s) {
	for(;;);
}
#endif

#if !HAVE_STDIO_H
__weak
int printf(const char* fmt, ...) {
	return 0;
}
#endif


#if !HAVE_UNISTD_H
__weak 
int close(int fd) {
	return -1;
}

__weak
off_t lseek(int fd, off_t off, int dir) {
	return -1;
}

int open(const char* name, int flags, ...) {
	return -1;
}

ssize_t read(int fd, void* buf, size_t size) {
	return 0;
}

pid_t getpid(void) {
	return 1; /* Fake Process ID */
}
#endif


#if !HAVE_SCHED_H
int sched_yield(void) {
	return -1;
}
#endif

#if !HAVE_STRING_H
__weak
void* memset(void* s1, int value, size_t size) {
	register char* p1 = s1;
	while(size--)
		*p1++ = value;

	return s1;
}
__weak
void* memcpy(void* s1, const void* s2, size_t size) {
	register char* p1 = s1;
	register const char* p2 = s2;
	
	while(size--)
		*p1++ = *p2++;
		
	return s1;
}
__weak
char* strcpy(char* s1, const char* s2) {
	register char* s = s1;
	while(*s2)
		*s1++ = *s2++;
		
	return s;
}
__weak
size_t strlen(const char* s) {
	register int p = 0;
	while(*s++)
		p++;
		
	return p;
}
__weak
char* strdup(const char* s) {
#if HAVE_GC_H
	return GC_STRDUP(s);
#endif

	char* p = avm->calloc(1, strlen(s));
	ASSERT(p);

	strcpy(p, s);
	
	return p;
}
__weak
char* strcat(char* s1, const char *s2) {
	strcpy(&s1[strlen(s1)], s2);
	return s1;
}
__weak
int strcmp(const char* s1, const char* s2) {
	for(; *s1 == *s2; s1++, s2++)
		if(*s1 == 0)
			return 0;

	return ((*(unsigned char*) s1 < *(unsigned char*) s2) ? -1 : 1);
}

__weak
int strncmp(const char* s1, const char* s2, size_t n) {
	for(; n > 0; s1++, s2++, --n)
		if(*s1 != *s2)
			return ((*(unsigned char*) s1 < *(unsigned char*) s2) ? -1 : 1);
		else if (*s1 == 0)
			return 0;

	return 0;
}
#endif


#if !HAVE_MATH_H
__weak
double fmod(double x, double y) {
#if defined(__i386__) || defined(__x86_64__)
	register double v;	
	__asm__ __volatile__ (
		"1: fprem; fnstsw %%ax; sahf; jp 1b"
		: "=t"(v)
		: "0"(x), "u"(y) : "ax", "cc"
	);

	return v;
#else
	return (double)((long) x % (long) y);
#endif
}
#endif



static void __free_stub(void* ptr) {
	return;
}


static struct avm_ops __ops = {
	__calloc,
#ifdef __GLIBC__
	__free_stub,
#else
	__free,
#endif
	open,
	close,
	lseek,
	read,
	sched_yield,
	getpid,
	printf,
};




struct avm_ops* avm = &__ops;



const char* strfmt(const char* fmt, ...) {
	va_list ll;
	va_start(ll, fmt);

	char buf[1024];
	memset(buf, 0, 1024);

	char* p = buf;
		
	do {
		switch(*fmt) {
			case '%':
				fmt++;
				switch(*fmt) {
					case '\0':
						break;
					case 's': {
						const char* s = va_arg(ll, const char*);
						while(*s)
							*p++ = *s++;
					} break;
					case 'd': {
						int i = va_arg(ll, int);
						if(i == 0) {
							*p++ = '0';
							break;
						}

						if(i < 0) {
							*p++ = '-';
							i = -i;
						}

						int j;
						for(j = i; j; p++)
							j /= 10;

						register char* t = p;
						do {
							*--p = '0' + (i % 10);
							i /= 10;
						} while(i != 0);
					
						p = t;
					} break;
				}
				break;

			default:
				*p++ = *fmt;
		}

		fmt++;
	} while(*fmt);

	va_end(ll);

	return strdup(buf);
}
