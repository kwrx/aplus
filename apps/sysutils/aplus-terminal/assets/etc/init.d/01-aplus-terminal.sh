#!/bin/sh

EXEC_NAME="aplus-terminal"
EXEC_DESC="Terminal Emulator"

if ! cat /proc/cmdline | grep -q "mode=console"; then
    echo "init.d: console mode not detected, skipping $EXEC_DESC startup."
    exit 0
fi

case "$1" in
    start)
        if command -v $EXEC_NAME >/dev/null 2>&1; then
            echo "init.d: starting $EXEC_DESC..."
            $EXEC_NAME -f
        else
            echo "init.d: $EXEC_NAME not found, skipping $EXEC_DESC startup."
        fi
        ;;
    stop)
        echo "init.d: stopping $EXEC_DESC..."
        pkill $EXEC_NAME
        ;;
    restart)
        "$0" stop
        "$0" start
        ;;
    *)
        echo "usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac
