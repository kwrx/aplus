#!/bin/sh
set -e

export DEBIAN_FRONTEND=noninteractive
export TZ=Etc/UTC

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
    grub-efi-amd64-bin \
    fontconfig \
    gzip \
    tar \
    zip \
    python3 \
    python3-pip \
    python3-venv

./configure --kconfig ${TARGET}
./makew -j$(nproc)
./makew install


NAME=release-aplus-$(git rev-parse --short HEAD)-${TARGET}

mkdir -p ${NAME}
mv aplus.img ${NAME}/${NAME}.img
install -m 755 ci/run.sh ${NAME}/run.sh

tar czf ${NAME}.tar.gz ${NAME}
tar cJf ${NAME}.tar.xz ${NAME}
zip -r ${NAME}.zip ${NAME}