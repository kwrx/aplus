#ifndef _I825XX_H
#define _I825XX_H

#define NUM_RX_DESC			768
#define NUM_TX_DESC			768


#define CTRL_FD				(1 << 0)
#define CTRL_ASDE			(1 << 5)
#define CTRL_SLU			(1 << 6)

#include <aplus.h>
#include <stdint.h>


typedef struct {
	volatile uint64_t address;
	volatile uint16_t length;
	volatile uint16_t checksum;
	volatile uint8_t status;
	volatile uint8_t errors;
	volatile uint16_t special;
} __packed i825xx_rx_desc_t;


typedef struct {
	volatile uint64_t address;
	volatile uint16_t length;
	volatile uint8_t cs0;
	volatile uint8_t cmd;
	volatile uint8_t sta;
	volatile uint8_t css;
	volatile uint16_t special;
} __packed i825xx_tx_desc_t;


typedef struct i825xx_device {
	uintptr_t mmio_address;
	uint32_t io_address;

	volatile uint8_t* rx_desc_base;
	volatile i825xx_rx_desc_t* rx_desc[NUM_RX_DESC];
	volatile uint16_t rx_tail;

	volatile uint8_t* tx_desc_base;
	volatile i825xx_tx_desc_t* tx_desc[NUM_TX_DESC];
	volatile uint16_t tx_tail;

	uint16_t (*eeprom_read) (struct i825xx_device*, uint8_t);
} i825xx_device_t;


#define MDIC_PHYADD			(1 << 21)
#define MDIC_OP_WRITE		(1 << 26)
#define MDIC_OP_READ		(2 << 26)
#define MDIC_R				(1 << 28)
#define MDIC_I				(1 << 29)
#define MDIC_E				(1 << 30)

#define RCTL_EN				(1 << 1)
#define RCTL_SBP			(1 << 2)
#define RCTL_UPE			(1 << 3)
#define RCTL_MPE			(1 << 4)
#define RCTL_LPE			(1 << 5)
#define RDMTS_HALF			(0 << 8)
#define RDMTS_QUARTER		(1 << 8)
#define RDMTS_EIGHTH		(2 << 8)
#define RCTL_BAM			(1 << 15)
#define RCTL_BSIZE_256		(3 << 16)
#define RCTL_BSIZE_512		(2 << 16)
#define RCTL_BSIZE_1024		(1 << 16)
#define RCTL_BSIZE_2048		(0 << 16)
#define RCTL_BSIZE_4096		((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192		((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384	((1 << 16) | (1 << 25))
#define RCTL_SECRC			(1 << 26)

#define TCTL_EN				(1 << 1)
#define TCTL_PSP			(1 << 3)

#endif
