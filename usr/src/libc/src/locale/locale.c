#include <locale.h>
#include <limits.h>

static struct lconv p__locale =  {
	".",
	"",
	"",
	"EUR",
	"€",
	"",
	"",
	"",
	"",
	"EUR",
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
	CHAR_MAX,
};

struct lconv* __libc_locale = &p__locale;
char __libc_locale_name[2] = { 'C', 0 };
