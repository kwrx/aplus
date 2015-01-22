#include <aplus.h>
#include <aplus/list.h>
#include <aplus/netif.h>
#include <aplus/attribute.h>


#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>



netif_socket_t* netif_socket_create(int domain, int type, int proto) {
	if(unlikely(netif_protocols[proto] == NULL)) {
		errno = EINVAL;
		return NULL;	
	}

	netif_socket_t* sock = (netif_socket_t*) kmalloc(sizeof(netif_socket_t));
	if(unlikely(!sock)) {
		errno = ENOMEM;
		return NULL;
	}

	memset(sock, 0, sizeof(netif_socket_t));


	static uint64_t socket_id = 0; 

	sock->id = socket_id++;
	sock->domain = domain;
	sock->type = type;
	sock->proto = proto;


#ifdef SOCKET_DEBUG
	kprintf("socket: create with proto \"%s\" (%d)\n", netif_protocols[proto]->name, proto);
#endif


	sock->check = netif_protocols[proto]->check;
	sock->send = netif_protocols[proto]->send;
	sock->close = netif_protocols[proto]->close;
	sock->netif = (netif_t*) netif_get_interface(0);

	return sock;
}

int netif_socket_bind(netif_socket_t* sock, struct sockaddr* addr, size_t size) {
	if(unlikely(!addr || !size || !sock)) {
		errno = EINVAL;
		return -1;
	}

	memcpy((void*) &sock->sockaddr, (void*) addr, size);
	return 0;
}


int netif_socket_read(netif_socket_t* sock, void* buf, size_t size) {
	if(unlikely(!buf || !size || !sock)) {
		errno = EINVAL;
		return -1;
	}

	if(unlikely(!sock->check)) {
		errno = ENOSYS;	
		return -1;
	}

	list_foreach(value, netif_stack[sock->proto]) {
		netif_packet_t* p = (netif_packet_t*) value;
	
		if(sock->check(sock, p)) {

			if(unlikely(size > p->data.length))
				size = p->data.length;
		
			if(unlikely(p->data.offset > p->data.length))
				p->data.offset = p->data.length;
		
			if(unlikely(p->data.offset + size > p->data.length))
				size = p->data.length - p->data.offset;
		
			if(unlikely(!size))
				return list_remove(netif_stack[p->proto], (listval_t) p);

			memcpy(buf, (void*) ((uint32_t) p->data.ptr + p->data.offset), size);
			p->data.offset += size;

			if(p->data.offset >= p->data.length)		
				list_remove(netif_stack[p->proto], (listval_t) p);
			
			return size;
		}
	}

	return 0;
}

int netif_socket_write(netif_socket_t* sock, void* buf, size_t size) {
	if(size > sock->netif->mtu)
		size = sock->netif->mtu;

	if(likely(sock->send))
		return sock->send(sock, buf, size);

	errno = ENOSYS;
	return -1;
}

int netif_socket_close(netif_socket_t* sock, int status) {
	if(likely(sock->close))
		return sock->close(sock, status);

	errno = ENOSYS;
	return -1;
}
