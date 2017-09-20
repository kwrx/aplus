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


void print_status(size_t bytes_read, size_t size, time_t tm) {
    static time_t __time = 0;
    static size_t __lastbytes = 0;
    static size_t __speed = 0;

    if(!size) {
        __time =
        __lastbytes =
        __speed = 0;
        return;
    }

    fprintf(stderr, "\r [");
    
    int i;
    for(i = 0; i < 20; i++)
        if(i < (int) (((double) bytes_read / (double) size) * 100.0 / 5.0))
            fprintf(stderr, "*");
        else
            fprintf(stderr, " ");

    fprintf(stderr, "] %.2f/%.2f kB (%.2f kB/s)", (double) bytes_read / 1024.0, (double) size / 1024.0, (double) __speed / 1024.0);

    if(tm != __time) {
        __time = tm;
        __speed = bytes_read - __lastbytes;
        __lastbytes = bytes_read;
    }
}

void http_request(int fd, char* host, char* resource) {
    char buf[BUFSIZ];
    memset(buf, 0, sizeof(buf));

    sprintf(buf,
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: keep-alive\r\n"
        "Accept-Encoding: identity, *;q=0\r\n"
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

    int n;
    if((n = read(fd, buf, sizeof(buf))) < 0) {
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

    char* p = *data = (char*) malloc(*size);
    if(!p) {
        fprintf(stderr, "http_request->malloc(): no memory left");
        return;
    }

    char* d = &strstr(buf, "\r\n\r\n")[4];
    int c = n - ((uintptr_t) d - (uintptr_t) buf);
    memcpy(p, buf, c);
    p = &p[c];

    print_status(0, 0, 0);

    int i;
    for(i = 0; i < *size; i += BUFSIZ) {
        n = read(fd, p, BUFSIZ);
        c += n;

        if(n < 0)
            break;

        if(n == 0)
            continue;

        p = &p[n];
        print_status(c, *size, time(NULL));
    }

    print_status(c, *size, time(NULL));
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
    

    fprintf(stderr, "GET http://%s%s\n", url, resource);

    http_request(fd, url, resource);
    http_response(fd, &data, &size);

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