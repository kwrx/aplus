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

void http_request(int fd, char* host, char* resource) {
    char buf[BUFSIZ];
    memset(buf, 0, sizeof(buf));

    sprintf(buf,
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: keep-alive\r\n"
        "\r\n\0",
        resource, host);


    if(write(fd, buf, strlen(buf)) <= 0) {
        perror("http_request->send()");
        return;
    }
}


void http_response(int fd, char** data, size_t* size) {
    if(!data || !size) {
        errno = EINVAL;
        perror("http_response()");
        return;
    }


    char buf[BUFSIZ];
    memset(buf, 0, sizeof(buf));

    if(read(fd, buf, sizeof(buf)) <= 0) {
        fprintf(stderr, "http_response->read(): I/O error");
        return;
    }

    if(strncmp(buf, "HTTP/1.1", 8) != 0) {
        fprintf(stderr, "Invalid HTTP Response\n%s", buf);
        return;
    }

    int status = atoi(&buf[9]);
    if(status != 200) {
        fprintf(stderr, "HTTP: Bad status %d", status);
        return;
    }

    if(strstr(buf, "Content-Length: "))
        *size = atoi(&strstr(buf, "Content-Length: ")[16]);

    char* p = (char*) malloc(*size);
    if(!p) {
        fprintf(stderr, "http_request->malloc(): no memory left");
        return;
    }

    *data = p;


    int i;
    for(i = 0; i < *size; i += BUFSIZ) {
        if(read(fd, p, BUFSIZ) < BUFSIZ)
            return;

        p = &p[BUFSIZ];
    }
}


int fetch(char* url, char* resource) {
    struct hostent* e = gethostbyname(url);
    if(!e) {
        perror(url);
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
    


    char* data = NULL;
    size_t size = 0;
    

    fprintf(stderr, "GET http://%s%s -> ", url, resource);

    http_request(fd, url, resource);
    http_response(fd, &data, &size);

    if(size)
        fprintf(stderr, "%d bytes", size);
    fprintf(stderr, "\n");


    if(data)
        free(data);

    close(fd);
    return size;   
}


int main(int argc, char** argv) {
    //fetch("www.osdev.org", "/");
    //fetch("www.geekstribe.altervista.org", "/index.php");
    //fetch("www.cplusplus.com", "/");
    fetch("ipv4.download.thinkbroadband.com", "/5MB.zip");
    return 0;
}