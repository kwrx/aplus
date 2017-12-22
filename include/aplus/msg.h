#ifndef _APLUS_MSG_H
#define _APLUS_MSG_H



#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#define SIGMSG			SIGUSR2

int msg_send(pid_t pid, void* data, size_t len);
int msg_recv(pid_t* pid, void* data, size_t len);


#ifdef __cplusplus
}
#endif
#endif
