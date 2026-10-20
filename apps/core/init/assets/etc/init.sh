#!/bin/sh

export PATH=/bin:/sbin:/usr/bin:/usr/sbin
export HOME=/home/root

cd $HOME

mkdir -p /var/log/init.d
for i in /etc/init.d/*; do
    if [ -x "$i" ]; then
        "$i" start > /var/log/init.d/$(basename $i).log 2>&1
    fi
done

sleep infinity
