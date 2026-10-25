#!/bin/sh

EXEC_NAME="aplus-wm"
EXEC_DESC="Window Manager"

if ! cat /proc/cmdline | grep -q "mode=desktop"; then
    echo "init.d: desktop mode not detected, skipping $EXEC_DESC startup."
    exit 0
fi

case "$1" in
    start)
        if command -v $EXEC_NAME >/dev/null 2>&1; then
            echo "init.d: starting $EXEC_DESC..."
            $EXEC_NAME
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
