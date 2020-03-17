
# Development Roadmap

## Architecture

| Feature       | x86_64 | i686    | aarch64 |
|---------------|--------|---------|---------|
| Startup       | yes    | no      | no      |
| Debug         | yes    | yes     | no      |
| Multiboot     | yes[^1]| yes[^1] | no      |
| CPU           | yes    | yes     | no      |
| Paging        | yes    | yes     | no      |
| Interrupts    | yes    | no      | no      |
| APIC          | yes    | yes     |         |
| IO APIC       | yes    | yes     |         |
| Tasking       | yes    | no      | no      |
| Fork          | no     | no      | no      |
| Timer         | yes    | yes     | no      |
| Random Gen    | no[^2] | no[^2]  | no      |
| ACPI          | yes    | yes     | no      |
| Multiprocessing| yes   | no      | no      |
| Syscalls      | yes[^3]| no      | no      |
| Reboot        | yes    | yes     | no      |
| Power off     | no     | no      | no      |

[^1]: Multiboot 2.0 Specification<br>
[^2]: RDRAND support<br>
[^3]: Interrupt and SYSCALL/SYSRET<br>


------------------------------------------
## Kernel

- [x] Physical Memory Manager
- [x] Kernel Heap
- [x] ELF Debugging
- [x] Scheduler
- [ ] I/O Scheduler
- [x] Spinlocks
- [x] Semaphores
- [ ] Unix Pipe
- [x] Symmetric Multiprocessing
- [x] Virtual File System
- [x] Network Stack (lwIP)
- [X] User API
- [ ] Wait Queue
- [ ] Signal Handling
- [x] Module Loading
- [ ] ELF Loading
- [ ] Fstab
- [ ] Unix Permissions

------------------------------------------
## Filesystems
- [x] RootFS
- [x] BindFS
- [x] TmpFS
- [x] DevFS
- [ ] ProcFS
- [ ] Iso9660 (CDFS)
- [x] Ext2 (Read-only)
- [ ] exFAT

------------------------------------------
## Drivers
- [x] Device Interface
- [x] Char Devices
- [x] Block Devices
- [x] Network Devices
- [ ] PTY Devices
- [ ] Framebuffer Video

### VirtIO
- [ ] Block
- [ ] Network

### Platform (PC)
- [x] PCI Support
- [x] AHCI Controller (SATA)
- [x] IDE Controller (PATA)
- [x] Bochs VGA Controller
- [ ] VMWare VGA Controller
- [ ] AC97 Audio Adepter
- [ ] Intel HD Audio Adepter
- [x] Serial Port
- [x] PS/2 Keyboard/Mouse
- [ ] USB Support
- [x] Intel E1000 Network Adepter
- [x] PCNET Network Adepter
- [x] Realter RTL8139 Network Adepter

------------------------------------------
## Userspace
- [ ] Init
- [ ] Coreutils
- [ ] Mininmal Shell
- [ ] NTP Time Daemon
- [ ] I/O Sync Daemon
- [ ] HTTP Server
- [ ] Graphics UI Server
- [ ] Graphics UI Toolkit
- [ ] Package Manager

### Porting
- [ ] Bash
- [ ] Java Virtual Machine (avm)
- [ ] Python
- [ ] Lua
- [ ] LibZ
- [ ] LibPNG
- [ ] LibJPEG
- [ ] LibWEBP
- [ ] LibFreetype2
- [ ] NCurses
- [ ] MesaGL
- [ ] Pixman
- [ ] Cairo
- [ ] SDL2
- [ ] Binutils
- [ ] Gcc
