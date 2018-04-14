#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <aplus/task.h>
#include <libc.h>

#include <aplus/base.h>
#include <aplus/events.h>
#include <aplus/sysconfig.h>
#include <aplus/utils/list.h>

#include <sys/termio.h>
#include <sys/termios.h>
#include <sys/ioctl.h>


int tty_deamon(void* unused) {
    (void) unused;

    current_task->name = "[ttyd]";
    current_task->description = "System's global TTY deamon";
    current_task->priority = TASK_PRIO_MIN;




    int fd = sys_open("/dev/ev0", O_RDONLY, 0);
    if(fd < 0) {
        kprintf(ERROR "tty: /dev/ev0: could not open\n");
        sys_exit(-1);
    }

    int stdio = sys_open("/dev/tty0", O_RDWR, 0);
    if(stdio < 0) {
        kprintf(ERROR "tty: /dev/tty0: could not open\n");
        sys_exit(-1);
    }


    int key_ctrl = 0;
    struct termios ios;
    sys_ioctl(stdio, TIOCGETA, &ios);


    for(;;) {
        event_t e;
        if(sys_read(fd, &e, sizeof(e)) <= 0) {
            kprintf(ERROR "tty: /dev/ev0: I/O error\n");
            break;
        }


        switch(e.ev_type) {
            case EV_KEY:
                switch(e.ev_key.vkey) {
                    case KEY_LEFTCTRL:
                    case KEY_RIGHTCTRL:
                        key_ctrl = e.ev_key.down;
                        continue;
                    default:
                        break;
                }

                if(!key_ctrl)
                    break;

                if(!e.ev_key.down)
                    break;

                switch(e.ev_key.vkey) {
                    case KEY_C:
                        sys_write(stdio, &ios.c_cc[VKILL], 1);
                        break;
                    case KEY_D:
                        sys_write(stdio, &ios.c_cc[VQUIT], 1);
                        break;
                    case KEY_Z:
                        sys_write(stdio, &ios.c_cc[VINTR], 1);
                        break;
                    case KEY_S:
                        sys_write(stdio, &ios.c_cc[VSTOP], 1);
                        break;
                    case KEY_Q:
                        sys_write(stdio, &ios.c_cc[VSTART], 1);
                        break;
                    default:
                        break;
                }
                break;

            default:
                continue;
        }
    }

    sys_close(fd);
    sys_exit(0);
}