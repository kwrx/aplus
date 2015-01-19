#include <locale.h>

struct lconv* localeconv(void) {
	extern struct lconv* __libc_locale;
	return __libc_locale;
}
