#include <locale.h>


char* setlocale(int category, const char* locale) {
	extern char* __libc_locale_name;
	char* p = __libc_locale_name;	

	while(*locale)
		*p++ = *locale++;

	return __libc_locale_name;
}
