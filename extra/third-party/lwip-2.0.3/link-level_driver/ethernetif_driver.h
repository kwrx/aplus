
/** 
 * @file ethernetif.h
 
#define ETHERNETIF_MAXFRAMES >0 if you want to set a limit on the amount of frames
that can be processed in the input loop. On a very-low-memory system, you might be
concerned that a host flooding your host may starve the output from buffers, due to
all of them being allocated to input, or you just don't like having a do {} while(1);


This is how it works:
 INITIALIZATION
  The user calls netif_add(), passing:
    ethetnetif_init() as the init function
    ethernet_input() as the input function (no RTOS, see the docs)
    the struct ethernetif for this interface, containing:
      the wardware address
      pointers to the low level functions
  This function then initializes the structures and calls ethetnetif_init(), which calls our
  low_level_init() function.
 OUTPUT
  When the stack needs to send a frame, it will call netif->output, which points
  to ethernetif_output() (we did that in our init function). This will call:
      low_level_startoutput(), which will check for room in the chip
      low_level_output() for every chunk in the packet buffer (pbuf)
      low_level_endoutput() to actually send the frame.
 INPUT
  When the Ethernet chip signals there is a frame, its interrupt handler or polling routine
  will call back. We then have the choice of starting or scheduling the loop to retrieve the data.
  As lwIP is single-threaded, it is not a good choice to ask for memory on an isr, so the 
  safest option seems to be to raise a flag and start the process later.
  One we are ready, we call ethernetif_input(). This function will call low_level_startinput()
  to actually check if there is something retrievable (it takes some time and polling to actually
  get that from some chips) and get the frame length. It will then allocate a buffer and
  then call:
      low_level_input() for every chunk in the packet buffer to actually get the frame from the chip
      low_level_endinput() to finish the process
  In case it can't allocate a buffer, ethernetif_input() will call low_level_inputnomem(), to drop
  the frame or hold it waiting in chip's memory (developer's choice)
  Finally, whith the frame in the buffer, it will call netif->input.
 */

void ethernetif_input(struct netif *netif);
err_t ethernetif_init(struct netif *netif);
