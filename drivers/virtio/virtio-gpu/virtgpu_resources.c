// /*
//  * Author:
//  *      Antonino Natale <antonio.natale97@hotmail.com>
//  * 
//  * Copyright (c) 2013-2019 Antonino Natale
//  * 
//  * 
//  * This file is part of aPlus.
//  * 
//  * aPlus is free software: you can redistribute it and/or modify
//  * it under the terms of the GNU General Public License as published by
//  * the Free Software Foundation, either version 3 of the License, or
//  * (at your option) any later version.
//  * 
//  * aPlus is distributed in the hope that it will be useful,
//  * but WITHOUT ANY WARRANTY; without even the implied warranty of
//  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  * GNU General Public License for more details.
//  * 
//  * You should have received a copy of the GNU General Public License
//  * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
//  */

// #include <stdint.h>
// #include <string.h>

// #include <aplus.h>
// #include <aplus/debug.h>
// #include <aplus/module.h>
// #include <aplus/memory.h>
// #include <aplus/smp.h>
// #include <aplus/hal.h>
// #include <aplus/fb.h>
// #include <aplus/errno.h>

// #include <dev/interface.h>
// #include <dev/video.h>
// #include <dev/pci.h>

// #include <dev/virtio/virtio.h>
// #include <dev/virtio/virtio-gpu.h>

// #include <stdint.h>


// int virtgpu_create_resource_2d(struct virtgpu* gpu, uint64_t* resource, uint32_t width, uint32_t height, uint32_t format, uintptr_t framebuffer) {

//     DEBUG_ASSERT(gpu);
//     DEBUG_ASSERT(gpu->driver);
//     DEBUG_ASSERT(resource);

//     int e;
//     uint64_t resource_id = ++gpu->resource_ids;


//     if((e = virtgpu_cmd_resource_create_2d(gpu, resource_id, format, width, height)) < 0)
//         return e;

//     if((e = virtgpu_cmd_resource_attach_backing(gpu, resource_id, framebuffer, width * height * virtgpu_get_bytes_per_pixel(format))) < 0)
//         return e;
    
//     return *resource = resource_id, 0;
    
// }

// int virtgpu_update_resource_2d(struct virtgpu* gpu, uint64_t resource, struct virtio_gpu_rect* rect, uint64_t offset) {

//     DEBUG_ASSERT(gpu);
//     DEBUG_ASSERT(gpu->driver);
//     DEBUG_ASSERT(rect);

//     return virtgpu_cmd_transfer_to_host_2d(gpu, resource, rect, offset);

// }

// void virtgpu_free_resources(struct virtgpu* gpu) {

//     for(size_t i = 0; i < gpu->resource_ids; i++) {
        
//         virtgpu_cmd_resource_detach_backing(gpu, i);
//         virtgpu_cmd_resource_unref(gpu, i);

//     }

// }


