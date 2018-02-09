#ifndef	_SYS_UTSNAME_H
#define	_SYS_UTSNAME_H

#define	_SYS_NAMELEN	256

struct	utsname {
	char	sysname[_SYS_NAMELEN];	/* Name of OS */
	char	nodename[_SYS_NAMELEN];	/* Name of this network node */
	char	release[_SYS_NAMELEN];	/* Release level */
	char	version[_SYS_NAMELEN];	/* Version level */
	char	machine[_SYS_NAMELEN];	/* Hardware type */
};


#ifdef __cplusplus
extern "C" {
#endif
int uname (struct utsname *);
#ifdef __cplusplus
}
#endif

#endif	/* !_SYS_UTSNAME_H */
