#!/bin/sh

export PATH=/bin:/sbin:/usr/bin:/usr/sbin
export HOME=/home/root

cd $HOME

mkdir -p /var/log/init.d
for i in /etc/init.d/*; do
    if [ -x "$i" ]; then
        "$i" start 2>&1 | tee /var/log/init.d/${i##*/}.log > /dev/kmsg &
    fi
done

sleep infinity
