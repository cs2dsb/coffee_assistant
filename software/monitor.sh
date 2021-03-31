#!/usr/bin/env bash

set -o errexit
set -o nounset
set -o pipefail

PROJECT="${1:-skeleton}"

source ./env.sh 2>/dev/null

TTY_BAUD=${TTY_BAUD:-115200}

SERIAL_PORT=`./find_port.sh 2>/dev/null`
if [ "$SERIAL_PORT" == "" ]; then
    echo -e "$(tput setaf 0)$(tput setab 1)\n\n  Monitoring failed to find a serial port \n$(tput sgr0)"
else
    echo -e "$(tput setaf 0)$(tput setab 4)\n\n  Monitoring \n$(tput sgr0)"
    stty -F $SERIAL_PORT raw speed $TTY_BAUD
    cat $SERIAL_PORT
fi
