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



// static uint64_t resource_ids = 0;


// void virtgpu_update_buffers(device_t* device) {
    
// }


// void virtgpu_free_buffers(device_t* device) {

//     DEBUG_ASSERT(device);

//     if(unlikely(!device->address))
//         return;


//     DEBUG_ASSERT(device->address);
//     DEBUG_ASSERT(device->size);


//     pmm_free_blocks(device->address, device->size / PML1_PAGESIZE + 1);

// }


// void virtgpu_free_resources(struct virtio_driver* driver) {

//     for(size_t i = 0; i < resource_ids; i++) {
        
//         virtgpu_cmd_resource_detach_backing(driver, i);
//         virtgpu_cmd_resource_unref(driver, i);

//     }

//     virtq_flush(driver);

// }


