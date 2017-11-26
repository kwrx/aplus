#include "dmx.h"
#include <aplus/msg.h>

void dmx_proto_ack(int type, pid_t pid, void* arg) {
    dmx_packet_ack_t ack;
    ack.type = type;
    ack.arg = arg;

    msg_send(pid, &ack, sizeof(ack));
}

void dmx_proto_disconnect_client(dmx_t* dmx, dmx_packet_connection_t* pk) {
    int i;
    do {
        i = 0;

        list_each(dmx->clients, c) {
            if(c->pid != pk->pid)
                continue;

            i++;
            dmx_gc_free(c);
            list_remove(dmx->clients, c);            
            break;
        }
    } while(i > 0);

    dmx_proto_ack(DMX_PROTO_DISCONNECT, pk->pid, NULL);
}

void dmx_proto_create_gc(dmx_t* dmx, dmx_packet_gc_t* pk) {
    dmx_gc_t* gc = dmx_gc_alloc(dmx, pk->pid, pk->width, pk->height);
    if(!gc)
        fprintf(stderr, "dmx: dmx_gc_alloc() failed: %s\n", strerror(errno));
    else
        list_push(dmx->clients, gc);

    dmx_proto_ack(DMX_PROTO_CREATE_GC, pk->pid, gc);
}

void dmx_proto_destroy_gc(dmx_t* dmx, dmx_packet_gc_t* pk) {
    list_remove(dmx->clients, pk->gc);
    dmx_gc_free(pk->gc);

    dmx_proto_ack(DMX_PROTO_DESTROY_GC, pk->pid, NULL);
}
