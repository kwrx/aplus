/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sched.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/auxv.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <termios.h>

#include <aplus/fb.h>
#include <aplus/ulog.h>

#include <pthread.h>


#define __trim(str)       \
    while (isblank(*str)) \
    str++


static void sig_shutdown_handler(int sig) {

    switch (sig) {
        case SIGTERM:
            ulog_infof("received shutdown request, rebooting...");
            break;
        case SIGUSR1:
            ulog_infof("received shutdown request, halting...");
            break;
        case SIGUSR2:
            ulog_infof("received shutdown request, powering off...");
            break;
    }

    kill(-1, SIGTERM);
    kill(-1, SIGHUP);
    kill(-1, SIGCONT);
    for(size_t timeout = 0; timeout < 5; timeout++) {
        errno = 0;
        while(waitpid(-1, NULL, WNOHANG) > 0)
            ;
        if (errno == ECHILD)
            break;
        ulog_infof("waiting for processes to terminate...");
        sleep(1);
    }

    kill(-1, SIGKILL);
    for(size_t timeout = 0; timeout < 3; timeout++) {
        errno = 0;
        while(waitpid(-1, NULL, WNOHANG) > 0)
            ;
        if (errno == ECHILD)
            break;
        ulog_infof("waiting for processes to forcefully terminate...");
        sleep(1);
    }

    sync();

    switch (sig) {

        case SIGTERM:
            reboot(RB_AUTOBOOT);
            break;

        case SIGUSR2:
            reboot(RB_POWER_OFF);
            break;

        case SIGUSR1:
            reboot(RB_HALT_SYSTEM);
            break;

    }

    for(;;)
        pause();

}




static void init_framebuffer(void) {

    int fd = open("/dev/fb0", O_RDONLY);

    if (fd < 0)
        return (void)ulog_warnf("fb0: no framebuffer device found");


    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;

    if (ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0)
        return (void)ulog_errorf("fb0: unable to get screen info: %s", strerror(errno));

    if (ioctl(fd, FBIOPUT_VSCREENINFO, &var) < 0)
        return (void)ulog_errorf("fb0: unable to set screen info: %s", strerror(errno));

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0)
        return (void)ulog_errorf("fb0: unable to get screen info: %s", strerror(errno));

#if defined(DEBUG)
    ulog_debugf("fb0: initialized framebuffer device %dx%dx%d [ptr(%p), size(%p)]", var.xres, var.yres, var.bits_per_pixel, (void*)((uintptr_t)fix.smem_start), (void*)((uintptr_t)fix.smem_len));
#endif

    close(fd);
}


static void init_welcome(void) {

    FILE* fp = fopen("/etc/motd", "r");

    if (!fp)
        return;


    char line[BUFSIZ];

    while (fgets(line, sizeof(line), fp) > 0) {
        fwrite(line, strlen(line), 1, stdout);
    }


    fflush(stdout);
    fclose(fp);
}


static void init_environment(void) {

    FILE* fp = fopen("/etc/environment", "r");
    if (!fp)
        return (void)ulog_warnf("env: no environment file found");

    char line[BUFSIZ];
    while (fgets(line, sizeof(line), fp) > 0) {

        if (line[strlen(line) - 1] == '\n') {
            line[strlen(line) - 1] = '\0';
        }

        char* p = &line[0];
        __trim(p);

        switch (*p) {

            case '\0':
            case '#':
                continue;

            default:
                if (strchr(p, '=')) {
                    char* entry = strdup(p);
                    if (entry)
                        putenv(entry);
                }

                break;
        }
    }


    fclose(fp);
}


static void init_fstab() {

    FILE* fp = fopen("/etc/fstab", "r");

    if (!fp)
        return (void)ulog_errorf("fstab: no fstab config file found");


    char line[BUFSIZ];
    while (fgets(line, sizeof(line), fp) > 0) {

        if (line[strlen(line) - 1] == '\n')
            line[strlen(line) - 1] = '\0';


        char* p = &line[0];
        __trim(p);


        switch (*p) {

            case '\0':
            case '#':
                continue;

            default: {

                int flags = 0;

                char dev[128];
                char dir[128];
                char fs[32];
                char fl[128];

                if (sscanf(p, "%127s %127s %31s %127s", dev, dir, fs, fl) != 4)
                    break;

#if defined(DEBUG)
                ulog_debugf("fstab: mount device %s in %s [fstype(%s), flags(%s)]", dev, dir, fs, fl);
#endif

                char* tok = fl;
                for (char* k = strtok_r(fl, ",", &tok); k; k = strtok_r(NULL, ",", &tok)) {

                    #define has(str, flag)         \
                        if (strcmp(k, str) == 0) { \
                            flags |= flag;         \
                            continue;              \
                        }

                    has("defaults", MS_NOATIME | MS_SHARED | MS_NOSUID);
                    has("noatime", MS_NOATIME);
                    has("nodev", MS_NODEV);
                    has("nodiratime", MS_NODIRATIME);
                    has("dirsync", MS_DIRSYNC);
                    has("noexec", MS_NOEXEC);
                    has("iversion", MS_I_VERSION);
                    has("mand", MS_MANDLOCK);
                    has("relatime", MS_RELATIME);
                    has("lazytime", MS_LAZYTIME);
                    has("nosuid", MS_NOSUID);
                    has("remount", MS_REMOUNT);
                    has("ro", MS_RDONLY);
                    has("sync", MS_SYNCHRONOUS);
                    has("nouser", MS_NOUSER);
                    has("silent", MS_SILENT);
                }


                if (mount(dev, dir, fs, flags, NULL) < 0)
                    ulog_errorf("fstab: mount(%s, %s): %s", dev, dir, strerror(errno));


            } break;
        }
    }

    fclose(fp);
}


