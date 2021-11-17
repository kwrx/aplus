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

#include <stdint.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/memory.h>
#include <aplus/hal.h>
#include <aplus/errno.h>

#include <dev/interface.h>
#include <dev/video.h>

#include <aplus/fb.h>



int video_resource_create_2d(device_video_context_t* context, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {

    for(size_t i = 0; i < DEVICE_VIDEO_MAX_RESOURCES; i++) {

        if(unlikely(context->resources[i].flags & DEVICE_VIDEO_RESOURCE_FLAGS_ENABLED))
            continue;

        context->resources[i].flags |= DEVICE_VIDEO_RESOURCE_FLAGS_ENABLED;
        context->resources[i].flags |= DEVICE_VIDEO_RESOURCE_FLAGS_2D;

        context->resources[i].x = x;
        context->resources[i].y = y;
        context->resources[i].width = width;
        context->resources[i].height = height;

        context->resources[i].buffer = (uintptr_t) kmalloc(width * height * (context->root.bpp >> 3), GFP_USER);


        if(unlikely(!context->resources[i].buffer)) {
            context->resources[i].flags &= ~DEVICE_VIDEO_RESOURCE_FLAGS_ENABLED;
            return -ENOMEM;
        }


        context->resources_count += 1;

        return i;

    }


    return -ENOSPC;

}


int video_resource_create_2d(device_video_context_t* context, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    return kpanic("not yet implemented");
}


int video_resource_destroy(device_video_context_t* context, int id) {

    if(unlikely(id < 0 || id >= DEVICE_VIDEO_MAX_RESOURCES))
        return -EINVAL;

    if(unlikely(!(context->resources[id].flags & DEVICE_VIDEO_RESOURCE_FLAGS_ENABLED)))
        return -ENOENT;

    if(unlikely(context->resources[id].flags & DEVICE_VIDEO_RESOURCE_FLAGS_2D))
        kfree((void*) context->resources[id].buffer);


    context->resources[id].flags &= ~DEVICE_VIDEO_RESOURCE_FLAGS_ENABLED;
    context->resources_count -= 1;

    return 0;

}


int video_resource_lock(device_video_context_t* device, int id) {

    if(unlikely(id < 0 || id >= DEVICE_VIDEO_MAX_RESOURCES))
        return -EINVAL;

    if(unlikely(!(device->resources[id].flags & DEVICE_VIDEO_RESOURCE_FLAGS_ENABLED)))
        return -ENOENT;

    
    spinlock_lock(&device->resources[id].lock);

    return 0;

}


int video_resource_unlock(device_video_context_t* device, int id) {
    
    if(unlikely(id < 0 || id >= DEVICE_VIDEO_MAX_RESOURCES))
        return -EINVAL;
    
    if(unlikely(!(device->resources[id].flags & DEVICE_VIDEO_RESOURCE_FLAGS_ENABLED)))
        return -ENOENT;


    spinlock_unlock(&device->resources[id].lock);

    return 0;
        
}
