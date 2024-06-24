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

use core::alloc::{GlobalAlloc, Layout};

extern "C" {
    fn kmalloc(size: usize, gfp: u32) -> *mut u8;
    fn kcalloc(nmemb: usize, size: usize, gfp: u32) -> *mut u8;
    fn kfree(ptr: *mut u8);
}

const GFP_KERNEL: u32 = 0;
// const GFP_ATOMIC: u32 = 1;
// const GFP_USER: u32 = 2;

pub struct Allocator;

unsafe impl GlobalAlloc for Allocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        kmalloc(layout.size(), GFP_KERNEL)
    }

    unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 {
        kcalloc(1, layout.size(), GFP_KERNEL)
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        kfree(ptr);
    }
}

#[global_allocator]
static ALLOCATOR: Allocator = Allocator;
