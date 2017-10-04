#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#include <pthread.h>

static void show_usage(int argc, char** argv) {
    printf(
        "Usage: telnet [OPTION...] [HOST [PORT]]\n"
        "Login to remote system HOST (optionally, on service port PORT)\n"
        "\n"
        "General options:\n"
        "\n"
        "-4, --ipv4                 use only IPv4\n"
        "-6, --ipv6                 use only IPv6\n"
        "    --help                 give this help list\n"
        "-V, --version              print program version\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus coreutils) 0.1\n"
        "Copyright (c) 2017 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}


static void* thr_input_handler(void* arg) {
    int fd = (int) arg;

    for(;;) {
        char buf[BUFSIZ];
        if(read(fd, buf, BUFSIZ) < 0)
            break;

        fprintf(stdout, "%s", buf);
    }

    close(fd);
    pthread_exit(NULL);
}

int main(int argc, char** argv) {
    
    if(argc < 2)
        show_usage(argc, argv);
    
    static struct option long_options[] = {
        { "ipv4", no_argument, NULL, '4'},
        { "ipv6", no_argument, NULL, '6'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'r'},
        { NULL, 0, NULL, 0 }
    };
    
    
    int ipv = 4;
    int port = 23;
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "flsvh", long_options, &idx)) != -1) {
        switch(c) {
            case '4':
                ipv = 4;
                break;
            case '6':
                ipv = 6;
                break;
            case 'r':
                show_version(argc, argv);
                break;
            case 'h':
            case '?':
                show_usage(argc, argv);
                break;
            default:
                abort();
        }
    }
    
    if(optind + 1 >= argc)
        show_usage(argc, argv);

    if(optind + 2 >= argc)
        port = atoi(argv[optind + 1]);

    fprintf(stdout, "Trying %s:%d...\n", argv[optind], port);

    

    struct hostent* ent = gethostbyname(argv[optind]);
    if(!ent) {
        fprintf(stderr, "telnet: unable to resolve hostname: %s\n", argv[optind]);
        return -1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        perror("telnet");
        return -1;
    }

    struct sockaddr_in in;
    in.sin_family = AF_INET;
    in.sin_port = htons(port);
    memcpy(&in.sin_addr.s_addr, &ent->h_addr_list[0], ent->h_length);

    if(connect(fd, (struct sockaddr*) &in, sizeof(in)) < 0) {
        perror("telnet");
        return -1;
    }

    pthread_t thr_input;
    pthread_create(&thr_input, NULL, thr_input_handler, (void*) fd);


    char buf[BUFSIZ];
    while(fgets(buf, BUFSIZ, stdin)) {
        if(write(fd, buf, strlen(buf) - 1) == -1)
            break;

        if(write(fd, "\r\n", strlen("\r\n")) == -1)
            break;
    }

    fprintf(stderr, "Connection closed");
    pthread_detach(thr_input);
    close(fd);

    return 0;
}

        