#!/bin/sh

EXEC_NAME="ntpd"
EXEC_DESC="NTP daemon"
EXEC_ARGS="-n -d -p pool.ntp.org"

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/00-init.sh"
