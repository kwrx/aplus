#ifndef _RTL8139_H
#define _RTL8139_H

#include <aplus/netif.h>

#define REG_ID0									0x00
#define REG_ID4									0x04

#define REG_TRANSMIT_STATUS0					0x10
#define REG_TRANSMIT_ADDR0						0x20
#define REG_RECEIVE_BUFFER						0x30
#define REG_COMMAND								0x37
#define REG_CUR_READ_ADDR						0x38
#define REG_INTERRUPT_MASK						0x3C
#define REG_INTERRUPT_STATUS					0x3E
#define REG_TRANSMIT_CONFIGURATION				0x40
#define REG_RECEIVE_CONFIGURATION				0x44
#define REG_CONFIG1								0x52

#define CR_RESET								(1 << 4)
#define CR_RECEIVER_ENABLE						(1 << 3)
#define CR_TRANSMITTER_ENABLE					(1 << 2)
#define CR_BUFFER_IS_EMPTY						(1 << 0)

#define TCR_IFG_STANDARD						(3 << 24)
#define TCR_MXDMA_512							(5 << 8)
#define TCR_MXDMA_1024							(6 << 8)
#define TCR_MXDMA_2048							(7 << 8)

#define RCR_MXDMA_512 							(5 << 8)
#define RCR_MXDMA_1024 							(6 << 8)
#define RCR_MXDMA_UNLIMITED 					(7 << 8)
#define RCR_ACCEPT_BROADCAST 					(1 << 3)
#define RCR_ACCEPT_MULTICAST 					(1 << 2)
#define RCR_ACCEPT_PHYS_MATCH 					(1 << 1)

#define ISR_RECEIVE_BUFFER_OVERFLOW 			(1 << 4)
#define ISR_TRANSMIT_OK 						(1 << 2)
#define ISR_RECEIVE_OK 							(1 << 0)

#define RX_BUFFER_SIZE 							8192
#define TX_BUFFER_SIZE							4096


#define int_out8(card, port, value) 			outb(card->device->iobase + port, value)
#define int_out16(card, port, value) 			outw(card->device->iobase + port, value)
#define int_out32(card, port, value) 			outl(card->device->iobase + port, value)

#define int_in8(card, port) 					inb(card->device->iobase + port)
#define int_in16(card, port) 					inw(card->device->iobase + port)

#define RTL8139_MAGIC							0x8139FFFF

#endif
