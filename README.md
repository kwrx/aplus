
# aPlus
A hobby operating system built mostly from scratch with a unix-like, hybrid and cross-platform kernel.
The project started in September 2013 as an educational and personal project, it's written mainly in C/C++ and Assembly.

<p align="center">
    <img src="/extra/images/v0.4-os.png" alt="aPlus v0.4 - CLI interface running on Qemu"></img>
</p>

## Build Instructions
To compile this project you need aPlus Toolchain which includes GCC, Binutils and System Libraries.
So... I'm sure one day you'll be able to compile it by yourself but this is not the day ...

## Kernel
The kernel provides a basic Unix/Posix environment.
It uses a hybrid modular architecture with support for various platforms like x86, x86_64, ARM, etc... and loadable modules.

* [x] Multitasking
* [ ] SMP
* [x] Multi-User
* [x] TCP/IP Network Stack
* [x] Signals, Pipes, IPC, Shared Memory, Unix Sockets
* [x] Virtual Filesystem
* [x] ELF Binary
* [x] Linux Framebuffer
* [ ] GUI 


## Userspace
aPlus's userspace is still under development, it provides several GNU/Linux core tools, development tools like gcc or binutils, a very basic Java Virtual Machine, GUI, Windows Manager, services like NTP, I/O Cache Sync, other tools, etc...

Userspace has Multi-User implementation with Unix permission support and superuser (root), unix-like filesystem with `/proc` and `/dev` supports


## Drivers
Modules provides various core platform features, basic TTY/Console, char/block devices, filesystems, I/O devices, system low-level services, network, audio/video and virtio support.

#### Storage:
* [x] IDE
* [ ] ACHI
* [x] Virtio-block

#### Network:
* [x] E1000
* [x] PCNET
* [x] Rtl8139
* [x] Virtio-network

#### Video:
* [x] Vesa
* [x] Bochs VGA Adepter
* [ ] VMWare SVGA

#### Audio:
* [ ] AC97
* [ ] Intel HDA

#### I/O:
* [x] PS/2 Keybord/Mouse
* [x] UART
* [ ] USB

#### Filesystems:
* [x] Iso9660
* [x] Ext2 (Read-only)
* [x] Fat (Read-only)
* [x] ProcFS
* [x] TmpFs
* [x] Devfs

---

## Third-Party Software:
aPlus uses and depends on a large number of third-party open-source libraries and applications which are outside of this repository.

* [x] gcc-7.2.0
* [x] binutils-2.29
* [x] newlib-3.0.0
* [x] lwip-2.0.3
* [x] libz-1.2.11
* [x] libjpeg-v9c
* [x] libpng-1.6.34
* [x] libwebp-0.6.0
* [ ] liblzma
* [x] freetype-2.8
* [x] pixman-0.34.0
* [x] cairo-1.14.12
* [x] SDL2-2.0.8
* [ ] SDL2 Image
* [ ] SDL2 TTF

