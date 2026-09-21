#!/bin/sh

# run.sh: boot an aplus disk image with QEMU, using the settings kept in config.txt.
# The first run asks for the settings, later runs reuse the file.

set -e

CONFIG_FILE=config.txt
RECONFIGURE=0
ASSUME_DEFAULTS=0
DRY_RUN=0

CR=$(printf '\r')
TAB=$(printf '\t')

ARCH=x86_64
IMAGE=
BIOS=auto
CPUS=4
MEMORY=4G
VIRTIO=1
KVM=auto
REMOTE=

# @brief Print the usage message.
# @return Nothing.
usage() {
    cat <<EOF
Usage: run.sh [OPTIONS]

Boot an aplus disk image with QEMU. The settings are read from config.txt;
when that file is missing they are asked for and then written to it.

Options:
  -h, --help           show this help message and exit
  -c, --config FILE    read and write the settings from FILE (default: config.txt)
  -r, --reconfigure    ask for the settings again and rewrite the file
  -y, --defaults       do not ask anything, take the default for every setting
  -n, --dry-run        print the QEMU command line instead of running it

Settings (config.txt):
  ARCH       architecture of the image (x86_64)
  IMAGE      path of the disk image to boot
  BIOS       path of the UEFI firmware, or "auto" to look for it
  CPUS       number of CPUs
  MEMORY     amount of RAM (QEMU syntax, e.g. 4G)
  VIRTIO     1 to use the virtio GPU and network card, 0 for std VGA and pcnet
  KVM        1 to run with KVM, 0 to emulate the CPU, "auto" to use it if present
  REMOTE     VNC display to show the screen on (e.g. 127.0.0.1:0), empty for a window
EOF
}

# @brief Print an error message and exit.
# @param $1 Message to print.
# @return Does not return.
die() {
    printf 'run.sh: %s\n' "$1" >&2
    exit 1
}

# @brief Strip the leading and trailing blanks of a string.
# @param $1 String to trim.
# @return The trimmed string on stdout.
trim() {
    trimmed=$1
    while :; do
        case $trimmed in
        " "* | "$TAB"*) trimmed=${trimmed#?} ;;
        *" " | *"$TAB") trimmed=${trimmed%?} ;;
        *) break ;;
        esac
    done
    printf '%s' "$trimmed"
}

# @brief Translate a yes/no answer into 1 or 0.
# @param $1 Answer to translate.
# @param $2 Name of the setting, used in the error message.
# @return 1 or 0 on stdout.
to_bool() {
    case $1 in
    1 | y | Y | yes | Yes | YES | true | True | on) printf '1' ;;
    0 | n | N | no | No | NO | false | False | off) printf '0' ;;
    *) die "invalid value \"$1\" for $2, expected yes or no" ;;
    esac
}

# @brief Tell whether a firmware image can be given to QEMU with the "-bios" option.
# @param $1 Path of the firmware image.
# @return Zero when the image is usable, non zero otherwise.
is_usable_firmware() {
    if [ ! -f "$1" ]; then
        return 1
    fi
    size=$(wc -c <"$1" | tr -d ' ')
    if [ -z "$size" ] || [ "$size" -le 0 ] || [ "$size" -gt 16777216 ] || [ $((size % 65536)) -ne 0 ]; then
        return 1
    fi
    return 0
}

# @brief Look for a UEFI firmware among the paths used by the common distributions.
# @return The path of the firmware on stdout, empty when none was found.
find_firmware() {
    for candidate in \
        /usr/share/OVMF/x64/OVMF_CODE.fd \
        /usr/share/OVMF/OVMF_CODE.fd \
        /usr/share/ovmf/OVMF_CODE.fd \
        /usr/share/ovmf/x64/OVMF_CODE.fd \
        /usr/share/edk2-ovmf/x64/OVMF_CODE.fd \
        /usr/share/edk2/x64/OVMF_CODE.fd \
        /usr/share/OVMF/OVMF.fd \
        /usr/share/OVMF/x64/OVMF.fd \
        /usr/share/ovmf/OVMF.fd \
        /usr/share/ovmf/x64/OVMF.fd \
        /usr/share/edk2-ovmf/x64/OVMF.fd \
        /usr/share/edk2/x64/OVMF.fd \
        /usr/share/qemu/OVMF.fd \
        /usr/share/OVMF/x64/OVMF.4m.fd \
        /usr/share/ovmf/x64/OVMF.4m.fd \
        /usr/share/edk2-ovmf/x64/OVMF.4m.fd \
        /usr/share/edk2/x64/OVMF.4m.fd; do
        if is_usable_firmware "$candidate"; then
            printf '%s' "$candidate"
            return 0
        fi
    done
    printf ''
}

# @brief Look for a disk image in the current directory.
# @return The path of the image on stdout, "aplus.img" when none was found.
find_image() {
    if [ -f aplus.img ]; then
        printf 'aplus.img'
        return 0
    fi
    for candidate in *.img; do
        if [ -f "$candidate" ]; then
            printf '%s' "$candidate"
            return 0
        fi
    done
    printf 'aplus.img'
}

