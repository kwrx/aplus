#!/bin/sh

EXEC_NAME="pymonitor"
EXEC_DESC="Web Dashboard"

if ! command -v python3 >/dev/null 2>&1; then
    echo "init.d: python3 not found, service $EXEC_DESC disabled."
    exit 0
fi

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/00-init.sh"
