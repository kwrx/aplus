#include <aplus.h>
#include <aplus/spinlock.h>

static spinlock_t debug_lock = 0;

/**
 *	\brief Send a character to serial debug port.
 *	\param ch Character to send.
 */
void debug_putc(char ch) {
	arch_debug_putc(ch);
}


/**
 *	\brief Send a string to serial debug port.
 *	\param str String to send.
 *	\see debug_putc
 */
void debug_puts(char* str) {
	spinlock_lock(&debug_lock);
	
	while(*str)
		debug_putc(*str++);
	
	spinlock_unlock(&debug_lock);
}


/**
 *	\brief Print a number in base 10 into buffer.
 *	\param value Number to convert as string.
 *	\param width Padding.
 *	\param buf Buffer output.
 *	\param ptr Size of returned string.
 */
static void print_dec(unsigned int value, unsigned int width, char* buf, int* ptr ) {
	unsigned int n_width = 1;
	unsigned int i = 9;
	while (value > i && i < UINT32_MAX) {
		n_width += 1;
		i *= 10;
		i += 9;
	}

	int printed = 0;
	while (n_width + printed < width) {
		buf[*ptr] = '0';
		*ptr += 1;
		printed += 1;
	}

	i = n_width;
	while (i > 0) {
		unsigned int n = value / 10;
		int r = value % 10;
		buf[*ptr + i - 1] = r + '0';
		i--;
		value = n;
	}
	*ptr += n_width;
}

/**
 *	\brief Print a number in base 16 into buffer.
 *	\param value Number to convert as string.
 *	\param width Padding.
 *	\param buf Buffer output.
 *	\param ptr Size of returned string.
 */
static void print_hex(unsigned int value, unsigned int width, char* buf, int* ptr) {
	int i = width;
	int z = 0;

	if (i == 0) i = 8;

	unsigned int n_width = 1;
	unsigned int j = 0x0F;
	while (value > j && j < UINT32_MAX) {
		n_width += 1;
		j *= 0x10;
		j += 0x0F;
	}

	while (i > (int)n_width) {
		buf[*ptr] = '0';
		*ptr += 1;
		i--;
	}

	i = (int)n_width;
	while (i-- > 0) {
		if(((value >> (i * 4)) & 0xF) == 0 && !z)
			continue;
		else
			z++;

		buf[*ptr] = "0123456789abcdef"[(value>>(i*4))&0xF];
		*ptr += + 1;
	}
}

/**
 *	\brief Print to allocated string.\n
 *	The functions asprintf() and vasprintf() are analogs of sprintf(3) and vsprintf(3), 
	except that they allocate a string large enough to hold the output including the 	
	terminating null byte, and return a pointer to it via the first argument. 			
	This pointer should be passed to free(3) to release the allocated storage when it 	
	is no longer needed.
 * 	\param buf Output buffer just allocated.
 *	\param fmt Format of string.
 *	\param args Arguments.
 *	\return When successful, these functions return the number of bytes printed, just 	
	like sprintf(3). If memory allocation wasn't possible, or some other error occurs, 	
	these functions will return -1, and the contents of strp is undefined.
 */
size_t vasprintf(char * buf, const char *fmt, va_list args) {
	int i = 0;
	char *s;
	int ptr = 0;
	int len = strlen(fmt);
	for ( ; i < len && fmt[i]; ++i) {
		if (fmt[i] != '%') {
			buf[ptr++] = fmt[i];
			continue;
		}
		++i;
		unsigned int arg_width = 0;
		while (fmt[i] >= '0' && fmt[i] <= '9') {
			arg_width *= 10;
			arg_width += fmt[i] - '0';
			++i;
		}
		/* fmt[i] == '%' */
		switch (fmt[i]) {
			case 's': /* String pointer -> String */
				s = (char *)va_arg(args, char *);
				if (s == NULL) {
					s = "(null)";
				}
				while (*s) {
					buf[ptr++] = *s++;
				}
				break;
			case 'c': /* Single character */
				buf[ptr++] = (char)va_arg(args, int);
				break;
			case 'x': /* Hexadecimal number */
			case 'X':
				print_hex((unsigned long)va_arg(args, unsigned long), arg_width, buf, &ptr);
				break;
			case 'd': /* Decimal number */
				print_dec((unsigned long)va_arg(args, unsigned long), arg_width, buf, &ptr);
				break;
			case 'f':
				(void) va_arg(args, double);
				break;
			case '%': /* Escape */
				buf[ptr++] = '%';
				break;
			default: /* Nothing at all, just dump it */
				buf[ptr++] = fmt[i];
				break;
		}
	}
	/* Ensure the buffer ends in a null */
	buf[ptr] = '\0';
	return ptr;

}


/**
 *	\brief Formatted output conversion and print to buffer.
 * 	\param buf Buffer output.
 *	\param fmt Format of string.
 *	\param ... Arguments.
 */
void ksprintf(char* buf, char* fmt, ...) {
	
	va_list lst;
	va_start(lst, fmt);
	vasprintf(buf, fmt, lst);
	va_end(lst);

}

/**
 *	\brief Formatted output conversion and print to Debug Standard Output.
 *	\param fmt Format of string.
 *	\param ... Arguments.
 */
#undef kprintf
void kprintf(char* fmt, ...) {
	static char __kprintf_buf[1024];
	memset(__kprintf_buf, 0, 1024);
	
	va_list lst;
	va_start(lst, fmt);
	vasprintf(__kprintf_buf, fmt, lst);
	va_end(lst);

	debug_puts(__kprintf_buf);
}


EXPORT_SYMBOL(kprintf);
EXPORT_SYMBOL(ksprintf);