# @brief Ask a question on the terminal.
# @param $1 Question to ask.
# @param $2 Value taken when the answer is empty.
# @return The answer on stdout.
ask() {
    printf '%s [%s]: ' "$1" "$2" >&2
    if ! read -r answer; then
        answer=
        printf '\n' >&2
    fi
    answer=$(trim "$answer")
    if [ -z "$answer" ]; then
        answer=$2
    fi
    printf '%s' "$answer"
}

# @brief Ask a yes/no question on the terminal.
# @param $1 Question to ask.
# @param $2 Value taken when the answer is empty, 1 or 0.
# @return 1 or 0 on stdout.
ask_bool() {
    if [ "$2" -eq 1 ]; then
        fallback=yes
    else
        fallback=no
    fi
    while :; do
        answer=$(ask "$1 (yes/no)" "$fallback")
        case $answer in
        1 | y | Y | yes | Yes | YES | true | True | on)
            printf '1'
            return 0
            ;;
        0 | n | N | no | No | NO | false | False | off)
            printf '0'
            return 0
            ;;
        *) printf 'run.sh: please answer yes or no\n' >&2 ;;
        esac
    done
}

# @brief Ask for every setting and store the answers in the global variables.
# @return Nothing.
ask_config() {
    printf 'run.sh: no settings found, answer the following questions to create %s\n\n' "$CONFIG_FILE" >&2
    ARCH=$(ask 'Architecture of the image' "$ARCH")
    IMAGE=$(ask 'Disk image to boot' "$IMAGE")
    BIOS=$(ask 'UEFI firmware ("auto" to look for it)' "$BIOS")
    CPUS=$(ask 'Number of CPUs' "$CPUS")
    MEMORY=$(ask 'Amount of RAM' "$MEMORY")
    VIRTIO=$(ask_bool 'Use the virtio GPU and network card' "$VIRTIO")
    KVM=$(ask_bool 'Run with KVM' "$KVM")
    REMOTE=$(ask 'VNC display to show the screen on (empty for a local window)' "$REMOTE")
    printf '\n' >&2
}

# @brief Write the current settings to the configuration file.
# @return Nothing.
write_config() {
    cat >"$CONFIG_FILE" <<EOF
# Settings used by run.sh to boot the aplus image with QEMU.
# Delete this file, or run "run.sh --reconfigure", to be asked again.

# Architecture of the image (x86_64).
ARCH=$ARCH

# Disk image to boot and UEFI firmware ("auto" to look for the firmware).
IMAGE=$IMAGE
BIOS=$BIOS

# Number of CPUs and amount of RAM.
CPUS=$CPUS
MEMORY=$MEMORY

# 1 for the virtio GPU and network card, 0 for std VGA and pcnet.
VIRTIO=$VIRTIO

# 1 to run with KVM, 0 to emulate the CPU, auto to use it when /dev/kvm is there.
KVM=$KVM

# VNC display to show the screen on, e.g. 127.0.0.1:0, empty for a local window.
REMOTE=$REMOTE
EOF
    printf 'run.sh: settings written to %s\n' "$CONFIG_FILE" >&2
}

# @brief Load the configuration file into the global variables.
# @return Nothing.
read_config() {
    while IFS= read -r line || [ -n "$line" ]; do
        line=${line%"$CR"}
        line=$(trim "$line")
        case $line in
        '' | '#'*) continue ;;
        *=*) ;;
        *) die "$CONFIG_FILE: invalid line \"$line\"" ;;
        esac
        key=$(trim "${line%%=*}")
        value=$(trim "${line#*=}")
        case $key in
        ARCH) ARCH=$value ;;
        IMAGE) IMAGE=$value ;;
        BIOS) BIOS=$value ;;
        CPUS) CPUS=$value ;;
        MEMORY) MEMORY=$value ;;
        VIRTIO) VIRTIO=$value ;;
        KVM) KVM=$value ;;
        REMOTE) REMOTE=$value ;;
        *) die "$CONFIG_FILE: unknown setting \"$key\"" ;;
        esac
    done <"$CONFIG_FILE"
}

