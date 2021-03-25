#!/usr/bin/env bash

TTY_PATTERN='*CP2102*'
TTY_BAUD=921600

mapfile -d $'\0' TTYS < <(find /dev/serial/by-id/ -name "${TTY_PATTERN}" -print0)
N="${#TTYS[@]}"


if [ "$N" == "0" ]; then
    echo -e "$(tput setaf 0)$(tput setab 1)\n"
    echo "   No devices matching $TTY_PATTERN found" 1>&2
    echo -e "$(tput sgr0)"
elif [ "$N" != "1" ]; then
    echo -e "$(tput setaf 0)$(tput setab 3)\n"
    echo "   More than one device matching $TTY_PATTERN found. Using the first one" 1>&2
    echo -e "$(tput sgr0)"
fi

if [ "$N" != "0" ]; then
    export ESPPORT="${TTYS[0]}"
    export ESPBAUD=$TTY_BAUD

    echo -e "$(tput setaf 0)$(tput setab 2)\n"
    echo "   Using port $ESPPORT"
    echo "   Using baud $ESPBAUD"
    echo -e "$(tput sgr0)"
fi

# Source shared exports
source ./exports.dict

# And export them all
export $(cut -d= -f1 ./exports.dict)

# Drop cached creds if there are any
sudo -k

# Try and sudo without password
if sudo -n whoami &>/dev/null; then
    echo "Passwordless sudo is enabled. Running random installation scripts not recommended..." 1>&2
    exit 1
fi

cd third_party/esp-idf
source ./export.sh
cd ../..
