#![no_std]
#![feature(allocator_api, alloc_error_handler, panic_info_message)]

extern crate alloc;

use core::panic::PanicInfo;

pub mod allocator;
pub mod io;


extern "C" {
    fn kpanicf(fmt: *const u8, ...) -> !;
}


#[macro_export]
macro_rules! print {
    ($($arg:tt)+) => ({
        use core::fmt::Write;
        let _ = write!(crate::io::KernelLogger::new(), $($arg)+);
    })
}

#[macro_export]
macro_rules! println {
    () => ($crate::print!("\r\n"));
    ($fmt:expr) => ($crate::print!(concat!($fmt, "\r\n")));
    ($fmt:expr, $($arg:tt)*) => ($crate::print!(concat!($fmt, "\r\n"), $($arg)*));
}


#[no_mangle]
extern "C" fn eh_personality() {}

#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    
    print!("panic: ");
    
    if let Some(p) = info.location() {
        println!(
            "file '{}' line {} column {}: {}",
            p.file(),
            p.line(),
            p.column(),
            info.message().unwrap()
        )
    } else {
        println!("no information available.");
    }
    
    unsafe {
        kpanicf(b"RUST PANIC\0".as_ptr());
    }

}

#[global_allocator]
static ALLOCATOR: allocator::AplusAllocator = allocator::AplusAllocator;