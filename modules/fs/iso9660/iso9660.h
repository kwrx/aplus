#ifndef _ISO9660_H
#define _ISO9660_H

#include <aplus.h>
#include <libc.h>


#define ISO9660_SECTOR_SIZE                         2048

#define ISO9660_PVD                                 (0x10 * ISO9660_SECTOR_SIZE)
#define ISO9660_ID                                  "CD001"
#define ISO9660_VERSION                             0x01

#define ISO9660_VOLDESC_BOOTRECORD                  0
#define ISO9660_VOLDESC_PRIMARY                     1
#define ISO9660_VOLDESC_SUPPLEMENTARY               2
#define ISO9660_VOLDESC_PARTITION                   3
#define ISO9660_VOLDESC_TERMINATOR                  255

#define ISO9660_VOLDESC_SIZE                        ISO9660_SECTOR_SIZE


#define ISO9660_FLAGS_HIDDEN                        (1 << 0)
#define ISO9660_FLAGS_DIRECTORY                     (1 << 1)
#define ISO9660_FLAGS_FILE                          (1 << 2)
#define ISO9660_FLAGS_EXTATTR                       (1 << 3)
#define ISO9660_FLAGS_EXTFLAGS                      (1 << 4)
#define ISO9660_FLAGS_NOTLAST                       (1 << 7)

#define ISO9660_NAME_LENGTH                         (1024)

#define HAVE_ROCKRIDGE                              1

typedef struct iso9660_volume_descriptor {
    int8_t type;
    char id[5];
    int8_t version;
    char data[2041];
} __attribute__ ((packed)) iso9660_volume_descriptor_t;


typedef struct iso9660_dir {
    uint8_t size;
    uint8_t exattr;
    uint64_t lba;
    uint64_t length;
    char date[7];
    uint8_t flags;
    uint8_t unitsize;
    uint8_t gapsize;
    uint32_t volnumber;
    uint8_t idlen;
    char reserved[0];
} __attribute__ ((packed)) iso9660_dir_t;

typedef struct iso9660_pvd {
    int8_t type;
    char id[5];
    int8_t version;
    int8_t unused;
    
    char sysid[32];
    char volid[32];
    char null0[8];
    
    uint64_t volsize;
    
    char null1[32];
    
    uint32_t volsetsize;
    uint32_t volnumber;
    uint32_t logical_blksize;
    uint64_t path_size;
    uint32_t path_lba;
    uint32_t optpath_lba;
    uint32_t path_lba_msb;
    uint32_t optpath_lba_msb;
    
    union {
        char __rootdir[34];
        iso9660_dir_t rootdir;
    };

    char volsetid[128];
    char pubid[128];
    char dataid[128];
    char appid[128];
    char copyid[38];
    char abstractfileid[36];
    char bibid[37];
    char cdate[17];
    char mdate[17];
    char edate[17];
    char efdate[17];
    int8_t fileversion;
    int8_t unused0;
    char null2[512];
    char reserved[653];
} __attribute__ ((packed)) iso9660_pvd_t;




typedef struct iso9660 {
    inode_t* dev;
    iso9660_pvd_t pvd;
    iso9660_dir_t dir;
} __attribute__ ((packed)) iso9660_t;



int iso9660_mount(struct inode* dev, struct inode* dir);
int iso9660_read(inode_t* ino, void* buf, off_t pos, size_t size);
int iso9660_open(struct inode* inode);
int iso9660_close(struct inode* inode);
struct inode* iso9660_finddir(struct inode* inode, char* name);
int iso9660_unlink(struct inode* inode, char* path);


void iso9660_checkname(char* name);
uint32_t iso9660_getlsb32(uint64_t val);
uint16_t iso9660_getlsb16(uint32_t val);
uint32_t iso9660_getmsb32(uint64_t val);
uint16_t iso9660_getmsb16(uint32_t val);

#if HAVE_ROCKRIDGE
char* rockridge_getname(void* offset);
void rockridge_getmode(void* offset, mode_t* mode, uid_t* uid, gid_t* gid, nlink_t* nlink);
#endif

#endif
