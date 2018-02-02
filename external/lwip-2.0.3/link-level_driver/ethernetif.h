#ifndef __ETHERNETIF_H
#define __ETHERNETIF_H

#define ETHERNET_MTU		1500

struct ethernetif {
  void (* low_level_init)(void *i, uint8_t *addr, void *mcast); ///< the hardware init function
  int (* low_level_startoutput)(void *i); ///< check room
  void (* low_level_output)(void *i, void *data, uint16_t len); ///< write blocks
  void (* low_level_endoutput)(void *i, uint16_t total_len);  ///< end writing, send
  int (* low_level_startinput)(void *i);  ///< check existence, get length
  void (* low_level_input)(void *i, void *data, uint16_t len); ///< read blocks
  void (* low_level_endinput)(void *i);   ///< end reading
  void (* low_level_input_nomem)(void *i, uint16_t len); ///< drop/queue
  void *internals; ///< trivial internal stuff, like for example I/O address, passed to low level functions
  uint8_t address[6]; ///< "the MAC"
};

/*
  uint8_t flags;

/// Flags
//@{
#define ETHERNETIF_FLAG_RXPENDING   (1<<0)
#define ETHERNETIF_FLAG_LSCHANGE    (1<<1)
#define ETHERNETIF_FLAG_LINKUP      (1<<2)
#define ETHERNETIF_FLAG_100MBPS     (1<<3)
#define ETHERNETIF_FLAG_FULLDUPLEX  (1<<4)
//@}
*/

#endif
