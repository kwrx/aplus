#ifndef _FAT_H
#define _FAT_H

/* Long file name's support */
#define CONFIG_LFN        1
#define DEBUG            1


typedef struct {
    char jmp[3];
    char oemid[8];
    uint16_t bytes_per_sector;
    uint8_t sector_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t rootdir_entries;
    uint16_t total_sectors;
    uint8_t media_type;
    uint16_t number_of_sector_per_fat;
    uint16_t number_of_sector_per_track;
    uint16_t number_of_heads;
    uint32_t number_of_hidden_sectors;
    uint32_t total_sectors_large;

    union {
        struct {
            uint8_t drive_number;
            uint8_t flags;
            uint8_t signature;
            uint32_t volume_id;
            char volume_label[11];
            char system_label[8];
            char boot_code[448];
            uint16_t bootable_magic;
        } fat12_16;

        struct {
            uint32_t sectors_per_fat;
            uint16_t flags;
            uint16_t fat_version;
            uint32_t cluster_of_root;
            uint16_t number_of_sector_per_FSinfo;
            uint16_t number_of_sector_per_backup;
            char always0[12];
            uint8_t drive_number;
            uint8_t flags2;
            uint32_t volume_id;
            char volume_label[11];
            char system_label[8];
            char boot_code[420];
            uint16_t bootable_magic;
        } fat32;
    };
} __packed fat_mbr_t;


typedef struct {
    char name[8];
    char extension[3];
    uint8_t flags;
    uint8_t reserved;
    uint8_t ctime_dec;

    struct {
        uint16_t hour:5;
        uint16_t minutes:6;
        uint16_t seconds:5;
    } __packed ctime;
    
    struct {
        uint16_t year:7;
        uint16_t month:4;
        uint16_t day:5;
    } __packed cdate;

    struct {
        uint16_t hour:5;
        uint16_t minutes:6;
        uint16_t seconds:5;
    } __packed atime;

    uint16_t cluster_high;

    struct {
        uint16_t hour:5;
        uint16_t minutes:6;
        uint16_t seconds:5;
    } __packed mtime;

    struct {
        uint16_t year:7;
        uint16_t month:4;
        uint16_t day:5;
    } __packed mdate;

    uint16_t cluster_low;
    uint32_t size;
} __packed fat_entry_t;

typedef struct {
    uint8_t order;
    uint16_t name_0[5];
    uint8_t flags;
    uint8_t let;
    uint8_t cksum;
    uint16_t name_1[6];
    uint16_t null;
    uint16_t name_2[2];
} __packed fat_entry_lfn_t;


#define FAT_BAD_CLUSTER        (-2)
#define FAT_END_CLUSTER        (-1)
#define FAT_UNUSED_CLUSTER     (0)

#define FAT32            1
#define FAT16            2
#define FAT12            3

#define ATTR_RDONLY        1
#define ATTR_HIDDEN        2
#define ATTR_SYSTEM        4
#define ATTR_VOLID        8
#define ATTR_DIRECTORY    16
#define ATTR_ARCHIVE    32
#define ATTR_LFN        15

#define FAT_MAXFN        1024

#define CLUSTER_TO_SECTOR(x, y)        \
    ((((int) (y) - 2) * x->sector_per_cluster) + x->first_data_sector)

#define SECTOR_TO_CLUSTER(x, y)        \
    ((((int) (y) - x->first_data_sector) / x->sector_per_cluster) + 2)

typedef struct {
    uint32_t sector_per_cluster;
    uint32_t total_sectors;
    uint32_t fat_size;
    uint32_t rootdir_sectors;
    uint32_t first_data_sector;
    uint32_t first_fat_sector;
    uint32_t data_sectors;
    uint32_t total_clusters;
    uint32_t bytes_per_sector;
    uint32_t entry_sector;
    uint32_t entry_cluster;
    uint32_t entry_offset;
    uint8_t type;
    uint8_t* FAT;

    struct inode* dev;
} __packed fat_t;



#define fail_if(x, y)                                                               \
    if(unlikely(x)) {                                                               \
        kprintf(ERROR "[FAT]f %s(%d) failed on " #x "\n", __func__, __LINE__);     \
        errno = y;                                                                  \
        return E_ERR;                                                               \
    }

#define return_if(x, y)                                                               \
    if(unlikely(x)) {                                                               \
        kprintf(ERROR "[FAT]r %s(%d) failed on " #x "\n", __func__, __LINE__);     \
        errno = y;                                                                  \
        return;                                                                           \
    }

#define null_if(x, y)                                                               \
    if(unlikely(x)) {                                                               \
        kprintf(ERROR "[FAT]n %s(%d) failed on " #x "\n", __func__, __LINE__);     \
        errno = y;                                                                  \
        return NULL;                                                                   \
    }

#define zero_if(x, y)                                                               \
    if(unlikely(x)) {                                                               \
        kprintf(ERROR "[FAT]z %s(%d) failed on " #x "\n", __func__, __LINE__);     \
        errno = y;                                                                  \
        return 0;                                                                       \
    }


#define FAT_EXTRACT_DONE        0
#define FAT_EXTRACT_END            1
#define FAT_EXTRACT_CONTINUE    2


void fatcat(char* name, char* fatnm, char* fatex);
void lfncat(char* name, uint16_t* lfn, size_t size);

char* fatutils_extract_name(char* bufname, int* r, fat_entry_t* e);
void fatutils_new_child(fat_t* fat, fat_entry_t* e, uint32_t entry_offset, inode_t** childptr, inode_t* parent);
int fatutils_next_cluster(fat_t* fat, int current_cluster);
int fatutils_alloc_cluster(fat_t* fat, int active_cluster, int* ncluster);
int fatutils_free_cluster(fat_t* fat, int active_cluster);
int fatutils_update_entry(fat_t* fat, struct inode* inode, int);
int fatutils_update_fat(fat_t* fat);


int fat_mount(struct inode* dev, struct inode* dir);
int fat12_16_root_open(struct inode* inode);
inode_t* fat12_16_root_finddir(struct inode* inode, char* name);
inode_t* fat12_16_root_mknod(struct inode* inode, char* name, mode_t mode);

int fat_open(struct inode* inode);
int fat_close(struct inode* inode);
inode_t* fat_finddir(struct inode* inode, char* name);
int fat_read(inode_t* ino, void* buf, size_t size);
int fat_write(inode_t* ino, void* buf, size_t size);
int fat_unlink(struct inode* inode, char* name);
inode_t* fat_mknod(struct inode* inode, char* name, mode_t mode);

#endif
