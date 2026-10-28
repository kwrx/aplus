#!/bin/sh

EXEC_NAME="aplus-wm"
EXEC_DESC="Window Manager"

if ! cat /proc/cmdline | grep -q "mode=desktop"; then
    echo "init.d: desktop mode not detected, service $EXEC_DESC disabled."
    exit 0
fi

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/00-init.sh"
