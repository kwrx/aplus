
# aPlus
Un sistema operativo interamente Built From Scratch (scritto da zero) con un kernel unix-like ibrido multipiattaforma.
Il progetto, iniziato a Settembre 2014, è scritto C, C++ e Assembly.

![alt text](/extra/images/v0.4-cli.png "aPlus v0.4 - CLI")


## Features:
* [x] Multitasking
* [ ] SMP
* [x] Multi-User
* [x] TCP/IP Network Stack
* [x] Signals, Pipes, IPC, Shared Memory, Unix Sockets
* [x] Virtual Filesystem
* [ ] GUI 

## Platforms:
* [x] i386
* [ ] x86_64
* [ ] ARM
* [ ] AArch64

---

## Drivers:
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

## Porting:
* [x] gcc-7.2.0
* [x] binutils-2.29
* [x] newlib-3.0.0
* [x] lwip-2.4.0
* [x] libz-1.2.11
* [ ] libjpeg-v6b
* [x] libpng-1.6.34
* [x] libwebp-0.6.0
* [ ] liblzma
* [x] freetype-2.8
* [x] pixman-0.34.0
* [x] cairo-1.14.12
* [ ] SDL2
* [ ] SDL2 Image
* [ ] SDL2 TTF

