/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2024 Antonino Natale
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

// #[import_c_fn(arch_vmm_p2v)]
// #[import_c_fn(arch_vmm_v2p)]
// #[import_c_fn(pmm_alloc_block)]
// #[import_c_fn(pmm_alloc_blocks)]
// #[import_c_fn(pmm_free_block)]
// #[import_c_fn(pmm_free_blocks)]
// #[import_c_var(GFP_ATOMIC)]
// #[import_c_var(GFP_KERNEL)]
// #[import_c_var(GFP_USER)]
// #[import_c_var(PML1_PAGESIZE)]
// #[import_c_var(ARCH_VMM_AREA_HEAP)]

use crate::bindings::{arch_vmm_p2v, arch_vmm_v2p, ARCH_VMM_AREA_HEAP};
use crate::bindings::{pmm_alloc_block, pmm_alloc_blocks, pmm_free_block, pmm_free_blocks};
use crate::bindings::{GFP_ATOMIC, GFP_KERNEL, GFP_USER, PML1_PAGESIZE};
use crate::ipc::mutex::Mutex;
use lazy_static::lazy_static;

pub struct KernelHeap {
    heap_used_blocks: usize,
}

#[repr(C, packed)]
pub struct KmallocHeader {
    #[cfg(debug_assertions)]
    magic: [u8; 4],
    #[cfg(not(debug_assertions))]
    _unused_magic: [u8; 4],
    blocks: u32,
    size: usize,
}

impl KernelHeap {
    /**
     * Create a new kernel heap.
     */
    pub fn new() -> Self {
        KernelHeap {
            heap_used_blocks: 0,
        }
    }

    /**
     * Allocate memory into the kernel heap.
     */
    pub fn allocate(&mut self, size: usize, gfp: u32) -> *mut u8 {
        debug_assert!(size > 0);
        debug_assert!(gfp == GFP_KERNEL || gfp == GFP_ATOMIC || gfp == GFP_USER);

        let mut data_size = size + core::mem::size_of::<KmallocHeader>();

        if data_size & (PML1_PAGESIZE as usize - 1) != 0 {
            data_size = (size & !(PML1_PAGESIZE as usize - 1)) + PML1_PAGESIZE as usize;
        }

        let header_addr;

        if data_size == PML1_PAGESIZE as usize {
            header_addr = unsafe { arch_vmm_p2v(pmm_alloc_block(), ARCH_VMM_AREA_HEAP as i32) } as *mut u8;
        } else {
            header_addr = unsafe {
                arch_vmm_p2v(
                    pmm_alloc_blocks(data_size / PML1_PAGESIZE as usize),
                    ARCH_VMM_AREA_HEAP as i32,
                ) as *mut u8
            }
        }

        self.heap_used_blocks += data_size / PML1_PAGESIZE as usize;

        if header_addr == core::ptr::null_mut() {
            return core::ptr::null_mut();
        }

        let header = unsafe { &mut *(header_addr as *mut KmallocHeader) };

        debug_assert!(header.magic != ['U' as u8, 'S' as u8, 'E' as u8, 'D' as u8]);

        #[cfg(debug_assertions)]
        {
            header.magic[0] = 'U' as u8;
            header.magic[1] = 'S' as u8;
            header.magic[2] = 'E' as u8;
            header.magic[3] = 'D' as u8;
        }

        header.blocks = (data_size / PML1_PAGESIZE as usize) as u32;
        header.size = size;

        unsafe { header_addr.add(core::mem::size_of::<KmallocHeader>()) as *mut u8 }
    }

    /**
     * Allocate zeroed memory into the kernel heap.
     */
    pub fn allocate_zeroed(&mut self, size: usize, gfp: u32) -> *mut u8 {
        let ptr = self.allocate(size, gfp);

        if ptr == core::ptr::null_mut() {
            return core::ptr::null_mut();
        }

        unsafe { core::ptr::write_bytes(ptr, 0, size) };
        ptr
    }

    /**
     * Reallocate memory into the kernel heap.
     */
    pub fn reallocate(&mut self, ptr: *mut u8, size: usize, gfp: u32) -> *mut u8 {
        debug_assert!(ptr != core::ptr::null_mut());
        debug_assert!(size > 0);

        let header_addr =
            (ptr as usize - core::mem::size_of::<KmallocHeader>()) as *mut KmallocHeader;

        let header = unsafe { &mut *header_addr };

        debug_assert!(header.magic == ['U' as u8, 'S' as u8, 'E' as u8, 'D' as u8]);

        let data_size = header.blocks as usize * PML1_PAGESIZE as usize;

        if data_size >= size {
            return ptr;
        }

        let new_ptr = self.allocate(size, gfp);

        if new_ptr == core::ptr::null_mut() {
            return core::ptr::null_mut();
        }

        unsafe {
            core::ptr::copy_nonoverlapping(ptr, new_ptr, header.size);
            self.free(ptr);
        }

        new_ptr
    }

    /**
     * Free memory from the kernel heap.
     */
    fn free(&mut self, ptr: *mut u8) {
        debug_assert!(ptr != core::ptr::null_mut());

        let header_addr =
            (ptr as usize - core::mem::size_of::<KmallocHeader>()) as *mut KmallocHeader;

        let header = unsafe { &mut *header_addr };

        debug_assert!(header.magic == ['U' as u8, 'S' as u8, 'E' as u8, 'D' as u8]);

        let data_blocks = header.blocks as usize;

        #[cfg(debug_assertions)]
        {
            header.magic[0] = 'F' as u8;
            header.magic[1] = 'R' as u8;
            header.magic[2] = 'E' as u8;
            header.magic[3] = 'E' as u8;
        }

        if data_blocks == 1 {
            unsafe { pmm_free_block(arch_vmm_v2p(header_addr as usize, ARCH_VMM_AREA_HEAP as i32)) };
        } else {
            unsafe {
                pmm_free_blocks(
                    arch_vmm_v2p(header_addr as usize, ARCH_VMM_AREA_HEAP as i32),
                    data_blocks,
                )
            };
        }

        self.heap_used_blocks -= data_blocks;
    }
}

lazy_static! {
    /**
     * Kernel heap instance.
     */
    pub static ref KERNEL_HEAP: Mutex<KernelHeap> = Mutex::new(KernelHeap::new());
}

/**
 * (C wrapper) Allocate memory.
 */
#[no_mangle]
pub fn kmalloc(size: usize, gfp: u32) -> *mut u8 {
    KERNEL_HEAP.lock_uninterruptible().allocate(size, gfp)
}

/**
 * (C wrapper) Allocate zeroed memory.
 */
#[no_mangle]
pub fn kcalloc(nmemb: usize, size: usize, gfp: u32) -> *mut u8 {
    KERNEL_HEAP
        .lock_uninterruptible()
        .allocate_zeroed(nmemb * size, gfp)
}

/**
 * (C wrapper) Reallocate memory.
 */
#[no_mangle]
pub fn krealloc(ptr: *mut u8, size: usize, gfp: u32) -> *mut u8 {
    KERNEL_HEAP
        .lock_uninterruptible()
        .reallocate(ptr, size, gfp)
}

/**
 * (C wrapper) Free memory.
 */
#[no_mangle]
pub fn kfree(ptr: *mut u8) {
    KERNEL_HEAP.lock_uninterruptible().free(ptr)
}

/**
 * (C wrapper) Get used memory.
 */
#[no_mangle]
pub fn kheap_get_used_memory() -> usize {
    KERNEL_HEAP.lock_uninterruptible().heap_used_blocks * PML1_PAGESIZE as usize
}
