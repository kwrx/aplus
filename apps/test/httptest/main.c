#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#include <aplus/base.h>
#include <aplus/network/http_parser.h>


int url_callback(http_parser* p, const char* at, size_t len) {
    char buf[len];
    strncpy(buf, at, len);

    fprintf(stderr, "url: %s\n", buf);
    return 0;
}

int status_callback(http_parser* p, const char* at, size_t len) {
    char buf[len];
    strncpy(buf, at, len);

    fprintf(stderr, "status: %s\n", buf);
    return 0;
}


int main(int argc, char** argv) {

    struct hostent* e = gethostbyname("www.geekstribe.altervista.org");
    if(!e) {
        fprintf(stderr, "gethostbyname() failed\n");
        return -1;
    }
    
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        perror("socket");
        return -1;
    }


    struct sockaddr_in in;
    in.sin_family = AF_INET;
    in.sin_port = __builtin_bswap16(80);
    memcpy(&in.sin_addr.s_addr, e->h_addr_list[0], e->h_length);
    

    if(connect(fd, (struct sockaddr*) &in, sizeof(in)) != 0) {
        perror("connect");
        return -1;
    }


    #define W(x)    \
        write(fd, x, strlen(x))
    

    W("GET /index.php HTTP/1.1\r\n");
    W("Host: www.geekstribe.altervista.org\r\n");
    W("\r\n");




    http_parser_settings s;
    http_parser_settings_init(&s);

    s.on_url = url_callback;
    s.on_status = status_callback;


    http_parser p;
    http_parser_init(&p, HTTP_REQUEST);
    
    
    return 0;
}