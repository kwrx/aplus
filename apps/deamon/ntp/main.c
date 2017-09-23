#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <aplus/base.h>
#include <aplus/sysconfig.h>


#define SNTP_LI_NO_WARNING      0xC0
#define SNTP_VERSION            (4 << 3)

#define SNTP_MODE_MASK          7
#define SNTP_MODE_CLIENT        3
#define SNTP_MODE_SERVER        4
#define SNTP_MODE_BROADCAST     5

#define SNTP_MSG_LEN            48


static void show_usage(int argc, char** argv) {
    printf(
        "Use: ntp\n"
        "Synchronize system time with network global time.\n\n"
        "       --deamon                run as deamon [ntpd]\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus sysutils) 0.1\n"
        "Copyright (c) 2017 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}


static void do_ntp() {
    char* host = (char*) sysconfig("ntpd.server", SYSCONFIG_FORMAT_STRING, (uintptr_t) NULL);
    if(!host) {
        fprintf(stderr, "ntpd: invalid configuration for \'ntpd.server\' in /etc/config\n");
        return;
    }

    struct hostent* e = gethostbyname(host);
    if(!e) {
        fprintf(stderr, "ntpd: host not found: %s\n", host);
        return;
    }

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd < 0) {
        perror("ntpd: socket()");
        return;
    }

    struct sockaddr_in in;
    in.sin_family = AF_INET;
    in.sin_port = __builtin_bswap16(123);
    memcpy(&in.sin_addr.s_addr, e->h_addr_list[0], e->h_length);


    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_port = __builtin_bswap16(INADDR_ANY);
    local.sin_addr.s_addr = __builtin_bswap32(INADDR_ANY);
    
    if(bind(fd, (struct sockaddr*) &local, sizeof(local)) != 0) {
        perror("ntpd: bind()");
        return;
    }

    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(timeout)) < 0) {
        perror("ntpd: setsockopt()");
        return;
    }
    
    struct {
        uint8_t li_vn_mode;
        uint8_t stratum;
        uint8_t poll;
        uint8_t precision;
        uint32_t root_delay;
        uint32_t root_dispersion;
        uint32_t reference_id;
        uint32_t reference_timestamp[2];
        uint32_t originate_timestamp[2];
        uint32_t receive_timestamp[2];
        uint32_t transmit_timestamp[2];
    } __attribute__ ((packed)) ntp;

    memset(&ntp, 0, sizeof(ntp));
    ntp.li_vn_mode = 0x1B; //SNTP_LI_NO_WARNING | SNTP_VERSION | SNTP_MODE_CLIENT;

    if(sendto(fd, &ntp, SNTP_MSG_LEN, 0, (struct sockaddr*) &in, sizeof(in)) < 0) {
        perror("ntpd: sendto()");
        return;
    }
    
    socklen_t len = sizeof(in);
    if(recvfrom(fd, &ntp, SNTP_MSG_LEN, 0, (struct sockaddr*) &in, &len) != SNTP_MSG_LEN) {
        if(errno == EAGAIN)
            fprintf(stderr, "ntpd: server not responding: %s\n", host);
        else
            perror("ntpd: recvfrom()");
        return;
    }
    
    if(!(((ntp.li_vn_mode & SNTP_MODE_MASK) == SNTP_MODE_SERVER) ||
         ((ntp.li_vn_mode & SNTP_MODE_MASK) == SNTP_MODE_BROADCAST)))
        fprintf(stderr, "ntpd: invalid response from host %s\n", host);
    

    fprintf(stderr, "ntpd: received_timestamp: %d : %d : %d\n", ntp.receive_timestamp[0], ntp.receive_timestamp[1], time(NULL));
    close(fd);
}


int main(int argc, char** argv) {
    
    static struct option long_options[] = {
        { "deamon", no_argument, NULL, 'd'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    
    
    int deamon = 0;
    int c, idx;
    while((c = getopt_long(argc, argv, "d", long_options, &idx)) != -1) {
        switch(c) {
            case 'd':
                deamon++;
                break;
            case 'v':
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
  

    if(!deamon)
        do_ntp();
    else {
        if(strcmp((const char*) sysconfig("ntpd.enabled", SYSCONFIG_FORMAT_STRING, (uintptr_t) "false"), "true") != 0) {
            fprintf(stderr, "ntpd: deamon disabled by /etc/config\n");
            return 0;
        }

        if(strcmp(argv[0], "[ntpd]") != 0)
            execl("/proc/self/exe", "[ntpd]", "--deamon", NULL);
        
        
        int s = (int) sysconfig("ntpd.timeout", SYSCONFIG_FORMAT_INT, 10);
        
        FILE* fp = fopen("/dev/log", "w");
        if(fp)
            stderr = fp;

        fprintf(stderr, "ntpd: running as deamon every %d seconds\n", s);


        for(;; sleep(s))
            do_ntp();
    }

    return 0;
}