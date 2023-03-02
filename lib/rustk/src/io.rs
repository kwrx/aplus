use core::fmt::{Error, Write};

extern "C" {
    fn kprintf(fmt: *const u8, ...) -> i32;
}


pub struct KernelLogger;

impl KernelLogger {
    pub const fn new() -> Self {
        Self
    }
}

impl Write for KernelLogger {
    fn write_str(&mut self, s: &str) -> Result<(), Error> {
        unsafe {
            kprintf(s.as_ptr());
        }
        Ok(())
    }
}

