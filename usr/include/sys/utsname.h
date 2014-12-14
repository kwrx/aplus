#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

#include <sys/cdefs.h>

#define _UTSNAME_LENGTH					256

#ifndef _UTSNAME_NODENAME_LENGTH
#define _UTSNAME_NODENAME_LENGTH		_UTSNAME_LENGTH
#endif


#ifdef __cplusplus
extern "C" {
#endif

struct utsname {
	char sysname[_UTSNAME_LENGTH];
	char nodename[_UTSNAME_NODENAME_LENGTH];
	char release[_UTSNAME_LENGTH];
	char version[_UTSNAME_LENGTH];
	char machine[_UTSNAME_LENGTH];
	char domainname[_UTSNAME_LENGTH];
};


#ifdef __USE_SVID
#define SYS_NMLN						_UTSNAME_LENGTH
#endif


extern int uname(struct utsname* name) __THROW;


#ifdef __cplusplus
}
#endif

#endif
