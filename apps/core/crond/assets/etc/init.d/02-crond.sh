#!/bin/sh

EXEC_NAME="crond"
EXEC_DESC="Cron Scheduler"
EXEC_ARGS="-f"

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/00-init.sh"
