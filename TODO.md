
# Development Roadmap

- [ ] Use Tree data structure in vfs_cache
- [x] Implement Sleep queue
- [x] Implement Wait queue



// TODO: Update TODO.md
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
| Fork          | no     | no      | no      |
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

## Syscalls

- [ ] sys_poll
- [ ] sys_mmap
- [ ] sys_mprotect
- [ ] sys_munmap
- [ ] sys_rt_sigaction
- [ ] sys_rt_sigprocmask
- [ ] sys_pread64
- [ ] sys_pwrite64
- [ ] sys_readv
- [ ] sys_writev
- [ ] sys_pipe
- [ ] sys_select
- [ ] sys_mremap
- [ ] sys_msync
- [ ] sys_mincore
- [ ] sys_madvice
- [ ] sys_shmget
- [ ] sys_shmat
- [ ] sys_shmctl
- [x] sys_dup
- [x] sys_dup2
- [ ] sys_pause
- [ ] sys_nanosleep
- [ ] sys_getitimer
- [ ] sys_alarm
- [ ] sys_setitimer
- [ ] sys_sendfile64
- [ ] sys_socket
- [ ] sys_connect
- [ ] sys_accept
- [ ] sys_sendto
- [ ] sys_recvfrom
- [ ] sys_sendmsg
- [ ] sys_recvmsg
- [ ] sys_shutdown
- [ ] sys_bind
- [ ] sys_listen
- [ ] sys_getsockname
- [ ] sys_getpeername
- [ ] sys_socketpair
- [ ] sys_setsockopt
- [ ] sys_getsockopt
- [ ] sys_clone
- [ ] sys_fork
- [ ] sys_vfork
- [ ] sys_vfork
- [x] sys_exit
- [ ] sys_wait4
- [ ] sys_kill

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
- [x] Futex

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
