# Screenshots

A timeline of aplus as it looked along the way, oldest first. Every screenshot was taken in a QEMU virtual machine; the newest ones were taken at 1920x1080 with KVM, `virtio-vga` and 4 CPUs.

<br>

## v0.4 — the text console

aplus was a 32-bit (`i386`) kernel with a text console. `init` mounted `/proc` and `/tmp`, started the system daemons and handed over to a login prompt.

<p align="center" width="100%">
    <img src="./v0.4-cli.png" alt="aplus v0.4 - init and the login shell" width="49%"></img>
    <img src="./v0.4-taskman.png" alt="aplus v0.4 - taskman" width="49%"></img>
</p>

* **Boot**: `init` mounts `procfs` and `tmpfs`, then starts `ntpd`, `syncd` and `login`
* **Shell**: a login session as a regular user, `uname` reporting `aplus kwrx 0.4 debug i386`
* **taskman**: a text-mode task manager with per-process CPU, I/O and memory usage

<p align="center" width="100%">
    <img src="./v0.4-nyancat.png" alt="aplus v0.4 - nyancat" width="100%"></img>
</p>

`nyancat` running full screen.

<p align="center" width="100%">
    <img src="./v0.4-os.png" alt="aplus v0.4 - three QEMU windows" width="100%"></img>
</p>

All three side by side, each in its own virtual machine.

<br>

## v0.7 "zeta" — x86_64 and the framebuffer

### September 2026: framebuffer console and networking

aplus moved to `x86_64` with SMP, a Linux-like syscall layer and a linear framebuffer. Ported programs started to run unchanged: a text editor, MesaGL demos rendered with `softpipe`, and `nyancat`.

<p align="center" width="100%">
    <img src="./v0.7-os.png" alt="aplus v0.7 - editor, gl-gears, shell and nyancat on the framebuffer" width="100%"></img>
</p>

With lwIP wired up to the socket syscalls and a virtio-net driver, BusyBox `httpd` serves a page from the guest to a browser on the host, through port `8080` forwarded by [run-qemu](/scripts/run-qemu).

<p align="center" width="100%">
    <img src="./v0.7-httpd.png" alt="aplus v0.7 - BusyBox httpd serving a page to a browser on the host" width="100%"></img>
</p>

### September 2026: the first windows

[aplus-wm](/apps/sysutils/aplus-wm) took over the framebuffer and the input devices and started handing out windows over a Unix socket. `gl-gears` became the first OpenGL program to run in a window of its own.

<p align="center" width="100%">
    <img src="./v0.7-gears.png" alt="aplus v0.7 - gl-gears running in a window" width="70%"></img>
</p>

### September – October 2026: the desktop

[libui](/lib/aplus/ui) grew a themed widget layer, and the desktop gained a taskbar, a launcher and the first applications: a terminal, a file explorer, a text editor, an image viewer and a calculator. The launcher can also ask an AI model, and NetSurf browses the web over HTTPS.

<p align="center" width="100%">
    <img src="./v0.7-ui.png" alt="aplus v0.7 - desktop running on QEMU" width="100%"></img>
</p>

Doom and a NES emulator joined as regular packages, along with a PDF viewer built on MuPDF; `top` reads every process from `/proc`.

<p align="center" width="100%">
    <img src="./v0.7-ui-2.png" alt="aplus v0.7 - Doom, the NES emulator and the PDF viewer" width="100%"></img>
</p>

### Late October 2026: themes, settings and console mode

Themes are files read by the window manager and switched at runtime: `aplus-wmctl set-theme`, or the new [Settings](/apps/sysutils/aplus-settings) application, restyles every open window without restarting it, together with its fonts and wallpaper.

<p align="center" width="100%">
    <img src="./v0.7-desktop-dark.png" alt="aplus v0.7 - a busy desktop in the dark theme" width="100%"></img>
</p>

<p align="center" width="100%">
    <img src="./v0.7-desktop-light.png" alt="aplus v0.7 - the same desktop in the light theme" width="100%"></img>
</p>

* **Desktop**: the text editor on a kernel header, the explorer, `top`, NetSurf on Wikipedia over HTTPS, an image viewer, `gl-gears` and the calculator
* **Themes**: the dark and the light theme, each with its own generated wallpaper; the WM, libui and the applications all follow the switch

<p align="center" width="100%">
    <img src="./v0.7-apps.png" alt="aplus v0.7 - Doom, a PDF, Python, the shaders demo and Settings" width="100%"></img>
</p>

* **Applications**: Doom, a paper downloaded over HTTPS and opened in the PDF viewer, an interactive Python 3.15 session, the MesaGL shaders demo and the Appearance page of Settings

<p align="center" width="100%">
    <img src="./v0.7-launcher.png" alt="aplus v0.7 - the launcher" width="100%"></img>
</p>

* **Launcher**: slides out of the taskbar as a drawer and filters the installed applications as you type; a query starting with `>` runs a command

<p align="center" width="100%">
    <img src="./v0.7-console.png" alt="aplus v0.7 - console mode" width="100%"></img>
</p>

* **Console mode**: the *aplus - Console Mode* boot entry skips the desktop and runs the terminal full screen on the framebuffer, with `bash`, Python and a native `gcc`
