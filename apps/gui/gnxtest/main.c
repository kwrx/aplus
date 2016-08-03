#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <aplus/gnx.h>


int main(int argc, char** argv) {
    printf("gnxtest: running...\n");
    
   
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        perror("socket");
        return -1;
    }
    

    struct sockaddr_in sock;
    sock.sin_len = sizeof(sock);
    sock.sin_family = AF_INET;
    sock.sin_port = __builtin_bswap16(GNX_PORT);
    sock.sin_addr.s_addr = __builtin_bswap32(0x7f000001UL);
    
    
    printf("gnxtest: connecting...\n");
    int e = connect(fd, (struct sockaddr*) &sock, sizeof(sock));
    if(e < 0) {
        perror("connect");
        return -1;
    }
    
    printf("gnxtest: connected!");
    
    for(;;);
}