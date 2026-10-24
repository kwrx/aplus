#!/bin/sh

EXEC_NAME="aplus-wm"
EXEC_DESC="Window Manager"

if cat /proc/cmdline | grep -q "mode=console"; then
    echo "init.d: Console mode detected, skipping $EXEC_DESC startup."
    exit 0
fi

case "$1" in
    start)
        if command -v $EXEC_NAME >/dev/null 2>&1; then
            echo "init.d: Starting $EXEC_DESC..."
            $EXEC_NAME
        else
            echo "init.d: $EXEC_NAME not found, skipping $EXEC_DESC startup."
        fi
        ;;
    stop)
        echo "init.d: Stopping $EXEC_DESC..."
        pkill $EXEC_NAME
        ;;
    restart)
        "$0" stop
        "$0" start
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac
