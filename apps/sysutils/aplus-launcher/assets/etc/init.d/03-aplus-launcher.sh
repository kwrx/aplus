#!/bin/sh

EXEC_NAME="aplus-launcher"
EXEC_DESC="Launcher assistant warm-up"
EXEC_ARGS="--warm"

if ! cat /proc/cmdline | grep -q "mode=desktop"; then
    echo "init.d: desktop mode not detected, service $EXEC_DESC disabled."
    exit 0
fi

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/00-init.sh"
