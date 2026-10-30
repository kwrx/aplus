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
    python3-venv \
    qemu-utils


if [ -d /tmp/get-pkg ]; then
    rm -rf /tmp/get-pkg
fi

echo "Generating minimal image for ${TARGET}..."
./configure --kconfig ${TARGET} --only-required-packages
./makew PROJECTS="kernel drivers lib apps" -j$(nproc) 
./makew PROJECTS="kernel drivers lib apps" install
qemu-img convert -p -f raw -O qcow2 aplus.img aplus-minmal.qcow2

echo "Generating full image for ${TARGET}..."
./configure --kconfig ${TARGET}
./makew PROJECTS="kernel drivers lib apps" -j$(nproc)
./makew PROJECTS="kernel drivers lib apps" install
qemu-img convert -p -f raw -O qcow2 aplus.img aplus.qcow2

echo "Generating live image for ${TARGET}..."
./configure --kconfig ${TARGET}
./makew PROJECTS="kernel drivers lib apps" -j$(nproc)
./makew PROJECTS="kernel drivers lib apps" install-live

NAME=release-aplus-$(git rev-parse --short HEAD)-${TARGET}

mkdir -p ${NAME}
mkdir -p ${NAME}-minimal
mkdir -p ${NAME}-live

mv aplus-minmal.qcow2 ${NAME}-minimal/aplus.qcow2
mv aplus.qcow2 ${NAME}/aplus.qcow2
mv aplus.img ${NAME}-live/aplus.img

install -m 755 ci/run.sh ${NAME}/run.sh
install -m 755 ci/run.sh ${NAME}-minimal/run.sh

tar cJf ${NAME}.tar.xz ${NAME}
tar cJf ${NAME}-minimal.tar.xz ${NAME}-minimal
tar cJf ${NAME}-live.tar.xz ${NAME}-live

zip -r ${NAME}.zip ${NAME}
zip -r ${NAME}-minimal.zip ${NAME}-minimal
zip -r ${NAME}-live.zip ${NAME}-live

rm -rf /tmp/get-pkg