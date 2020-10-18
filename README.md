
# aPlus
A hobby operating system built mostly from scratch with a unix-like, hybrid and cross-platform kernel.
The project started in September 2013 as an educational and personal project, it's written mainly in C/C++ and Assembly.

<p align="center">
    <img src="./extra/images/v0.4-os.png" alt="aPlus v0.4 - CLI interface running on Qemu"></img>
</p>




## Features

* **Cross-platform**: [arch/*](/arch), designed for cross-platform environment targets
* **Multitasking**: Thread and Process support with SMP
* **Network**: [kernel/network/](/kernel/network), Almost full TCP/IP Network Stack by [lwIP](https://savannah.nongnu.org/projects/lwip/)
* **Unix-like**: VFS, Signals, Pipes, IPC, Shared Memory, Unix Sockets
* **ELF**: Dynamic and static executables 
* **Linux Framebuffer**: Linux-like framebuffer support
* **Linux Syscalls**: Linux-like syscall layer
* **GUI**: coming soon...

## Kernel
The kernel provides a basic Unix/Posix environment.
It uses a hybrid modular architecture with support for various platforms like x86, x86_64, ARM, etc... and loadable modules.


## Userspace
aPlus's userspace is still under development, it provides several GNU/Linux core tools, development tools like gcc or binutils, a very simple Java Virtual Machine, GUI, Windows Manager, services like NTP, I/O Cache Sync, other tools, etc...

Userspace has **multi-user** implementation with Unix permission support and superuser (root), unix-like filesystem with `/proc` and `/dev` supports

### Notable applications/libraries
* **C/C++**, [libs/c](sdk/libs/c), [libs/m](sdk/libs/m), [libs/crt](sdk/libs/crt) standard c11, math and runtime libraries forked from newlib; C++ support provided by gcc
* **Startup**, [apps/system/init](usr/apps/system/init/main.c), where it all begins
* **Java VM**, [apps/extra/jvm](https://www.github.com/kwrx/aplus-jvm), very simple java virtual machine 

## Drivers
Modules provides various core platform features, caching, char/block devices, filesystems, I/O devices, system low-level services, network, audio/video and virtio support.

### Notable modules
* **Device Interface**, [dev/*](/drivers/dev), provides a standard interface for drivers
* **AHCI** (Advanced Host Controller Interface), [platform/pc/block/ahci](/drivers/platform/pc/block/ahci/main.c), almost full SATA/SATAPI driver
* **BGA** (Bochs Video Card), [platform/pc/video/bga](/drivers/platform/pc/video/bga/main.c), Bochs Virtual VGA Adapter
* **Intel e1000** (Network device), [platform/pc/network/e1000](/drivers/platform/pc/network/e1000/main.c), Intel NIC driver

---

## Building / Installation
Clone the repository and change working directory.
```bash
$ git clone https://github.com/kwrx/aplus
$ cd aplus
```

To build this project from source, you have basically two options.

### Building with Docker (Recommended):
You can use my pre-built toolchain through Docker:

```console
# docker pull alpine
# docker run --privileged=true -v $(pwd):/opt/aplus -w /opt/aplus -t alpine \
    ./extra/utils/build-with-docker TARGET
```

And run with:
```
$ ./extra/utils/run-qemu TARGET
```

Replace `TARGET` with your desired target output: `i686`, `x86_64`, ecc...

### Building with Toolchain:
It's recommended you use a recent Linux host environment with this method.

Some packages are required for the build system:
* `gcc`, `binutils`, `make`, `autotools` (or `build-essential` on Ubuntu/Debian)
* `g++` to compile gcc sources
* `nasm` to compile .asm sources
* `cmake`, `ninja` to compile project
* `nodejs`, `npm` to run some build scripts
* `e2fsprogs`, `grub` to generate hdd image
* `qemu` or `VirtualBox` to run Virtual Machine

```bash
# Install dependencies
$ pip3 install -r docs/requirements.txt

# Build Toolchain
$ cd sdk
$ ../extra/utils/build-toolchain TARGET

# Install CRT Files
$ cd ..
$ ./build -t TARGET core

# Install C++ Support (Optional)
$ cd sdk
$ ../extra/utils/build-toolchain TARGET

# Build and run Project
$ cd ..
$ ./build --target TARGET
```
Replace `TARGET` with your desired target output: `i686`, `x86_64`, ecc...

---

## Third-Party Software:
aPlus uses and depends on a large number of third-party open-source tools and libraries which are outside of this repository.

Licenses for the included third-party tools and libraries can be found [here](/extra/licenses), and for project [here](/LICENSE)

