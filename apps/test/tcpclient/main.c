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
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = 0x01000000 | 127;
    serv_addr.sin_port = htons(portno);

    fprintf(stderr, "tcpclient: connecting on: %d.%d.%d.%d:%d\n", (serv_addr.sin_addr.s_addr & 0xFF000000) >> 24, (serv_addr.sin_addr.s_addr & 0x00FF0000) >> 16, (serv_addr.sin_addr.s_addr & 0x0000FF00) >> 8, (serv_addr.sin_addr.s_addr & 0x000000FF), htons(serv_addr.sin_port));
       
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
    printf("Please enter the message: ");
    bzero(buffer,256);
    fgets(buffer,255,stdin);
    n = write(sockfd,buffer,strlen(buffer));
    if (n < 0) 
         error("ERROR writing to socket");
    bzero(buffer,256);
    n = read(sockfd,buffer,255);
    if (n < 0) 
         error("ERROR reading from socket");
    printf("%s\n",buffer);
    close(sockfd);
    return 0;
}
