#!/usr/bin/env bash

# Exit when any command fails
set -o errexit

# Exit when an undeclared variable is used
set -o nounset

# Exit when a piped command returns a non-zero exit code
set -o pipefail

PROJECT="${1:-skeleton}"
FLASH="${FLASH:-false}"

source ./env.sh 2>/dev/null

if [ "$FQBN" == "" ]; then
    echo "FQBN not defined in .env*" >&2
    exit 1
fi

if [ "$FLASH" == "true" ]; then
    SERIAL_PORT=`./find_port.sh`
    if [ "$SERIAL_PORT" == "" ]; then
        echo "Failed to find a serial port" >&2
        exit 1
    fi
fi

cat > credentials.h <<- EOF
#define VERSION         "${VERSION:-0.0.1}"
#define MDNS_HOST       "${WIFI_HOST:-esp32}"
#define WIFI_SSID       "${WIFI_SSID:-}"
#define WIFI_PASSWORD   "${WIFI_PASSWORD:-}"
EOF

A_CLI=third_party/arduino-cli/arduino-cli
A_CLI="`realpath $A_CLI`"
A_CFG="--config-file `realpath arduino-cli.yml`"
A_BUILD="`realpath $PROJECT/build`"

echo -e "$(tput setaf 0)$(tput setab 2)\n\n  Building \"${PROJECT}\"...\n$(tput sgr0)"
set +o errexit

$A_CLI $A_CFG compile --fqbn "$FQBN" --build-path $A_BUILD --build-cache-path $A_BUILD/cache  "$PROJECT"

if [ "$?" == "0" ]; then
    echo -e "$(tput setaf 0)$(tput setab 2)\n\n  Build succeeded \n$(tput sgr0)"
    if [ "$FLASH" == "true" ]; then
        echo -e "$(tput setaf 0)$(tput setab 2)\n\n  Flashing \n$(tput sgr0)"
        PORT="${SERIAL_PORT:-}"
        if [ "$PORT" != "" ]; then
            PORT="--port $PORT"
        fi

        $A_CLI $A_CFG upload --fqbn "$FQBN" $PORT "$PROJECT"
        if [ "$?" == "0" ]; then
            echo -e "$(tput setaf 0)$(tput setab 2)\n\n  Flashing succeeded \n$(tput sgr0)"
        else
            echo -e "$(tput setaf 0)$(tput setab 1)\n\n  Flashing failed \n$(tput sgr0)"
        fi
    fi
else
    echo -e "$(tput setaf 0)$(tput setab 1)\n\n  Build failed \n$(tput sgr0)"
fi