static void init_hostname() {

    FILE* fp = fopen("/etc/hostname", "r");

    if (!fp)
        return;


    char hostname[BUFSIZ] = {0};

    if (fgets(hostname, BUFSIZ, fp) == NULL)
        return;

    if (fclose(fp) < 0)
        return;


    if (strlen(hostname) > 0) {

        for (size_t i = 0; i < sizeof(hostname); i++) {

            if (isalnum(hostname[i]) || hostname[i] == '-' || hostname[i] == '.')
                continue;

            hostname[i] = '\0';
            break;
        }

        syscall(SYS_sethostname, hostname, strlen(hostname));
    }

    ulog_debugf("hostname: set hostname '%s'", hostname);
}




int main(int argc, char** argv, char** envp) {

    int null = open("/dev/null", O_RDONLY);
    int kmsg = open("/dev/kmsg", O_WRONLY);

    if (kmsg < 0 || null < 0)
        return 1;

    dup2(null, STDIN_FILENO);
    dup2(kmsg, STDOUT_FILENO);
    dup2(kmsg, STDERR_FILENO);

    if (null > STDERR_FILENO)
        close(null);

    if (kmsg > STDERR_FILENO)
        close(kmsg);


#if defined(DEBUG)

    if (getpid() != 1)
        ulog_warnf("getpid() != 1");


    ulog_debugf("pid(%d) ppid(%d) gid(%d) uid(%d)", getpid(), getppid(), getgid(), getuid());
    ulog_debugf("argc(%d) argv(%p) envp(%p)", argc, argv, envp);

    for (size_t i = 0; i < argc; i++)
        ulog_debugf("argv[%zu] = %s (%p)", i, argv[i], argv[i]);


    ulog_debugf("auxval(AT_RANDOM)        = %zu", getauxval(AT_RANDOM));
    ulog_debugf("auxval(AT_PAGESZ)        = %zu", getauxval(AT_PAGESZ));
    ulog_debugf("auxval(AT_HWCAP)         = %zu", getauxval(AT_HWCAP));
    ulog_debugf("auxval(AT_HWCAP2)        = %zu", getauxval(AT_HWCAP2));
    ulog_debugf("auxval(AT_CLKTCK)        = %zu", getauxval(AT_CLKTCK));
    ulog_debugf("auxval(AT_UID)           = %zu", getauxval(AT_UID));
    ulog_debugf("auxval(AT_GID)           = %zu", getauxval(AT_GID));
    ulog_debugf("auxval(AT_EUID)          = %zu", getauxval(AT_EUID));
    ulog_debugf("auxval(AT_EGID)          = %zu", getauxval(AT_EGID));
    ulog_debugf("auxval(AT_ENTRY)         = %zu", getauxval(AT_ENTRY));
    ulog_debugf("auxval(AT_FLAGS)         = %zu", getauxval(AT_FLAGS));
    ulog_debugf("auxval(AT_NULL)          = %zu", getauxval(AT_NULL));


#endif

    setsid();
    tcsetpgrp(STDIN_FILENO, getpgrp());

    init_environment();
    init_framebuffer();
    init_fstab();
    init_hostname();
    init_welcome();

    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    for (int cpu = 0; cpu < (CPU_SETSIZE << 3); cpu++)
        CPU_SET(cpu, &cpus);

    sched_setaffinity(0, sizeof(cpu_set_t), &cpus);

    sigset_t mask;
    sigemptyset(&mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);


    int e = fork();
    if (e < 0) {

        ulog_errorf("fork(): %s", strerror(errno));
        exit(EXIT_FAILURE);

    }

    else if (e == 0) {

        execle("/etc/init.sh", "/etc/init.sh", NULL, environ);
        exit(EXIT_FAILURE);

    } else {

        setpriority(0, PRIO_PROCESS, 19);

#if defined(DEBUG)
        ulog_debugf("going to sleep...");
#endif

        struct sigaction sa = {0};
        sa.sa_handler = sig_shutdown_handler;
        sigfillset(&sa.sa_mask);
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGUSR1, &sa, NULL);
        sigaction(SIGUSR2, &sa, NULL);

        do {
            waitpid(-1, NULL, 0);
        } while (errno != ECHILD);

        ulog_errorf("unreachable point! system halted!");

        for (;;)
            pause();

    }

}
