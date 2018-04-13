#ifndef _SYS_MOUNT_H
#define _SYS_MOUNT_H

#define MNT_RDONLY			0x01
#define MNT_SYNCHRONOUS			0x02
#define MNT_NOEXEC			0x04
#define MNT_NOSUID			0x08
#define MNT_NODEV			0x10


#ifdef __cplusplus
extern "C" {
#endif

int mount(const char*, const char*, const char*, int, const void*);
int umount(const char*);
int umount2(const char*, int);

#ifdef __cplusplus
}
#endif

#endif
