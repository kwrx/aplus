#ifndef _SYS_FSUID_H
#define _SYS_FSUID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

int setfsuid(uid_t);
int setfsgid(gid_t);

#ifdef __cplusplus
}
#endif

#endif
