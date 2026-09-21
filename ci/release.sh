#!/bin/sh
set -e

case "$1" in
    "x86_64")
        TARGET=x86_64
        ;;
    *)
        echo "Unknown target: $1"
        exit 1
        ;;
esac

sudo apt update
sudo apt install -y \
    git \
    build-essential \
    automake \
    autoconf \
    dosfstools \
    e2fsprogs \
    mtools \
    gdisk \
    grub-common \
    fontconfig \
    gzip \
    tar \
    zip

./configure --kconfig ${TARGET}
./makew -j$(nproc)
./makew install


NAME=release-aplus-$(git rev-parse --short HEAD)-${TARGET}

mv aplus.img ${NAME}.img

tar czf ${NAME}.tar.gz ${NAME}.img
tar cJf ${NAME}.tar.xz ${NAME}.img
zip -r ${NAME}.zip ${NAME}.img