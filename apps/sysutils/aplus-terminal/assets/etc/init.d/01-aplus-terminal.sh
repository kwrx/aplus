#!/bin/sh

EXEC_NAME="aplus-terminal"
EXEC_DESC="Terminal Emulator"
EXEC_ARGS="-f"

if ! cat /proc/cmdline | grep -q "mode=console"; then
    echo "init.d: console mode not detected, service $EXEC_DESC disabled."
    exit 0
fi


. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/00-init.sh"
