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

#define CMD_USER            1
#define CMD_PASS            2
#define CMD_MODE            3
#define CMD_TYPE            4
#define CMD_PASV            5
#define CMD_RETR            6

void print_status(size_t bytes_read, size_t size, time_t tm) {
    static time_t __time = 0;
    static size_t __lastbytes = 0;
    static size_t __speed = 0;

    if(!size) {
        __time = tm;
        __lastbytes =
        __speed = 0;
        return;
    }

    fprintf(stdout, "\r [");
    
    int i;
    for(i = 0; i < 20; i++)
        if(i < (int) (((double) bytes_read / (double) size) * 100.0 / 5.0))
            fprintf(stdout, "*");
        else
            fprintf(stdout, " ");

    fprintf(stdout, "] %.2f/%.2f kB (%.2f kB/s)", (double) bytes_read / 1024.0, (double) size / 1024.0, (double) __speed / 1024.0);

    if(tm != __time) {
        __time = tm;
        __speed = bytes_read - __lastbytes;
        __lastbytes = bytes_read;
    }

    fflush(stdout);
}



int init_ftp(char* url, uint16_t p) {
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
    in.sin_port = __builtin_bswap16(p);
    memcpy(&in.sin_addr.s_addr, e->h_addr_list[0], e->h_length);
    
    if(connect(fd, (struct sockaddr*) &in, sizeof(in)) != 0) {
        perror("connect");
        return -1;
    }

    return fd;
}



void SEND(int fd, char* cmd) {
    fprintf(stderr, "\033[32m - %s\n", cmd);
    send(fd, cmd, strlen(cmd), 0);
}

void RECV(int fd, int cmd, void* arg) {
    char buf[BUFSIZ];
    memset(buf, 0, sizeof(buf));

    int n;
    if((n = recv(fd, buf, sizeof(buf), 0)) < 0)
        fprintf(stderr, "\033[33m - I/O Error!\n");

            
    fprintf(stderr, "\033[33m%s\n", buf);
    switch(cmd) {
        case CMD_PASV: {
            uint8_t d[6];
            int i = 0;

            for(char* p = strtok(&strstr(buf, "(")[1], ","); p; p = strtok(NULL, ",")) {
                d[i++] = atoi(p) & 0xFF;
            }

            if(arg)
                *(uint16_t*) arg = (d[4] << 8) | d[5] & 0xFF;
        } break;

        default:
            break;
    }
}


int main(int argc, char** argv) {
    stderr = fopen("/dev/log", "w");


    int sfd = init_ftp("ftp.geekstribe.altervista.org", 21);
    uint16_t dt_port = 0;

    RECV(sfd, -1, NULL);
    SEND(sfd, "USER geekstribe\r\n");
    RECV(sfd, CMD_USER, NULL);
    SEND(sfd, "PASS custagiddo42\r\n");
    RECV(sfd, CMD_PASS, NULL);
    SEND(sfd, "MODE S\r\n");
    RECV(sfd, CMD_MODE, NULL);
    SEND(sfd, "TYPE A\r\n");
    RECV(sfd, CMD_TYPE, NULL);
    SEND(sfd, "PASV\r\n");
    RECV(sfd, CMD_PASV, &dt_port);    
    SEND(sfd, "RETR El_procedimiento_de_tratamiento.docx\r\n");
    
    fprintf(stderr, "\033[39m - Opening Data Channel on port %d\n", dt_port);
    
    int sock = init_ftp("ftp.geekstribe.altervista.org", dt_port);
    if(sock < 0)
        return -1;

    fprintf(stderr, "\033[39m - Data channel opened\n");
    RECV(sfd, -1, NULL);


    fprintf(stderr, "\033[39m - Begin Transmission\n\n");

    char buf[BUFSIZ];
    print_status(0, 0, 0);

    int n, c = 0;
    while((n = read(sock, buf, sizeof(buf))) > 0) {
        c += n;
        print_status(c, 3425730, time(NULL));
    }

    print_status(c, 3425730, time(NULL));
    fprintf(stdout, "\n"); 
    //fprintf(stdout, "%s\n", buf); 

    fprintf(stderr, "\n\n\033[39m - End Transmission\n\n");
    RECV(sfd, -1, NULL);

    SEND(sfd, "QUIT\r\n");
    RECV(sfd, -1, NULL);

    close(sfd);
    close(sock);

    fflush(stderr);
    fflush(stdout);
    return 0;
}