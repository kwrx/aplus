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

#include <axtls/ssl.h>
#include <axtls/cert.h>
#include <axtls/private_key.h>


#define URL         "smtp.gmail.com"
#define PORT        587

int main(int argc, char** argv) {
    struct hostent* e = gethostbyname(URL);
    assert(e);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);


    struct sockaddr_in in;
    in.sin_family = AF_INET;
    in.sin_port = htons(PORT);
    memcpy(&in.sin_addr.s_addr, e->h_addr_list[0], e->h_length);

    int h = connect(fd, (struct sockaddr*) &in, sizeof(in));
    assert(h == 0);



    char buf[BUFSIZ];
    uint8_t* data = NULL;

    #define WR(x)                                   \
        fprintf(stderr, "\e[33m" x "\e[39m\n");     \
        write(fd, x "\r\n", strlen(x "\r\n"))

    #define RD()                                    \
        memset(buf, 0, sizeof(buf));                \
        read(fd, buf, sizeof(buf));                 \
        fprintf(stderr, "\e[34m%s\e[39m", buf)
    


    RD();
    WR("HELO smtp.org");
    RD();
    WR("STARTTLS");
    RD();

    SSL_CTX* ctx = ssl_ctx_new(0, 0);
    assert(ctx);

    ssl_obj_memory_load(ctx, SSL_OBJ_PKCS12, default_private_key, default_private_key_len, NULL);
    ssl_obj_memory_load(ctx, SSL_OBJ_X509_CERT, default_certificate, default_certificate_len, NULL);
    
    
    static char sessionid[32];
    SSL* ssl = ssl_client_new(ctx, fd, sessionid, sizeof(sessionid), NULL);
    assert(ssl);

    int r;
    if((r = ssl_handshake_status(ssl)) != SSL_OK)
        { ssl_display_error(r); assert(0); }

    #undef WR
    #undef RD

    #define WR(x)                                   \
        fprintf(stderr, "\e[33m" x "\e[39m\n");     \
        ssl_write(ssl, x "\r\n", strlen(x "\r\n"))

    #define RD()                                    \
        if(ssl_read(ssl, &data) < 0) assert(0);     \
        if(data)                                    \
            fprintf(stderr, "\e[34m%s\e[39m", data)


    
    WR("AUTH LOGIN");
    RD();
    WR("aW5mZXJkZXZpbDk3QGdtYWlsLmNvbQ==");
    RD();
    WR("cGxheXN0YXRpb24yMg==");
    RD();
    WR("MAIL FROM:<inferdevil97@gmail.com>");
    RD();
    WR("RCPR TO:<inferdevil97@gmail.com>");
    RD();
    WR("DATA");
    RD();
    WR("Date: Sun, 24 Sep 2017 22:22:00");
    WR("From: AA<inferdevil97@gmail.com>");
    WR("X-Mailer: The Bat! (v3.02) Professional");
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
    ssl_free(ssl);
    ssl_ctx_free(ctx);
    
    return 0;
}