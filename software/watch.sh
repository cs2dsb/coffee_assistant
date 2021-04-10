#!/usr/bin/env bash

set -o errexit
set -o nounset
set -o pipefail

if ! command -v inotifywait >/dev/null 2>&1; then
    echo "inotifywait missing. Try \"apt-get install inotify-tools\"" 1>&2
    exit 1
fi

PROJECT="${1:-skeleton}"
FILES="${FILES:-$PROJECT third_party build.sh .env .env_shared arduino-cli.yml *.h}"
FLASH="${FLASH:-false}"
MONITOR="${MONITOR:-false}"

source ./env.sh 2>/dev/null

TTY_BAUD=${TTY_BAUD:-115200}

MONITOR_JOB=""

cleanup() {
    local pids=$(jobs -pr)
    [ -n "$pids" ] && kill $pids
}

trap "trap - SIGTERM && cleanup" SIGINT SIGTERM EXIT

# Delete the old spiffs to force it to transfer them if we ctrl-c'd out previously
rm -rf "$PROJECT/build/spiffs" "$PROJECT/build/spiffs.old"

while true; do
    set +o errexit
    if [ "$MONITOR_JOB" != "" ]; then
        kill $MONITOR_JOB
        MONITOR_JOB=""
    fi


    ./build.sh "$PROJECT"
    RC=$?

    if [ "$RC" == "0" ] && [ "$MONITOR" == "true" ]; then
        SERIAL_PORT=`./find_port.sh 2>/dev/null`
        echo $SERIAL_PORT
        if [ "$SERIAL_PORT" == "" ]; then
            echo -e "$(tput setaf 0)$(tput setab 1)\n\n  Monitoring failed to find a serial port \n$(tput sgr0)"
        else
            echo -e "$(tput setaf 0)$(tput setab 4)\n\n  Monitoring \n$(tput sgr0)"
            stty -F $SERIAL_PORT raw speed $TTY_BAUD
            cat $SERIAL_PORT &
            MONITOR_JOB="$!"
        fi
    fi
    set -o errexit

    inotifywait -q -e close_write -r $FILES
done
