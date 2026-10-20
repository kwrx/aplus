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
    fakeroot \
    grub-common \
    grub-efi-amd64-bin \
    fontconfig \
    gzip \
    tar \
    zip \
    xz-utils \
    python3 \
    python3-pip \
    python3-venv


if [ -d /tmp/get-pkg ]; then
    rm -rf /tmp/get-pkg
fi

./configure --kconfig ${TARGET}
./makew PROJECTS="kernel drivers lib apps" -j$(nproc) 
./makew PROJECTS="kernel drivers lib apps" install


NAME=release-aplus-$(git rev-parse --short HEAD)-${TARGET}

mkdir -p ${NAME}
mv aplus.img ${NAME}/${NAME}.img
install -m 755 ci/run.sh ${NAME}/run.sh

tar cJf ${NAME}.tar.xz ${NAME}
zip -r ${NAME}.zip ${NAME}

rm -rf /tmp/get-pkg