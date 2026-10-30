# Screenshots

A timeline of aplus as it looked along the way, oldest first. Every screenshot was taken in a QEMU virtual machine, except for the last photo, taken on a real laptop; the newest screenshots were taken at 1920x1080 with `virtio-vga` and 4 CPUs.

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

### Late October 2026: Linux inside aplus

[Bochs](https://bochs.sourceforge.io/) 3.1 was ported with a native frontend on libui and ships as a regular package. It emulates a 32-bit PC in an aplus window, fast enough to boot Tiny Core Linux to its desktop.

<p align="center" width="100%">
    <img src="./v0.7-bochs-and-linux.png" alt="aplus v0.7 - Bochs booting Tiny Core Linux in a window" width="100%"></img>
</p>

* **Bochs**: Tiny Core Linux running in the emulator, where `uname -a` reports Linux on `i686`; the console below reports aplus on `x86_64`, and the one on the left shows the Bochs log

### Late October 2026: context menus and frosted glass

aplus-wm learned to draw context menus, both its own and on behalf of clients: an application asks for one with `UI_REQ_POPUP_MENU` and the window manager draws it in the window palette over a blurred backdrop, then drives it by mouse or keyboard. Translucent windows can ask for the same frosted backdrop with `UI_WINDOW_BLUR`. The three shots below also show two of the per-wallpaper themes generated by [gen-themes](/scripts/gen-themes), *crimson* and *dusk*.

<p align="center" width="100%">
    <img src="./v0.7-menu-crimson.png" alt="aplus v0.7 - the explorer context menu in the crimson theme" width="100%"></img>
</p>

* **Client menus**: a right press on a row in the explorer opens Open, Open in New Window, Open in Console, Refresh and Delete; Delete has to be pressed twice and turns into *Delete permanently?* in between. The wallpaper is frosted behind the menu

<p align="center" width="100%">
    <img src="./v0.7-taskbar-dusk.png" alt="aplus v0.7 - the taskbar context menu in the dusk theme" width="100%"></img>
</p>

* **Taskbar menu**: right-clicking a taskbar entry slides a menu out of the rail with New window, Pin to taskbar or Unpin from taskbar, and Quit. A pinned application keeps its entry after its windows close, and the pins persist in `/etc/ui/pinned.conf`

<p align="center" width="100%">
    <img src="./v0.7-launcher-blur.png" alt="aplus v0.7 - the frosted launcher in the dusk theme" width="100%"></img>
</p>

* **Frosted launcher**: Super+Space opens the centred launcher as a `UI_WINDOW_BLUR` window, so the windows and the wallpaper behind it show through blurred

### Late October 2026: on real hardware

aplus running on a real machine, an old Huawei laptop with an 11th generation Intel Core i3. The desktop came up and the keyboard worked; the touchpad and the network card have no driver yet, so there is no mouse and no network.

<p align="center" width="100%">
    <img src="./v0.7-real-machine-test.jpg" alt="aplus v0.7 - the desktop on a Huawei laptop with an Intel Core i3-1115G4" width="100%"></img>
</p>

* **Hardware**: `/proc/cpuinfo` lists the four threads of the Intel Core i3-1115G4 at 3.00 GHz, and `free -h` reports about 8 GB of memory with some 830 MB in use
