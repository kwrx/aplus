//
//  atapi.h
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.


#ifndef _ATAPI_H
#define _ATAPI_H

#include <aplus.h>
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