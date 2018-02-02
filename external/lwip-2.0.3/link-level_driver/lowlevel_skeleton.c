/**
 * @file
 * low level Ethernet Interface Skeleton
 *
 */

/* portions 
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * See ethernetif.c for copyright details
 */
#include <stdint.h>
#include "ethernetif.h"

/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
void
low_level_init(void *i, uint8_t *addr, void *mcast)
{
	
}

/**
 * This function starts the transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param ethernetif the lwip network interface structure for this netif
 * @return 1 if the packet could be sent
 *         0 if the packet couldn't be sent	(no space on chip)
 */

int
low_level_startoutput(void *i)
{
  return 1;
}

/**
 * This function should do the actual transmission of the packet.
 The packet is contained in chained pbufs, so this function will be called
 for each chunk
 *
 * @param ethernetif the lwip network interface structure for this netif
 * @param data where the data is
 * @param len the block size
 */

void
low_level_output(void *i, void *data, uint16_t len)
{

}
/**
 * This function begins the actual transmission of the packet, ending the process
 *
 * @param ethernetif the lwip network interface structure for this netif
 */

void
low_level_endoutput(void *i, uint16_t total_len)
{
	
}
/**
 * This function checks for a packet on the chip, and returns its length
 * @param ethernetif the lwip network interface structure for this netif
 * @return 0 if no packet, packet length otherwise
 */
int
low_level_startinput(void *i)
{
		return 0;
}

/**
 * This function takes the data from the chip and copies it to a chained pbuf
 * @param ethernetif the lwip network interface structure for this netif
 * @param data where the data is
 * @param len the block size
 */
void
low_level_input(void *i, void *data, uint16_t len)
{
  
}

/**
 * This function ends the receive process
 * @param ethernetif the lwip network interface structure for this netif
 */
void
low_level_endinput(void *i)
{
  
}

/**
 * This function is called in case there is not enough memory to hold a frame
 * after its length has been got from the chip. The driver decides whether to 
 * drop it or let it waiting in the chip's memory, based on the developer's
 * knowledge of the hardware (the chip can have more or less memory than the system)
 * @param ethernetif the lwip network interface structure for this netif
 * @param len the frame length
 */
void
low_level_inputnomem(void *i, uint16_t len)
{
  
}

