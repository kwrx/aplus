#!/bin/sh

die() {
    echo "init.d: $*" >&2
    exit 1
}

log() {
    echo "init.d: $*"
}

if [ "$(basename "$0")" = "00-init.sh" ]; then
    die "do not run 00-init.sh directly"
fi

: "${EXEC_NAME:?init.d: required variable EXEC_NAME is not set}"
: "${EXEC_DESC:?init.d: required variable EXEC_DESC is not set}"

case "$EXEC_NAME" in
    ''|*[!A-Za-z0-9_.-]*)
        die "invalid EXEC_NAME: $EXEC_NAME"
        ;;
esac

PID_FILE="/var/run/init.d/${0##*/}.pid"


read_pid() {
    [ -f "$PID_FILE" ] ||
        die "PID file does not exist: $PID_FILE"

    PID=$(cat "$PID_FILE") ||
        die "cannot read PID file: $PID_FILE"

    case "$PID" in
        ''|*[!0-9]*|0)
            die "invalid PID in $PID_FILE"
            ;;
    esac

    echo "$PID"
}

is_running() {
    kill -0 "$1" 2>/dev/null
}

remove_pid_file() {
    rm -f "$PID_FILE" ||
        die "cannot remove PID file: $PID_FILE"
}

start_service() {
    if [ -f "$PID_FILE" ]; then
        PID=$(read_pid)

        if is_running "$PID"; then
            log "$EXEC_DESC is already running (PID $PID)"
            return 0
        fi

        log "removing stale PID file for $EXEC_DESC"
        remove_pid_file
    fi

    command -v "$EXEC_NAME" >/dev/null 2>&1 ||
        die "'$EXEC_NAME' not found; cannot start $EXEC_DESC"

    log "starting $EXEC_DESC..."

    "$EXEC_NAME" $EXEC_ARGS &
    PID=$!

    if ! echo "$PID" > "$PID_FILE"; then
        kill "$PID" 2>/dev/null
        die "cannot write PID file: $PID_FILE"
    fi

    log "$EXEC_DESC started (PID $PID)"
}

stop_service() {
    if [ ! -f "$PID_FILE" ]; then
        log "$EXEC_DESC is not running"
        return 0
    fi

    PID=$(read_pid)

    if ! is_running "$PID"; then
        log "$EXEC_DESC is not running; removing stale PID file"
        remove_pid_file
        return 0
    fi

    log "stopping $EXEC_DESC (PID $PID)..."

    kill "$PID" ||
        die "failed to stop $EXEC_DESC (PID $PID)"

    ATTEMPTS=0
    while is_running "$PID" && [ "$ATTEMPTS" -lt 5 ]; do
        sleep 1
        ATTEMPTS=$((ATTEMPTS + 1))
    done

    if is_running "$PID"; then
        die "$EXEC_DESC (PID $PID) did not stop; PID file retained"
    fi

    remove_pid_file
    log "$EXEC_DESC stopped"
}

status_service() {
    if [ ! -f "$PID_FILE" ]; then
        log "$EXEC_DESC is not running"
        return 1
    fi

    PID=$(read_pid)

    if is_running "$PID"; then
        log "$EXEC_DESC is running (PID $PID)"
        return 0
    fi

    log "$EXEC_DESC is not running; removing stale PID file"
    remove_pid_file
    return 1
}

case "$1" in
    start)
        start_service
        ;;
    stop)
        stop_service
        ;;
    restart)
        stop_service || exit $?
        start_service
        ;;
    status)
        status_service
        exit $?
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}" >&2
        exit 2
        ;;
esac
