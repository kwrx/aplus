#!/bin/sh

# Usage: ft-cache [SYSROOT]

echo -n "" > $1/etc/fonts/fonts.conf
for line in $(find $1/usr/share/fonts/* -type f); do
    echo "${line#$1}:$(fc-scan $line | grep "family: " | cut -d '"' -f 2):$(fc-scan $line | grep "style: " | cut -d '"' -f 2)" >> $1/etc/fonts/fonts.conf
    echo "Found '$(fc-scan $line | grep "fullname: " | cut -d '"' -f 2)'"
done