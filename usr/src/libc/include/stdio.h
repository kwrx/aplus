#ifndef _STDIO_H
#define _STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#define _FSTDIO

#include <stddef.h>
#include <stdarg.h>



#define __SLBF		0x0001
#define __SNBF		0x0002
#define __SRD		0x0004
#define __SWR		0x0008

#define __SRW		0x0010
#define __SEOF		0x0020
#define __SERR		0x0040
#define __SMBF		0x0080
#define __SAPP		0x0100
#define __SSTR		0x0200
#define __SOPT		0x0400
#define __SNPT		0x0800
#define __SOFF		0x1000
#define __SMOD		0x2000
#define __SCLE		0x4000



#define _IOFBF		0
#define _IOLBF		1
#define _IONBF		2

#ifndef NULL
#define NULL		((void) 0)
#endif

#define BUFSIZ		1024
#define EOF			(-1)


#define FOPEN_MAX		20
#define FILENAME_MAX	1024
#define L_tmpnam		1024
#define P_tmpdir		"/tmp"


#ifndef SEEK_SET
#define SEEK_SET		0
#endif

#ifndef SEEK_CUR
#define SEEK_CUR		1
#endif

#ifndef SEEK_END
#define SEEK_END		2
#endif

#define TMP_MAX			26

#ifdef __GNUC__
#define __VALIST		__gnuc_va_list
#else
#define __VALIST		char*
#endif

#ifndef _EXFUN
#define _EXFUN(a, b)	a b
#endif





#ifdef __cplusplus
}
#endif

#endif
