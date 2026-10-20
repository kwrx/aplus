#!/bin/sh

export PATH=/bin:/sbin:/usr/bin:/usr/sbin

for i in /etc/init.d/*; do
    if [ -x "$i" ]; then
        "$i" start
    fi
done

sleep infinity
