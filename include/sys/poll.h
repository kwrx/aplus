#ifndef _SYS_POLL_H
#define _SYS_POLL_H

#define POLLIN				01
#define POLLPRI				02
#define POLLOUT				04

#if defined __USE_XOPEN || defined __USE_XOPEN2K8
#	define POLLRDNORM		POLLIN
#	define POLLRDBAND		POLLPRI
#	define POLLWRNORM		POLLOUT
#	define POLLWRBAND		POLLOUT
#endif

#define POLLERR				010
#define POLLHUP				020
#define POLLNVAL			040

typedef unsigned long int nfds_t;

struct pollfd {
	int fd;
	short int events;
	short int revents;
};


#ifdef __cplusplus
extern "C" {
#endif

extern int poll(struct pollfd* fds, nfds_t nfds, int timeout);

#ifdef __cplusplus
}
#endif

#endif
