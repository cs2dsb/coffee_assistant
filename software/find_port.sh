#!/usr/bin/env bash

# Exit when any command fails
set -o errexit

# Exit when an undeclared variable is used
set -o nounset

# Exit when a piped command returns a non-zero exit code
set -o pipefail

source ./env.sh 2>/dev/null

mapfile -d $'\0' TTYS < <(find /dev/serial/by-id/ -name "${TTY_PATTERN}" -print0)
N="${#TTYS[@]}"

if [ "$N" == "0" ]; then
    echo -e "$(tput setaf 0)$(tput setab 1)\n" 1>&2
    echo "   No devices matching $TTY_PATTERN found" 1>&2
    echo -e "$(tput sgr0)" 1>&2
    exit 1
elif [ "$N" != "1" ]; then
    echo -e "$(tput setaf 0)$(tput setab 3)\n" 1>&2
    echo "   More than one device matching $TTY_PATTERN found. Using the first one" 1>&2
    echo -e "$(tput sgr0)" 1>&2
fi

echo -e "$(tput setaf 0)$(tput setab 2)\n" 1>&2
echo "   Using port ${TTYS[0]}" 1>&2
echo -e "$(tput sgr0)" 1>&2

echo "${TTYS[0]}"
