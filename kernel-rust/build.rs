use regex::Regex;
use std::env;
use std::path::PathBuf;
use walkdir::WalkDir;

fn main() {
    let mut bindings = bindgen::Builder::default()
        .header("include/wrapper.h")
        .clang_arg("-I../include")
        .clang_arg("-I../sdk/toolchain/x86_64-aplus/include")
        .clang_arg("-I../sdk/toolchain/lib/gcc/x86_64-aplus/12.2.0/include")
        .clang_arg("-include../config.h")
        .clang_arg("-nostdinc")
        .clang_arg("-nostdlib")
        .clang_arg("-DKERNEL=1")
        .clang_arg("-D_GNU_SOURCE=1")
        .clang_arg("-std=c23");

    let srcdir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let srcdir = srcdir.join("src");

    let bindings_regex = Regex::new(r"//\s+#\[import_c_(\w+)\((.+)\)\]").unwrap();

    for entry in WalkDir::new(srcdir) {
        let entry = entry.unwrap();
        let path = entry.path();
        let extension = path.extension().unwrap_or(std::ffi::OsStr::new(""));

        if extension != "rs" {
            continue;
        }

        if !path.is_file() {
            continue;
        }

        let content = std::fs::read_to_string(&path).unwrap();

        for line in content.lines() {
            if bindings_regex.is_match(line) {
                let captures = bindings_regex.captures(line).unwrap();
                let directive = captures.get(1).unwrap().as_str();
                let argument = captures.get(2).unwrap().as_str();

                match directive {
                    "fn" => {
                        bindings = bindings.allowlist_function(argument);
                    }
                    "type" => {
                        bindings = bindings.allowlist_type(argument);
                    }
                    "var" => {
                        bindings = bindings.allowlist_var(argument);
                    }
                    _ => {
                        panic!("Unknown directive: {}", directive);
                    }
                }
            }
        }
    }

    let bindings = bindings
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .use_core()
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());

    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");

    println!("cargo:rerun-if-changed=../include");
    println!("cargo:rerun-if-changed=../sdk/toolchain/x86_64-aplus/include");
    println!("cargo:rerun-if-changed=../config.h");
}
