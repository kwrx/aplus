#ifndef _ISO9660_H
#define _ISO9660_H

#include <stdint.h>


#define ISO9660_SECTOR_SIZE						2048

#define ISO9660_PVD								(0x10 * ISO9660_SECTOR_SIZE)
#define ISO9660_ID								"CD001"
#define ISO9660_VERSION							0x01

#define ISO9660_VOLDESC_BOOTRECORD				0
#define ISO9660_VOLDESC_PRIMARY					1
#define ISO9660_VOLDESC_SUPPLEMENTARY			2
#define ISO9660_VOLDESC_PARTITION				3
#define ISO9660_VOLDESC_TERMINATOR				255

#define ISO9660_VOLDESC_SIZE					ISO9660_SECTOR_SIZE


#define ISO9660_FLAGS_HIDDEN					(1 << 0)
#define ISO9660_FLAGS_DIRECTORY					(1 << 1)
#define ISO9660_FLAGS_FILE						(1 << 2)
#define ISO9660_FLAGS_EXTATTR					(1 << 3)
#define ISO9660_FLAGS_EXTFLAGS					(1 << 4)
#define ISO9660_FLAGS_NOTLAST					(1 << 7)

#define ISO9660_NAME_LENGTH						(1024)

typedef struct iso9660_volume_descriptor {
	int8_t type;
	char id[5];
	int8_t version;
	char data[2041];
} __attribute__ ((packed)) iso9660_volume_descriptor_t;

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
	
	char rootdir[34];
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

#endif
