## 0.7.0 (2024-08-02)

### Feat

- **generic**: refactor reboot code to support crash mode
- **generic**: update atomic operations to use stdatomic.h
- **kernel-rust**: add ipc, mm, runtime, and bindings modules imports
- **kernel-rust**: rewrite kernel heap in rust
- **kernel-rust**: add lock_interruptible on mutexes
- **kernel-rust**: add build.rs script for generating C bindings on Rust
- **generic**: add allocator module to kernel-rust runtime
- **generic**: update atomic operations to use stdatomic.h
- **kernel-rust**: add ipc, mm, runtime, and bindings modules imports
- **kernel-rust**: rewrite kernel heap in rust
- **kernel-rust**: add lock_interruptible on mutexes
- **kernel-rust**: add build.rs script for generating C bindings on Rust
- **generic**: add allocator module to kernel-rust runtime
- **generic**: add kernel-rust runtime module for logging

### Fix

- **generic**: update clang-format hook to use clang-format-18
- **generic**: clear contiguous_memory_area on initialization and fix data types
- **generic**: improve spinlock implementation in semaphore.c
- **generic**: improve spinlock implementation in semaphore.c
- **generic**: increase size of sections array in aplus.h
- **generic**: fixed process info display in runtime_dump() function
- **generic**: copy grub modules during image generation
- **virtio**: fixed define declaration for VIRTIO_F_EVENT_IDX feature
- **virtio**: fixed virtio wrong negotiation and bad isr_status read access
- **pci**: fixed pci routines
- **pci**: fixed pci on 64bit read/write
- **x86**: fixed integer overflow when monotonic timer reaches 243s

### Refactor

- **generic**: remove unused scripts
- **generic**: add format git hook
- **generic**: run global formatting
- **generic**: disable access modifier indentation in .clang-format
- **generic**: improved readability by removing useless macro
- **generic**: add 'core' group to apps in Makefile
- **generic**: moved init app into core group
- **generic**: removed raw rustc build
- **generic**: remove kmsg2 module from the repository
- **generic**: format source files
- **generic**: update Makefile to include kernel-rust in the build process
- **generic**: update readme
- **generic**: removed extra directory
- **generic**: rename functions and variables in get-pkg.py for clarity
- **generic**: update qemu scripts
- **generic**: update Makefile to disable parallel execution during installation
- **generic**: update pointer declarations to align to the left
- **generic**: update .clang-format to align pointer declarations to the left
- **generic**: reformat source files
- **generic**: moved format into build-format.mk
- **generic**: update .clang-format to align escaped newlines to the left
- **generic**: update .clang-format to indent preprocessor directives before hash symbol
- **generic**: disable short functions on a single line in .clang-format
- **generic**: add .clang_format
- **generic**: removed __user qualifier from all sources
- **generic**: moved directories
