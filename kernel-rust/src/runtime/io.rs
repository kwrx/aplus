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

// #[import_c_fn(arch_debug_putc)]

use crate::bindings::arch_debug_putc;
use crate::ipc::mutex::Mutex;
use core::fmt::{self, Write};
use lazy_static::lazy_static;

pub struct KernelLogger;

impl Write for KernelLogger {
    fn write_str(&mut self, s: &str) -> Result<(), fmt::Error> {
        for c in s.bytes() {
            unsafe {
                arch_debug_putc(c as i8);
            }
        }
        Ok(())
    }
}

lazy_static! {
    pub static ref LOGGER: Mutex<KernelLogger> = Mutex::new(KernelLogger);
}

#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => {
        $crate::runtime::io::_print(format_args!($($arg)*));
    };
}

#[macro_export]
macro_rules! println {
    () => {
        $crate::print!("\n");
    };
    ($($arg:tt)*) => {
        $crate::print!("{}\n", format_args!($($arg)*));
    };
}

#[doc(hidden)]
pub fn _print(args: fmt::Arguments) {
    use fmt::Write;
    LOGGER.lock_uninterruptible().write_fmt(args).unwrap();
}
