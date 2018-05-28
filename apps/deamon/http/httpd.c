#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <aplus/base.h>
#include <aplus/sysconfig.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <pthread.h>
#include <aplus/utils/async.h>
#include <aplus/utils/list.h>


#undef BUFSIZ
#define BUFSIZ                  10240L

#define SERVER_VERSION          "aplus/0.1"
#define HTTP_REQUEST_GET        'G'
#define HTTP_REQUEST_POST       'P'
#define HTTP_REQUEST_HEAD       'H'
#define HTTP_REQUEST_UNKNOWN    '\0'




typedef struct {
    int socket;
    struct sockaddr_in address;
    async_t thread;
} socket_request_t;

typedef struct {
    list(char*, header);
    char* filename;
    char* query;
    char* host;
    char* version;
    char* content_type;
    char* cookie;
    char* user_agent;
    char* referer;

    int request_type;
    int content_length;
} http_request_t;


static char* default_index[] = {
    "index.php",
    "index.html",
    "index.htm",
    NULL
};


static int sock_server = -1;



static void http_response(FILE* fp, char* status, char* message) {
    fprintf (fp,
        "HTTP/1.1 %s\r\n"
        "Server: " SERVER_VERSION "\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s\r\n",

        status, strlen(message), message
    );

    fprintf(stdout,
        "Send %s: %s\n", status, message
    );
}

static void http_free_request(http_request_t* r) {
    list_each(r->header, h)
        free(h);

    list_clear(r->header);
}


static void on_signal(int sig) {
    printf("httpd: [info] Shutting down\n");
    close(sock_server);
    exit(sig);
}

static void on_request(socket_request_t* r) {
    FILE* fp = fdopen(r->socket, "r+");
    if(!fp) {
        perror("httpd: [error] Can not respond to request");
        goto fail;
    }


    do {
        http_request_t proto;
        memset(&proto, 0, sizeof(proto));

        char buf[BUFSIZ];
        while(!feof(fp)) {
            if(!fgets(buf, BUFSIZ - 2, fp))
                break;

            if(strcmp(buf, "\r\n") == 0 || strcmp(buf, "\n"))
                break;

            if(!strstr(buf, "\n")) {
                http_response(fp, "400 Bad Request", "Bad request: Request line was too long.");
                http_free_request(&proto);
                goto fail;
            }

            list_push(proto.header, strdup(buf));
        }
        

        if(feof(fp)) {
            http_free_request(&proto);
            break;
        }

        int i = 0;
        list_each(proto.header, h) {
            fprintf(stdout, "LINE: %s\n", h);
            char* c = strstr(h, ": ");
            if(!c) {
                if(i > 0) {
                    http_response(fp, "400 Bad Request", "Bad request: expected colon \':\'");
                    http_free_request(&proto);
                    goto fail;
                }


                int w = 0;
                switch(h[0]) {
                    case HTTP_REQUEST_GET:
                        if(strstr(h, "GET ") != h)
                            break;

                        proto.request_type = HTTP_REQUEST_GET;
                        w = 4;
                        break;

                    case HTTP_REQUEST_POST:
                        if(strstr(h, "POST ") != h)
                            break;

                        proto.request_type = HTTP_REQUEST_POST;
                        w = 5;
                        break;

                    case HTTP_REQUEST_HEAD:
                        if(strstr(h, "HEAD ") != h)
                            break;

                        proto.request_type = HTTP_REQUEST_HEAD;
                        w = 5;
                        break;
                    default:
                        //proto.request_type = HTTP_REQUEST_UNKNOWN;
                        break;
                }

                if(proto.request_type == HTTP_REQUEST_UNKNOWN) {
                    http_response(fp, "501 Not Implemented", "Not implemented: the request type sent is not yet supported.");
                    http_free_request(&proto);
                    goto fail;
                }



                char* v;
                if(!(v = strstr(h, "HTTP/"))) {
                    http_response(fp, "400 Bad Request", "Bad request: expected HTTP version.");
                    http_free_request(&proto);
                    goto fail;
                }

                proto.version = strndup(v, 8);


                if((v = strstr(h, "?")))
                    proto.query = strdup(&v[1]);

            } else {
                if(i == 0) {
                    http_response(fp, "400 Bad Request", "Bad request: First line was not a valid request syntax.");
                    http_free_request(&proto);
                    goto fail;
                }

                c[0] = '\0';
                c++; c++;

                char* v;
                if((v = strstr(c, "\r")))
                    v[0] = '\0';
                else if((v = strstr(c, "\n")))
                    v[0] = '\0';

                
                
                
                #define if_eq(x) \
                    if(strcmp(h, x) == 0)

                if_eq("Host")
                    proto.host = strdup(c);
                else if_eq("Content-Type")
                    proto.content_type = strdup(c);
                else if_eq("Cookie")
                    proto.cookie = strdup(c);
                else if_eq("User-Agent")
                    proto.user_agent = strdup(c);
                else if_eq("Referer")
                    proto.referer = strdup(c);
                else if_eq("Content-Length")
                    proto.content_length = atoi(c);

                #undef if_eq
            }
        }
    } while(1);
    printf("NOOOOO\n");
fail:
printf("FIIIIIIIIIIIIIIIIIAAAAAAAAAIIIIIIIIIIILLLLLLLLL\n");
    (void) 0;
}


int httpd(int port) {
    struct sockaddr_in sin;
    sock_server = socket(AF_INET, SOCK_STREAM, 0);
    sin.sin_family = AF_INET;
    sin.sin_port = __htons(port);
    sin.sin_addr.s_addr = INADDR_ANY;


    int on = 1;
    if(setsockopt(sock_server, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) < 0) {
        perror("setsockopt");
        //goto error;
    }


    if(bind(sock_server, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
        perror("bind");
        goto error;
    }



    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGQUIT, on_signal);
    signal(SIGPIPE, SIG_IGN);


    listen(sock_server, 64);
    printf("httpd: [info] Server \'%s\' started on port %d\n", SERVER_VERSION, port);

    do {
        socket_request_t* r = (socket_request_t*) calloc(sizeof(socket_request_t), 1);
        if(!r) {
            perror("memory");
            break;
        }


        socklen_t len = sizeof(r->address);

        r->socket = accept (
            sock_server, 
            (struct sockaddr*) &r->address, 
            &len
        );
printf("httpd: [info] Server ACCEPTED\n");
        r->thread = async(on_request(arg), r);
    } while(1);

error:
    close(sock_server);
    return -1;
}