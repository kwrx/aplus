
# Development Roadmap

- [ ] Use Tree data structure in vfs_cache
- [x] Implement Sleep queue
- [x] Implement Wait queue


------------------------------------------


## Architecture

| Features      | x86_64 | i686    | aarch64 |
|---------------|--------|---------|---------|
| Startup       | yes    | no      | no      |
| Debug         | yes    | yes     | no      |
| Multiboot     | yes[^1]| yes[^1] | no      |
| CPU           | yes    | yes     | no      |
| Paging        | yes[^2]| yes[^2] | no      |
| Interrupts    | yes    | no      | no      |
| Tasking       | yes    | no      | no      |
| Timer         | yes    | yes     | no      |
| Fork          | yes    | no      | no      |
| Random        | no[^3] | no[^3]  | no      |
| ACPI          | yes    | yes     | no      |
| Multiprocessing| yes   | no      | no      |
| Syscalls      | yes[^4]| no      | no      |
| Reboot        | yes    | yes     | no      |
| Power off     | no     | no      | no      |
| APIC          | yes    | yes     |         |
| I/O APIC      | yes    | yes     |         |
| HPET          | yes    | yes     |         |
| APIC TIMER    | yes    | yes     |         |
| TSC           | yes    | yes     |         |
| SYSCALL/SYSRET| yes    | no      |         |
| RDRAND        | no     | no      |         |

[^1]: Multiboot 2.0 Specification<br>
[^2]: On-demand paging provided<br>
[^3]: RDRAND support<br>
[^4]: Interrupt and SYSCALL/SYSRET<br>


------------------------------------------


## Kernel

- [x] Physical Memory Manager
- [x] Kernel Heap
- [x] ELF Debugging
- [x] Scheduler
- [ ] I/O Scheduler
- [x] Spinlocks
- [x] Semaphores
- [x] Unix Pipe
- [x] System V Shared Memory (shmget, shmat, shmdt, shmctl)
- [x] Unix Socket (AF_UNIX, SOCK_STREAM)
- [x] BSD Sockets (AF_INET, SOCK_STREAM), as ordinary file descriptors
- [x] Symmetric Multiprocessing
- [x] Virtual File System
- [x] Network Stack (lwIP)
- [x] User API
- [x] Wait Queue
- [x] Signal Handling
- [x] Module Loading
- [x] ELF Loading
- [ ] Fstab
- [ ] Unix Permissions
- [x] Futex
- [x] I/O Multiplexing (poll, ppoll, select, pselect)

------------------------------------------
## Filesystems
- [x] RootFS
- [x] BindFS
- [x] TmpFS
- [x] DevFS
- [x] ProcFS
- [x] Iso9660 (CDFS)
- [x] Ext2
- [ ] exFAT

------------------------------------------
## Drivers
- [x] Device Interface
- [x] Char Devices
- [x] Block Devices
- [x] Network Devices
- [x] PTY Devices
- [x] Framebuffer Video
- [ ] Full Video Interface
- [x] Virtio PCI
- [x] Virtio Queues
- [x] Virtio GPU
- [ ] Virtio Block
- [ ] Virtio Network
- [ ] Virtio Crypto
- [x] Virtio Console
- [x] Virtio Input
- [x] Virtio Random
- [x] Bochs VGA Controller
- [x] VMWare VGA Controller

### Platform (PC)
- [x] PCI
- [x] PCI INTx
- [ ] PCI MSI
- [ ] PCI MSIX
- [x] AHCI Controller (SATA)
- [x] IDE Controller (PATA)
- [ ] AC97 Audio Adapter
- [ ] Intel HD Audio Adapter
- [x] Serial Port UARTx
- [x] PS/2 Keyboard/Mouse
- [ ] USB Support
- [x] Intel E1000 Network Adapter
- [x] PCNET Network Adapter
- [ ] Realtek RTL8139 Network Adapter

------------------------------------------
## Userspace
- [x] Init
- [ ] Coreutils
- [ ] Minimal Shell
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
