#ifndef _ATAPI_H
#define _ATAPI_H

#include <stdint.h>

#define ATAPI_SECTOR_SIZE		2048

#define ATA_IRQ_PRIMARY			0x0E
#define ATA_IRQ_SECONDARY		0x0F

#define ATA_DATA(X)				(X)
#define ATA_FEATURES(X)			(X + 1)
#define ATA_SECTOR_COUNT(X)		(X + 2)
#define ATA_ADDR1(X)			(X + 3)
#define ATA_ADDR2(X)			(X + 4)
#define ATA_ADDR3(X)			(X + 5)
#define ATA_DRIVE_SELECT(X)		(X + 6)
#define ATA_COMMAND(X)			(X + 7)
#define ATA_DCR(X)				(X + 0x206)


#define ATAPI_CMD_TEST			0x00
#define ATAPI_CMD_EJECT			0x1B
#define ATAPI_CMD_READCAPS		0x25
#define ATAPI_CMD_READ			0xA8


#define ATA_BUS_PRIMARY			0x1F0
#define ATA_BUS_SECONDARY		0x170

#define ATA_DRIVE_MASTER		0xA0
#define ATA_DRIVE_SLAVE			0xB0

#define ATA_SELECT_DELAY(bus)	\
	{ 							\
		inb(ATA_DCR(bus));		\
		inb(ATA_DCR(bus));		\
		inb(ATA_DCR(bus));		\
		inb(ATA_DCR(bus));		\
	}
	



#endif
