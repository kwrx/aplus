#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include <aplus/gnx.h>
#include "gnxsrv.h"



static gnx_res_t* gnx_res_queue = NULL;


gnx_res_t* gnxsrv_resources_find(const char* name) {
    gnx_res_t* tmp;
    for(tmp = gnx_res_queue; tmp; tmp = tmp->next)
        if(strcmp(name, tmp->name) == 0)
            return tmp;
 
    return NULL;
}

int gnxsrv_resources_unload_by_name(const char* name) {
    gnx_res_t* res;
    if((res = gnxsrv_resources_find(name)) == NULL) {
        fprintf(stderr, "gnx-server: unable to unload resources %s\n", name);
        return -1;
    }
    
    if(--res->refcount)
        return 0;
    
    gnx_res_t* tmp = NULL;
    for(tmp = gnx_res_queue; tmp; tmp = tmp->next)
        if(tmp->next == res)
            break;
            
    if(!tmp && res != gnx_res_queue) {
        fprintf(stderr, "gnx-server: BUG!! %s::%s (%d)\n", __FILE__, __func__, __LINE__);
        return -1;
    }
    
    if(res == gnx_res_queue)
        gnx_res_queue = res->next;
    else
        tmp->next = res->next;
        
    switch(res->type) {
        case GNXRES_TYPE_UNKNOWN:
            free(res->data);
            break;
        case GNXRES_TYPE_FONT:
            TTF_CloseFont(res->font);
            break;
        case GNXRES_TYPE_IMAGE:
            SDL_FreeSurface(res->image);
            break;
        default:
            fprintf(stderr, "gnx-server: BUG!! %s::%s (%d)\n", __FILE__, __func__, __LINE__);
            return -1;
    }
    
    if(verbose)
        fprintf(stdout, "gnx-server: unloaded resource: %s\n", res->name);
        
    
    free(res->name);
    free(res);
    
    return 0;
}

int gnxsrv_resources_unload(void* data) {
    gnx_res_t* res;
    for(res = gnx_res_queue; res; res = res->next)
        if(res->data == data)
            break;
            
    if(!res) {
        fprintf(stderr, "gnx-server: unable to unload resources data %p\n", data);
        return -1;
    }
    
    if(--res->refcount)
        return 0;
    
    gnx_res_t* tmp = NULL;
    for(tmp = gnx_res_queue; tmp; tmp = tmp->next)
        if(tmp->next == res)
            break;
            
    if(!tmp && res != gnx_res_queue) {
        fprintf(stderr, "gnx-server: BUG!! %s::%s (%d)\n", __FILE__, __func__, __LINE__);
        return -1;
    }
    
    if(res == gnx_res_queue)
        gnx_res_queue = res->next;
    else
        tmp->next = res->next;
        
    switch(res->type) {
        case GNXRES_TYPE_UNKNOWN:
            free(res->data);
            break;
        case GNXRES_TYPE_FONT:
            TTF_CloseFont(res->font);
            break;
        case GNXRES_TYPE_IMAGE:
            SDL_FreeSurface(res->image);
            break;
        default:
            fprintf(stderr, "gnx-server: BUG!! %s::%s (%d)\n", __FILE__, __func__, __LINE__);
            return -1;
    }
    
    if(verbose)
        fprintf(stdout, "gnx-server: unloaded resource: %s\n", res->name);
        
    
    free(res->name);
    free(res);
    
    return 0;
}

void* gnxsrv_resources_load(const char* path, int type) {
    
    gnx_res_t* res;
    if(res = gnxsrv_resources_find(path)) {
        res->refcount++;
        return res->data;
    }
    
    
    switch(type) {
        case GNXRES_TYPE_UNKNOWN: {
            struct stat st;
            if(stat(path, &st) != 0) {
                fprintf(stderr, "gnx-server: %s: %s\n", path, strerror(errno));
                return NULL;
            }
            
            gnx_res_t* r = (gnx_res_t*) malloc(sizeof(gnx_res_t));
            r->name = strdup(path);
            r->type = type;
            r->data = malloc(st.st_size);
            r->refcount = 1;
            r->next = gnx_res_queue;
            
            if(!r->data) {
                fprintf(stderr, "gnx-server: %s: %s\n", path, strerror(errno));
                
                free(r->name);
                free(r->data);
                free(r);
                return NULL;
            }
            
            int fd = open(path, O_RDONLY);
            if(fd < 0) {
                fprintf(stderr, "gnx-server: %s: %s\n", path, strerror(errno));
                
                free(r->name);
                free(r->data);
                free(r);
                return NULL;
            }
            
            if(read(fd, r->data, st.st_size) != st.st_size) {
                fprintf(stderr, "gnx-server: %s: %s\n", path, strerror(errno));
                
                free(r->name);
                free(r->data);
                free(r);
                return NULL;
            }
            
            close(fd);
            gnx_res_queue = r;
            
            if(verbose)
                fprintf(stdout, "gnx-server: loaded unknown resources: %s (%d Bytes)\n", path, st.st_size);
        
            return r->data;
        } break;
        
        case GNXRES_TYPE_FONT: {
            struct stat st;
            if(stat(path, &st) != 0) {
                fprintf(stderr, "gnx-server: %s: %s\n", path, strerror(errno));
                return NULL;
            }
            
            gnx_res_t* r = (gnx_res_t*) malloc(sizeof(gnx_res_t));
            r->name = strdup(path);
            r->type = type;
            r->refcount = 1;
            r->next = gnx_res_queue;
            
            r->font = TTF_OpenFont(path, 16);
            if(!r->font) {
                fprintf(stderr, "gnx-server: %s: %s\n", path, TTF_GetError());
                
                free(r->name);
                free(r);
                return NULL;
            }
            
            gnx_res_queue = r;
            
            if(verbose)
                fprintf(stdout, "gnx-server: loaded font: %s (%d Bytes)\n", path, st.st_size);
        
            return r->data;
        } break;
        
        case GNXRES_TYPE_IMAGE: {
            struct stat st;
            if(stat(path, &st) != 0) {
                fprintf(stderr, "gnx-server: %s: %s\n", path, strerror(errno));
                return NULL;
            }
            
            gnx_res_t* r = (gnx_res_t*) malloc(sizeof(gnx_res_t));
            r->name = strdup(path);
            r->type = type;
            r->refcount = 1;
            r->next = gnx_res_queue;
            
            r->image = IMG_Load(path);
            if(!r->image) {
                fprintf(stderr, "gnx-server: %s: %s\n", path, IMG_GetError());
                
                free(r->name);
                free(r);
                return NULL;
            }
            
            gnx_res_queue = r;
            
            if(verbose)
                fprintf(stdout, "gnx-server: loaded image: %s (%d Bytes)\n", path, st.st_size);
        
            return r->data;
        } break;
        
        default:
            fprintf(stderr, "gnx-server: invalid resource type\n");
            return NULL;
    }
    
    return NULL;
}