#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <aplus/base.h>
#include <aplus/gnx.h>
#include <aplus/kmem.h>



int main(int argc, char** argv) {
    int fd = open("/tmp/gnx.lock", O_RDONLY);


    pid_t gnx;
    read(fd, &gnx, sizeof(pid_t));
    close(fd);


    struct {
            uint16_t type;
            void* id;
            uint32_t flags;
            uint16_t width;
            uint16_t height;
            uint16_t x;
            uint16_t y;
            void* pixels;
        } __attribute__((packed)) context;

    context.type = GNXPROT_TYPE_TEST;
    msg_send(gnx, &context, 2);

    context.type = GNXPROT_TYPE_UPDATE_CONTEXT;
    context.id = NULL;
    context.flags = GNX_FLAGS_SHOW;
    context.width = 400;
    context.height = 300;
    context.x = 100;
    context.y = 100;
    context.pixels = kmem_alloc(400 * 300 * 4);

    memset(context.pixels, 0xFF, 400 * 300 * 4);



    msg_send(gnx, &context, sizeof(context));

    context.type = GNXPROT_TYPE_BLIT_CONTEXT;
    msg_send(gnx, &context, sizeof(context));

    context.type = GNXPROT_TYPE_TEST;
    msg_send(gnx, &context, 2);

    
    return 0;
}