#include "config.h"


static int pipefd;
client_t* client_queue = NULL;


static client_t* client_get(void* data) {
    client_t* tmp;
    for(tmp = client_queue; tmp; tmp = tmp->next)
        if(tmp->data == data)
            return tmp;

    return NULL;
}

static void client_remove(client_t* cl) {
    if(!client_queue)
        return;

    if(client_queue == cl)
        client_queue = client_queue->next;
    else {
        client_t* tmp;
        for(tmp = client_queue; tmp->next; tmp = tmp->next) {
            if(tmp->next != cl)
                continue;

            tmp->next = cl->next;
            break;
        }
    }


    fprintf(stdout, "gnx: client_remove(%p): %d:%d %dx%d\n", cl->data, cl->data->x, cl->data->y, cl->data->w, cl->data->h);

    cairo_surface_destroy(cl->surface);
    cairo_destroy(cl->cr);
    kmem_free(cl->data);
    free(cl);
}

static void client_update(client_t* cl) {
    if(cl->surface)
        cairo_surface_destroy(cl->surface);

    if(cl->cr)
        cairo_destroy(cl->cr);

    cl->surface = cairo_image_surface_create_for_data (
        (unsigned char*) cl->data->frame,
        surface_format,
        cl->data->w,
        cl->data->h,
        cl->data->w * (surface_bpp / 8)
    );

    if(!cl->surface) {
        fprintf(stdout, "gnx: cairo_image_create_for_data(cl->surface) failed!\n");
        exit(-1);
    }

    cl->cr = cairo_create(cl->surface);
    if(!cl->cr) {
        fprintf(stdout, "gnx: cairo_create(cl->cr) failed\n");
        exit(-1);
    }

    fprintf(stdout, "gnx: client_update(%p): %d:%d %dx%d\n", cl->data, cl->data->x, cl->data->y, cl->data->w, cl->data->h);
}


static void client_add(void* data) {
    client_t* cl = (client_t*) calloc(sizeof(client_t), 1);
    if(!cl) {
        fprintf(stdout, "gnx: calloc(client_t) failed!\n");
        exit(-1);
    }

    cl->data = data;
    cl->next = client_queue;
    client_queue = cl;

    fprintf(stdout, "gnx: client_add(%p): %d:%d %dx%d\n", data, cl->data->x, cl->data->y, cl->data->w, cl->data->h);
    client_update(cl);
}




void init_clients(void) {
    if(access(GNX_PIPE, F_OK) == 0) {
        fprintf(stdout, "gnx: server already running\n");
        exit(-1);
    }


    if(mkfifo(GNX_PIPE, 0666) != 0) {
        fprintf(stdout, "gnx: mkfifo(" GNX_PIPE ") failed\n!");
        exit(-1);
    }

    pipefd = open(GNX_PIPE, O_RDWR);
    if(pipefd < 0) {
        fprintf(stdout, "gnx: open(" GNX_PIPE ") failed\n");
        exit(-1);
    }
}


void* th_clients(void* arg) {
    fprintf(stdout, "gnx: initialized client controller: #%d\n", getpid());

    for(;; 
        usleep(50000)
    ) {

        void* data;
        if(!read(pipefd, &data, sizeof(void*))) {
            fprintf(stdout, "gnx: warning! read(" GNX_PIPE ") failed!\n");
            continue;
        }

        
        client_t* cl;
        if((cl = client_get(data))) {
            if(!cl->data->w && !cl->data->h)
                client_remove(cl);
            else
                client_update(cl);
        } else
            client_add(data);
    }
}
