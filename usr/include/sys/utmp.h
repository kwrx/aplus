#ifndef _UTMP_H
#define _UTMP_H

#include <time.h>

#define UT_NAMESIZE		8
#define UT_LINESIZE		8
#define UT_HOSTSIZE		16

struct lastlog {
	time_t ll_time;
	char ll_line[UT_LINESIZE];
	char ll_host[UT_HOSTSIZE];
};

struct utmp {
	char ut_line[UT_LINESIZE];
	char ut_user[UT_NAMESIZE];
#define ut_name ut_user
	char ut_host[UT_HOSTSIZE];
	long int ut_time;
};

#define _HAVE_UT_HOST	1

#endif
