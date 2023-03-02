use core::alloc::{GlobalAlloc, Layout};

extern "C" {
    fn kcalloc(n: usize, size: usize, flags: i32) -> *mut u8;
    fn kfree(ptr: *const u8);
}


#[allow(dead_code)]
pub const GFP_KERNEL: i32 = 0x0000;

#[allow(dead_code)]
pub const GFP_ATOMIC: i32 = 0x0001;

#[allow(dead_code)]
pub const GFP_USER: i32 = 0x0002;



pub struct AplusAllocator;

unsafe impl GlobalAlloc for AplusAllocator {
    
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        match kcalloc(1, layout.size(), GFP_KERNEL) as *mut u8 {
            ptr if ptr.is_null() => panic!("out of memory"),
            ptr => ptr,
        }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        kfree(ptr as *const u8);
    }

}


#[alloc_error_handler]
fn oom(_layout: Layout) -> ! {
    panic!("out of memory");
}