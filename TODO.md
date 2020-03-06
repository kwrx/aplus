
# Development Roadmap

## Architecture

| Feature       | x86_64 | aarch64 |     |
|---------------|--------|---------|-----|
| Startup       | yes    | no      |     |
| Debug         | yes    | no      |     |
| Multiboot     | yes[^1]| no      |     |
| CPU           | yes    | no      |     |
| Paging        | yes    | no      |     |
| Interrupts    | yes    | no      |     |
| APIC          | yes    |         |     |
| IO APIC       | yes    |         |     |
| Tasking       | yes    | no      |     |
| Fork          | no     | no      |     |
| Timer         | yes    | no      |     |
| ACPI          | yes    | no      |     |
| Multiprocessing| yes    | no      |     |
| Syscalls      | yes[^2]| no      |     |
| Reboot        | yes    | no      |     |
| Power off     | no     | no      |     |
| CPU           | yes    | no      |     |
| CPU           | yes    | no      |     |

[^1]: Multiboot 2.0 Specification
[^2]: Interrupt and SYSCALL/SYSRET


------------------------------------------
## Kernel

- [x] Physical Memory Manager
- [x] Kernel Heap
- [x] ELF Debugging
- [x] Scheduler
- [ ] I/O Scheduler
- [x] Symmetric Multiprocessing
- [x] Virtual File System
- [x] Network Stack (lwIP)
- [ ] User API
- [ ] Module Loading
- [ ] ELF Loading
- [ ] Fstab

------------------------------------------
## Filesystems
- [x] RootFS
- [x] BindFS
- [x] TmpFS
- [x] DevFS
- [ ] ProcFS
- [ ] Iso9660 (CDFS)
- [x] Ext2 (Read-only)
- [ ] FAT32

------------------------------------------
## Drivers
- [x] Device Interface
- [x] Char Devices
- [ ] PTY Devices
- [ ] Framebuffer Video

### VirtIO
- [ ] Block
- [ ] Network

### Platform (PC)
- [x] PCI Support
- [x] AHCI Controller (SATA)
- [x] IDE Controller (ATA)
- [x] IDE Controller (ATA)
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
