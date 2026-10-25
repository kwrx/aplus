#!/bin/sh

EXEC_NAME="pymonitor"
EXEC_DESC="Web Dashboard"

case "$1" in
    start)
        if command -v $EXEC_NAME >/dev/null 2>&1 && command -v python3 >/dev/null 2>&1; then
            echo "init.d: starting $EXEC_DESC..."
            $EXEC_NAME
        else
            echo "init.d: $EXEC_NAME or python3 not found, skipping $EXEC_DESC startup."
        fi
        ;;
    stop)
        echo "init.d: stopping $EXEC_DESC..."
        pkill -f "sbin/$EXEC_NAME"
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
