#!/bin/sh

EXEC_NAME="ntpd"
EXEC_DESC="NTP daemon"
EXEC_ARGS="-n -d"

if [ -r /etc/ntp.conf ]; then
    while read -r key value _; do
        case "$value" in
            ''|-*|*[!A-Za-z0-9.:_-]*)
                continue
                ;;
        esac

        [ "$key" = "server" ] && EXEC_ARGS="$EXEC_ARGS -p $value"
    done < /etc/ntp.conf
fi

[ "$EXEC_ARGS" = "-n -d" ] && EXEC_ARGS="$EXEC_ARGS -p pool.ntp.org"

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/00-init.sh"
