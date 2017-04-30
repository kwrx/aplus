#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 


#define htons(x)  __builtin_bswap16(x)

void error(const char *msg)
{
    perror(msg);
    //exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    portno = 5000;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = 0x01000000 | 127;
    serv_addr.sin_port = htons(portno);

    fprintf(stderr, "tcpclient: connecting on: %d.%d.%d.%d:%d\n", (serv_addr.sin_addr.s_addr & 0x000000FF), (serv_addr.sin_addr.s_addr & 0x0000FF00) >> 8, (serv_addr.sin_addr.s_addr & 0x00FF0000) >> 16, (serv_addr.sin_addr.s_addr & 0xFF000000) >> 24, htons(serv_addr.sin_port));
       
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");
    fprintf(stderr, "CONNESSOOOOOOOOOOOOOOO!!!\n");

    int e = write(sockfd, "HELLOOOO!\0", 10);
    fprintf(stderr, "tcpclient: sent %d bytes\n", e);
    //close(sockfd);
    for(;;) sched_yield();
    return 0;
}
