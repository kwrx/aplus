#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/sched.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

#include <aplus/base.h>
#include <aplus/pwm.h>


#define S_HALT                          1
#define S_POWEROFF                      2
#define S_REBOOT                        3



static void show_usage(int argc, char** argv) {
    printf(
        "Use: shutdown [OPTIONS...] [TIME] [WALL...]\n"
        "Shut down the system.\n\n"
        "   -H, --halt                  Halt the machine\n"
        "   -P, --poweroff              Power-off the machine\n"
        "   -r, --reboot                Reboot the machine\n"
        "   -h                          Equivalent to --poweroff, overridden by --halt\n"
        "       --no-wall               Don't send wall message before halt/power-off/reboot"
        "   -c                          Cancel a pending shutdown"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus coreutils) 0.1\n"
        "Copyright (c) 2016 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}



static int shutdown_exec(uint8_t mode) {
    fprintf(stdout, "\n Shutdown:\n");
    unlink("/tmp/shutdown.lock");
    


    fprintf(stdout, "\t- Killing processes\n");

    DIR* d = opendir("/proc");
    if(!d) {
        perror("/proc");
        return -1;
    }

    struct dirent* ent;
    while((ent = readdir(d))) {
        if(!isdigit(ent->d_name[0]))
            continue;

        int pid = atoi(ent->d_name);
        if(getpid() != pid)
            if(kill(pid, SIGKILL) == 0)
                waitpid(pid, NULL, 0);
    }

    closedir(d);
    fprintf(stdout, "\t- Done! Goodbye!\n");



    int fd = open(PATH_PWM, O_RDONLY);
    if(fd < 0) {
        perror(PATH_PWM);
        return -1;
    }


    switch(mode) {
        case S_POWEROFF:
            ioctl(fd, PWMIOCTL_POWEROFF, NULL);
            break;
        case S_REBOOT:
            ioctl(fd, PWMIOCTL_REBOOT, NULL);
            break;
        case S_HALT:
            ioctl(fd, PWMIOCTL_HALT, NULL);
            break;
    }

    close(fd);
    return 0;
}


static void shutdown_deamon() {
    if(fork())
        return;

    for(;;) {
        int fd = open("/tmp/shutdown.lock", O_RDONLY);
        if(fd < 0)
            exit(0);

        time_t tm;
        uint8_t mode;

        read(fd, &mode, sizeof(mode));
        read(fd, &tm, sizeof(tm));
        close(fd);

        if(time(NULL) > tm)
            exit(shutdown_exec(mode));

        sched_yield();
    }
}


static int shutdown(time_t tm, uint8_t mode) {
    int fd = open("/tmp/shutdown.lock", O_RDWR | O_CREAT | O_EXCL, S_IFREG | 0644);
    if(fd < 0) {
        if(errno != EEXIST) {
            perror("shutdown: /tmp/shutdown.lock");
            return -1;
        }

        fd = open("/tmp/shutdown.lock", O_RDWR | O_TRUNC);
    } else
        shutdown_deamon();
    

    if(fd < 0) {
        perror("shutdown: /tmp/shutdown.lock");
        return -1;
    }


    write(fd, &mode, sizeof(mode));
    write(fd, &tm, sizeof(tm));
    close(fd);

    char buf[32];
    strftime(buf, sizeof(buf), "%c", localtime(&tm));

    fprintf(stdout, "Shutdown scheduled for %s, use 'shutdown -c' to cancel.\n", buf);
    return 0;
}



int main(int argc, char** argv) {
    
    static struct option long_options[] = {
        { "halt", no_argument, NULL, 'H'},
        { "poweroff", no_argument, NULL, 'P'},
        { "reboot", no_argument, NULL, 'r'},
        { "no-wall", no_argument, NULL, 'w'},
        { "help", no_argument, NULL, 'k'},
        { "version", no_argument, NULL, 'q'},
        { NULL, 0, NULL, 0 }
    };
    

    uint8_t wall = 1;
    uint8_t mode = S_POWEROFF;
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "HPrhc", long_options, &idx)) != -1) {
        switch(c) {
            case 'c':
                unlink("/tmp/shutdown.lock");
                return 0;
            case 'H':
                mode = S_HALT;
                break;
            case 'P':
            case 'h':
                mode = S_POWEROFF;
                break;
            case 'r':
                mode = S_REBOOT;
                break;
            case 'w':
                wall = 0;
                break; 
            case 'q':
                show_version(argc, argv);
                break;
            case 'k':
            case '?':
                show_usage(argc, argv);
                break;
            default:
                abort();
        }
    }
    
    
    if(optind >= argc)
        return shutdown(time(NULL) + 60, mode);
        
    if(optind + 1 >= argc)
        if(strcmp(argv[optind], "now") == 0)
            return shutdown(time(NULL), mode);
        else
            return shutdown(time(NULL) + atoi(argv[optind]), mode);
    
    return 0;
}