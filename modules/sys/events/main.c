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


MODULE_NAME("sys/events");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


static evid_t nextid = 0;
static list(inode_t*, ev_nodes);
static list(event_device_t*, ev_devices);


static int events_ioctl(inode_t* inode, int req, void* data) {
    if(unlikely(!inode)) {
        errno = EINVAL;
        return -1;
    }

    switch(req) {
        case EVIOGID: {
            int i = 0;
            list_each(ev_devices, d) {
                if(i++ != ((long*) data) [0])
                    continue;

                ((long*) data) [1] = d->ed_id;
                return 0;
            }

            break;
        };

        case EVIOGNAME: {
            struct { 
                evid_t id; 
                char name[64];
            } *p = (void*) data;

            list_each(ev_devices, d) {
                if(d->ed_id != p->id)
                    continue;

                strcpy(p->name, d->ed_name);
                return 0;
            }

            break;
        };

        case EVIOGCAPS: {
            list_each(ev_devices, d) {
                if(d->ed_id != (evid_t) ((uintptr_t) data))
                    continue;

                return d->ed_caps;
            }

            break;
        }

        case EVIOGSTATUS: {
            list_each(ev_devices, d) {
                if(d->ed_id != (evid_t) ((uintptr_t) data))
                    continue;

                return d->ed_enabled;
            }

            break;
        }

        case EVIOSSTATUS: {
            list_each(ev_devices, d) {
                if(d->ed_id != (evid_t) ((long*) data) [0])
                    continue;

                d->ed_enabled = !!(((long*) data) [1]);
                return 0;
            }

            break;
        }

        case EVIOSEXCL: {
            list_each(ev_devices, d) {
                if(d->ed_id != (evid_t) ((long*) data) [0])
                    continue;

                d->ed_exclusive = ((long*) data) [1];
                return 0;
            }

            break;
        }
    }


    errno = ESRCH;
    return -1;
}


evid_t sys_events_device_add(char* name, int caps) {
    event_device_t* d = (event_device_t*) kmalloc(sizeof(event_device_t), GFP_KERNEL);
    memset(d, 0, sizeof(event_device_t));

    strcpy(d->ed_name, name);
    d->ed_caps = caps;
    d->ed_enabled = 0;
    d->ed_exclusive = -1;
    d->ed_id = nextid++;

    
    kprintf(INFO "events: registered device \'%s\'\n", name);

    list_push(ev_devices, d);
    return d->ed_id;
}

int sys_events_device_remove(evid_t id) {
    list_each(ev_devices, d) {
        if(d->ed_id != id)
            continue;



        kprintf(INFO "events: removed device \'%s\'\n", d->ed_name);

        kfree(d);
        list_remove(ev_devices, d);
        return 0;
    }

    errno = ESRCH;
    return -1;
}

int sys_events_device_set_enabled(evid_t id, int enabled) {
    list_each(ev_devices, d) {
        if(d->ed_id != id)
            continue;

        d->ed_enabled = !!(enabled);
        return 0;
    }

    errno = ESRCH;
    return -1;
}

int sys_events_device_set_caps(evid_t id, int caps) {
    list_each(ev_devices, d) {
        if(d->ed_id != id)
            continue;

        d->ed_caps = caps;
        return 0;
    }

    errno = ESRCH;
    return -1;
}

int sys_events_device_set_exclusive(evid_t id, int evno) {
    list_each(ev_devices, d) {
        if(d->ed_id != id)
            continue;

        d->ed_exclusive = evno;
        return 0;
    }

    errno = ESRCH;
    return -1;
}


int sys_events_raise(event_t* e) {
    list_each(ev_devices, d) {
        if(d->ed_id != e->ev_devid)
            continue;

        if(!d->ed_enabled)
            return 0;

        if(d->ed_exclusive == -1) {
            list_each(ev_nodes, n)
                vfs_write(n, e, -1, sizeof(event_t));

            return 0;
        } else {
            int i = 0;
            inode_t* p = NULL;

            list_each(ev_nodes, n) {
                if(i++ != d->ed_exclusive) 
                    continue;
                    
                p = n;
                break;
            }

            if(!p) {
                errno = ESRCH;
                return -1;
            }

            vfs_write(p, e, -1, sizeof(event_t));
            return 0;
        }
    }

    errno = ESRCH;
    return -1;
}


int init(void) {
    memset(&ev_nodes, 0, sizeof(ev_nodes));
    memset(&ev_devices, 0, sizeof(ev_devices));


    for(int i = 0; i < 4; i++) {
        char buf[BUFSIZ];
        memset(buf, 0, sizeof(buf));
        sprintf(buf, "/dev/ev%d", i);

        if(sys_mkfifo(buf, S_IFCHR | 0666) != 0) {
            kprintf(ERROR "events: could not create \'%s\'\n", buf);
            return E_ERR;
        }


        int fd = sys_open(buf, O_RDONLY, 0);
        if(fd < 0) {
            kprintf(ERROR "events: coult not open \'%s\'\n", buf);
            return E_ERR;
        }

        inode_t* e = current_task->fd[fd].inode;
        sys_close(fd);

        e->ioctl = events_ioctl;
        list_push(ev_nodes, e);
    }


    return E_OK;
}



int dnit(void) {
    return E_OK;
}
