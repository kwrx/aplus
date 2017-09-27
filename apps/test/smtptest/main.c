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
#include <assert.h>




int main(int argc, char** argv) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);


    struct sockaddr_in in;
    in.sin_family = AF_INET;
    in.sin_port = htons(5000);
    in.sin_addr.s_addr = htonl(167772674);

    int h = connect(fd, (struct sockaddr*) &in, sizeof(in));
    assert(h == 0);


    FILE* fp = fopen("/dev/log", "w");
    if(fp)
        stderr = fp;

    char buf[BUFSIZ];

    #define WR(x)                                   \
        fprintf(stderr, "\e[33m" x "\e[39m\n");     \
        write(fd, x "\r\n", strlen(x "\r\n"))

    #define RD()                                    \
        memset(buf, 0, sizeof(buf));                \
        read(fd, buf, sizeof(buf));                 \
        fprintf(stderr, "\e[34m%s\e[39m", buf)
    


    RD();
    WR("EHLO smtp.org");
    RD();
    WR("AUTH LOGIN");
    RD();
    WR("aW5mZXJkZXZpbDk3QGdtYWlsLmNvbQ==");
    RD();
    WR("cGxheXN0YXRpb24yMg==");
    RD();
    WR("MAIL FROM:<inferdevil97@gmail.com>");
    RD();
    WR("RCPT TO:<inferdevil97@gmail.com>");
    RD();
    WR("DATA");
    RD();
    WR("Date: Sun, 24 Sep 2017 22:22:00");
    WR("From: AA<inferdevil97@gmail.com>");
    WR("X-Priority: 3");
    WR("To:<inferdevil97@gmail.com>");
    WR("Subject: The Title");
    WR("MIME Version 1.0");
    WR("Content-Type: text/plain");
    WR("\r\nQuesto e un messaggio di esempio!\r\n");
    WR(".");
    RD();
    WR("QUIT");
    RD();


    close(fd);
    return 0;
}