#ifndef _FAT_H
#define _FAT_H


#define FAT_DIR_ATTR_READONLY           0x01
#define FAT_DIR_ATTR_HIDDEN             0x02
#define FAT_DIR_ATTR_SYSTEM             0x04
#define FAT_DIR_ATTR_VOLUME_ID          0x08
#define FAT_DIR_ATTR_DIRECTORY          0x10
#define FAT_DIR_ATTR_ARCHIVE            0x20

#define FAT_DIR_ATTR_LFN            \
    (FAT_DIR_ATTR_READONLY      |   \
     FAT_DIR_ATTR_HIDDEN        |   \
     FAT_DIR_ATTR_SYSTEM        |   \
     FAT_DIR_ATTR_VOLUME_ID)





typedef struct {
    uint8_t sb_jmp[3];
    uint8_t sb_oem[8];
    uint16_t sb_bytes_per_sector;
    uint8_t sb_sectors_per_cluster;
    uint16_t sb_reserved_sectors;
    uint8_t sb_number_of_FAT;
    uint16_t sb_number_of_dir_entries;
    uint16_t sb_total_sectors;
    uint8_t sb_media_type;
    uint16_t sb_number_of_sectors_per_FAT;
    uint16_t sb_number_of_sectors_per_track;
    uint16_t sb_number_of_heads;
    uint32_t sb_number_of_hidden_sectors;
    uint32_t sb_total_sectors_large;

    union {
        struct {
            uint8_t esb16_drive_number;
            uint8_t esb16_flags;
            uint8_t esb16_signature;
            uint32_t esb16_volume_id;
            uint8_t esb16_volume_label[11];
            uint8_t esb16_sysid[8];
            uint8_t esb16_boot_code[448];
            uint16_t esb16_boot_magic;
        } sb_esb16;

        struct {
            uint32_t esb32_sectors_per_FAT;
            uint16_t esb32_flags;
            uint16_t esb32_version;
            uint32_t esb32_cluster_of_root;
            uint16_t esb32_sector_of_FSInfo;
            uint16_t esb32_sector_of_BBS;
            uint8_t esb32_reserved[12];
            uint8_t esb32_drive_number;
            uint8_t esb32_winflags;
            uint8_t esb32_signature;
            uint32_t esb32_volume_id;
            uint8_t esb32_volume_label[11];
            uint8_t esb32_sysid[8];
            uint8_t esb32_boot_code[420];
            uint16_t esb32_boot_magic;
        } sb_esb32;
    };
} fat_superblock_t;


typedef struct {
    union {
        struct {
            uint8_t d_name[8];
            uint8_t d_ext[3];
            uint8_t d_flags;
            uint8_t d_reserved;
            
            uint8_t d_ctime_ds;
            uint16_t d_ctime;
            uint16_t d_cdate;
            
            uint16_t d_adate;
            uint16_t d_cluster_high;
            uint16_t d_mtime;
            uint16_t d_mdate;
            uint16_t d_cluster_low;
            uint32_t d_size;
        } d_node;

        struct {
            uint8_t lfn_index;
            uint16_t lfn_name_0[5];
            uint8_t lfn_flags;
            uint8_t lfn_type;
            uint8_t lfn_cksum;
            uint16_t lfn_name_1[6];
            uint16_t lfn_zero;
            uint16_t lfn_name_2[2];
        } d_lfn;
    };

} fat_dirent_t;


#endif