# @brief Check the settings and resolve the ones left to "auto".
# @return Nothing.
check_config() {
    case $ARCH in
    x86_64) ;;
    *) die "unknown ARCH \"$ARCH\"" ;;
    esac

    case $CPUS in
    '' | *[!0-9]*) die "invalid CPUS \"$CPUS\", expected a number" ;;
    esac

    if [ "$CPUS" -lt 1 ]; then
        die "invalid CPUS \"$CPUS\", expected at least one CPU"
    fi

    case $MEMORY in
    '' | *[!0-9KMGkmg]*) die "invalid MEMORY \"$MEMORY\", expected a size such as 4G" ;;
    esac

    VIRTIO=$(to_bool "$VIRTIO" VIRTIO)

    case $KVM in
    auto | Auto | AUTO)
        if [ -w /dev/kvm ]; then
            KVM=1
        else
            KVM=0
        fi
        ;;
    *) KVM=$(to_bool "$KVM" KVM) ;;
    esac

    if [ "$KVM" -eq 1 ] && [ ! -w /dev/kvm ]; then
        die "KVM is enabled but /dev/kvm is not writable, set KVM=0 in $CONFIG_FILE"
    fi

    if ! command -v "qemu-system-$ARCH" >/dev/null 2>&1; then
        die "missing required command: qemu-system-$ARCH"
    fi

    if [ -z "$IMAGE" ] || [ ! -f "$IMAGE" ]; then
        die "disk image \"$IMAGE\" not found, set IMAGE in $CONFIG_FILE"
    fi

    case $BIOS in
    auto | Auto | AUTO | '') BIOS=$(find_firmware) ;;
    esac

    if [ -z "$BIOS" ]; then
        die "UEFI firmware not found, install it (ovmf or edk2-ovmf) and set BIOS in $CONFIG_FILE"
    fi

    if [ ! -f "$BIOS" ]; then
        die "UEFI firmware \"$BIOS\" not found"
    fi

    if ! is_usable_firmware "$BIOS"; then
        die "UEFI firmware \"$BIOS\" is not usable with -bios, its size must be a multiple of 64K: use a whole OVMF.fd image, not a split OVMF_CODE one"
    fi
}

while [ $# -gt 0 ]; do
    case $1 in
    -h | --help)
        usage
        exit 0
        ;;
    -c | --config)
        [ $# -ge 2 ] || die "missing argument for \"$1\""
        CONFIG_FILE=$2
        shift
        shift
        ;;
    -r | --reconfigure)
        RECONFIGURE=1
        shift
        ;;
    -y | --defaults)
        ASSUME_DEFAULTS=1
        shift
        ;;
    -n | --dry-run)
        DRY_RUN=1
        shift
        ;;
    *)
        die "unknown argument \"$1\""
        ;;
    esac
done

IMAGE=$(find_image)

if [ -w /dev/kvm ]; then
    KVM=1
else
    KVM=0
fi

if [ -f "$CONFIG_FILE" ] && [ $RECONFIGURE -eq 0 ]; then
    read_config
else
    if [ $ASSUME_DEFAULTS -eq 0 ] && [ -t 0 ]; then
        ask_config
    else
        printf 'run.sh: taking the default settings\n' >&2
    fi
    write_config
fi

check_config

set -- -no-reboot -no-shutdown
set -- "$@" --bios "$BIOS" -net none

# GPU
if [ "$VIRTIO" -eq 1 ]; then
    set -- "$@" -device virtio-vga
else
    set -- "$@" -vga std
fi

# Pointer
set -- "$@" -device virtio-tablet-pci,disable-legacy=on

# Network
if [ "$VIRTIO" -eq 1 ]; then
    set -- "$@" -device virtio-net-pci,netdev=net0,disable-legacy=on
else
    set -- "$@" -device pcnet,netdev=net0
fi

set -- "$@" -netdev user,id=net0,hostfwd=tcp::8080-:80

# Block
set -- "$@" -drive "id=disk,file=$IMAGE,if=none,format=raw"
set -- "$@" -device ich9-ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0

# Random
set -- "$@" -object rng-random,filename=/dev/random,id=rng0
set -- "$@" -device virtio-rng-pci,rng=rng0,disable-legacy=on

# Console
set -- "$@" -chardev file,id=vc0,path=console.log
set -- "$@" -device virtio-serial-pci,disable-legacy=on
set -- "$@" -device virtconsole,chardev=vc0

# CPU
if [ "$KVM" -eq 1 ]; then
    set -- "$@" -enable-kvm
    set -- "$@" -cpu host,+x2apic,-pdpe1gb,+rdtscp,+fxsr
else
    set -- "$@" -cpu max,-pdpe1gb,+rdtscp,+fxsr
fi

# SMP
set -- "$@" -smp "$CPUS,sockets=1,cores=$CPUS,threads=1"

# Memory
set -- "$@" -m "$MEMORY"

# Clock
set -- "$@" -rtc clock=host

# Boot
set -- "$@" -boot order=c

# Quick Exit
set -- "$@" -device isa-debug-exit,iobase=0xf4,iosize=0x04

# Display
if [ -z "$REMOTE" ]; then
    set -- "$@" -display gtk
else
    set -- "$@" -display none
    set -- "$@" -vnc "$REMOTE"
fi

# Serial
set -- "$@" -serial stdio

# Monitor
set -- "$@" -monitor telnet::4444,server,nowait

printf '%s' "qemu-system-$ARCH"
for arg in "$@"; do
    printf ' %s' "$arg"
done
printf '\n'

if [ $DRY_RUN -eq 1 ]; then
    exit 0
fi

exec "qemu-system-$ARCH" "$@"
