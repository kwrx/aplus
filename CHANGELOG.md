## 0.7.0 (2024-08-02)

### Feat

- Refactor reboot code to support crash mode
- Update atomic operations to use stdatomic.h
- **kernel-rust**: add ipc, mm, runtime, and bindings modules imports
- **kernel-rust**: rewrite kernel heap in rust
- **kernel-rust**: add lock_interruptible on mutexes
- **kernel-rust**: add build.rs script for generating C bindings on Rust
- add allocator module to kernel-rust runtime
- Update atomic operations to use stdatomic.h
- **kernel-rust**: add ipc, mm, runtime, and bindings modules imports
- **kernel-rust**: rewrite kernel heap in rust
- **kernel-rust**: add lock_interruptible on mutexes
- **kernel-rust**: add build.rs script for generating C bindings on Rust
- add allocator module to kernel-rust runtime
- add kernel-rust runtime module for logging

### Fix

- update clang-format hook to use clang-format-18
- clear contiguous_memory_area on initialization and fix data types
- Improve spinlock implementation in semaphore.c
- Improve spinlock implementation in semaphore.c
- increase size of sections array in aplus.h
- fixed process info display in runtime_dump() function
- copy grub modules during image generation
- **virtio**: fixed define declaration for VIRTIO_F_EVENT_IDX feature
- **virtio**: fixed virtio wrong negotiation and bad isr_status read access
- **pci**: fixed pci routines
- **pci**: fixed pci on 64bit read/write
- **x86**: fixed integer overflow when monotonic timer reaches 243s

### Refactor

- Remove unused scripts
- add format git hook
- run global formatting
- Disable access modifier indentation in .clang-format
- improved readability by removing useless macro
- add 'core' group to apps in Makefile
- moved init app into core group
- removed raw rustc build
- remove kmsg2 module from the repository
- format source files
- update Makefile to include kernel-rust in the build process
- update readme
- removed extra directory
- rename functions and variables in get-pkg.py for clarity
- update qemu scripts
- update Makefile to disable parallel execution during installation
- update pointer declarations to align to the left
- update .clang-format to align pointer declarations to the left
- reformat source files
- moved format into build-format.mk
- update .clang-format to align escaped newlines to the left
- update .clang-format to indent preprocessor directives before hash symbol
- disable short functions on a single line in .clang-format
- add .clang_format
- removed __user qualifier from all sources
- moved directories
