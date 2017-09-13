#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


#define htons(x)  __builtin_bswap16(x)
#define htonl(x)  __builtin_bswap32(x)


int mkrequest(char* url, char* resource) {
    fprintf(stdout, "\nGET \033[37m%s%s\033[39m\n", url, resource);

    struct hostent* e = gethostbyname(url);
    if(!e) {
        fprintf(stderr, "%s: HOST_NOT_FOUND\n", url);
        return -1;
    }

    fprintf(stdout, "\t => Resolved DNS %d.%d.%d.%d\n",
        e->h_addr_list[0][0] & 0xFF,
        e->h_addr_list[0][1] & 0xFF,
        e->h_addr_list[0][2] & 0xFF,
        e->h_addr_list[0][3] & 0xFF
    );

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in in;
    memset(&in, 0, sizeof(in));

    in.sin_family = AF_INET;
    in.sin_port = htons(80);
    memcpy(&in.sin_addr.s_addr, e->h_addr_list[0], e->h_length);


    if(connect(fd, (const struct sockaddr*) &in, sizeof(in)) < 0) {
        perror("connect");
        return -1;
    }

    char buf[BUFSIZ];
    sprintf(buf, 
        "GET %s HTTP/1.0\r\n"
        "Host: %s\r\n"
        "\r\n"
    , resource, url);

    if(write(fd, buf, strlen(buf)) < 0)
        perror("send");

    fprintf(stdout, "\t => Sent %d Bytes\n", strlen(buf));
    memset(buf, 0, BUFSIZ);

    size_t n = 0;
    for(;;)
        if((n = read(fd, buf, BUFSIZ)) > 0)
            break;
    fprintf(stdout, "\t => Received %d Bytes\n", n);

    
    char* p = buf;
    while(*p != '\r')
        p++;
    *p++ = 0;
    p++;

    fprintf(stdout, "\t => Response %s\n", &buf[9]);
    
    while(*p && strncmp(p, "\r\n\r\n", strlen("\r\n\r\n")) != 0)
        p++;


    fprintf(stdout, "\n%s\n", p);
    close(fd);
    return 0;
}

int main(int argc, char** argv) {
    mkrequest("www.geekstribe.altervista.org", "/index.php");

    return 0;
}