#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/termios.h>

#include <aplus/base.h>
#include <aplus/fb.h>
#include <aplus/events.h>
#include <aplus/sysconfig.h>
#include <aplus/utils/list.h>
#include <aplus/utils/async.h>


#include <peach/peach.h>


typedef struct {
    int server;
    list(int, clients);

    struct {
        struct fb_fix_screeninfo fix;
        struct fb_var_screeninfo var;
    } screen;

    struct {
        struct {
            int x;
            int y;
            int z;
        } cursor;
    } input;
} peach_server_t;


static void show_usage(int argc, char** argv) {
    printf(
        "Use: peach [OPTIONS...]\n"
        "Desktop Manager Server\n\n"
        "   -k, --kill                  kill all running server\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus sysutils) 0.1\n"
        "Copyright (c) 2016 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}


static void die(char* s) {
    perror(s);
    exit(1);
}

static void msg_build(peach_msg_t* msg, size_t size, uint16_t type) {
    msg->magic = PEACH_MSG_MAGIC;
    msg->size = size - sizeof(msg);
    msg->type = type;
}

static void init_pipe(peach_server_t* pp) {
    pp->server = -1;

    int i;
    for(i = 0; i < 4096; i++) {
        char buf[256];
        memset(buf, 0, sizeof(buf));
        sprintf(buf, "/tmp/peach.%d", i);

        if(mkfifo(buf, 0666) != 0)
            continue;

        if((pp->server = open(buf, O_RDWR)) < 0) {
            perror(buf);
            exit(1);
        }

        break;
    }

    if(pp->server == -1)
        die("peach: server");


    static char display[8];
    sprintf(display, "%d", i);

    setenv("DISPLAY", display, 1);
}


static void init_screen(peach_server_t* pp) {
    setsid();
    tcsetpgrp(STDIN_FILENO, getpgrp());
    
    struct termios ios;
    tcgetattr(STDIN_FILENO, &ios);
    ios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    ios.c_oflag &= ~(OPOST);
    ios.c_cflag |= (CS8);
    ios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    tcsetattr(STDIN_FILENO, TCSANOW, &ios);


    int fd;
    if((fd = open((const char*) sysconfig("screen.device", "/dev/fb0"), O_RDONLY)) < 0)
        die("framebuffer");
    

    ioctl(fd, FBIOGET_VSCREENINFO, &pp->screen.var);

    if(pp->screen.var.xres == 0 || pp->screen.var.yres == 0 || pp->screen.var.bits_per_pixel == 0) {
        pp->screen.var.xres =
        pp->screen.var.xres_virtual = (uint32_t) sysconfig("screen.width", 800);
        pp->screen.var.yres = 
        pp->screen.var.yres_virtual = (uint32_t) sysconfig("screen.height", 600);
        pp->screen.var.bits_per_pixel = (uint32_t) sysconfig("screen.bpp", 32);
        pp->screen.var.activate = FB_ACTIVATE_NOW;

        ioctl(fd, FBIOPUT_VSCREENINFO, &pp->screen.var);
        ioctl(fd, FBIOGET_VSCREENINFO, &pp->screen.var);
    }

    ioctl(fd, FBIOGET_FSCREENINFO, &pp->screen.fix);
}



static int init_events(peach_server_t* pp) {
    async({
        int fd = open("/dev/ev1", O_RDONLY);
        if(fd < 0)
            die("events");
        

        event_t e;
        while(read(fd, &e, sizeof(e))) {
            switch(e.ev_type) {
                case EV_KEY:
                    break;
                case EV_REL:
                    arg->input.cursor.x += e.ev_rel.x;
                    arg->input.cursor.y += e.ev_rel.y;
                    break;
            }
        };

        close(fd);
    }, pp);
}


static void init_client(peach_server_t* pp, int fd) {
    peach_msg_welcome_t m;
    msg_build((peach_msg_t*) &m, sizeof(m), PEACH_MSG_WELCOME);

    memcpy(&m.fix, &pp->screen.fix, sizeof(m.fix));
    memcpy(&m.var, &pp->screen.var, sizeof(m.var));

    write(fd, &m, sizeof(m));
    fprintf(stderr, "peach: client #%d subscribed\n", fd);
}



int main(int argc, char** argv) {
   
    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'q'},
        { NULL, 0, NULL, 0 }
    };


    int c, idx;
    while((c = getopt_long(argc, argv, "k", long_options, &idx)) != -1) {
        switch(c) {
            case 'q':
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


    int ld = open("/dev/log", O_WRONLY);
    if(ld >= 0) {
        dup2(ld, STDOUT_FILENO);
        dup2(ld, STDERR_FILENO);
    }



    peach_server_t pp;
    memset(&pp, 0, sizeof(pp));


    init_pipe(&pp);
    init_screen(&pp);
    init_events(&pp);


    peach_msg_t msg;
    while(read(pp.server, &msg, sizeof(msg))) {
        if(msg.magic != PEACH_MSG_MAGIC)
            continue;

        switch(msg.type) {
            case PEACH_MSG_SUBSCRIBE: {
                char buf[msg.size];
                read(pp.server, buf, msg.size);

                int fd = open(buf, O_RDWR);
                if(fd < 0)
                    die("msg_subscribe");

                init_client(&pp, fd);
                list_push(pp.clients, fd);
            } break;
        }
    }


    return 0;
}