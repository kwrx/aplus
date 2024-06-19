#![no_std]

extern crate rustk;

use rustk::{println};
use rustk::io;

#[no_mangle]
#[link_section = ".module_name"]
pub static __module__name: [u8; 6] = *b"kmsg2\0";

#[no_mangle]
#[link_section = ".module_deps"]
pub static __module__deps: [u8; 1] = *b"\0";

#[no_mangle]
extern "C" fn init() {
    println!("Hello, world!");
}

#[no_mangle]
extern "C" fn dnit() {
    println!("Goodbye, world!");
}