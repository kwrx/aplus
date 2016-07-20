#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/ipc.h>
#include <xdev/mm.h>
#include <xdev/timer.h>
#include <libc.h>


#include "lwip/opt.h"

#include "lwip/init.h"
#include "lwip/stats.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/sockets.h"
#include "lwip/ip.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/tcp_impl.h"
#include "lwip/snmp_msg.h"
#include "lwip/autoip.h"
#include "lwip/igmp.h"
#include "lwip/dns.h"
#include "lwip/timers.h"
#include "lwip/tcpip.h"
#include "netif/etharp.h"

#if CONFIG_NETWORK

static void tcpip_init_done(void* arg) {
#if DEBUG
	kprintf(LOG, "[%d] tcpip: initialized in %d MS\n", sys_getpid(), timer_getms() - (uint32_t) arg);
#else
	(void) arg;
#endif


	struct netif* lo = netif_find("lo0");
	if(likely(lo)) {
		netif_set_up(lo);
		netif_set_default(lo);
	} else
		kprintf(WARN, "netif: Loopback interface not found\n");


	lwip_socket_init();
}


int network_init() {
#if DEBUG
	tcpip_init(tcpip_init_done, (void*) timer_getms());
#else
	tcpip_init(tcpip_init_done, NULL);
#endif

	return 0;
}

#endif
