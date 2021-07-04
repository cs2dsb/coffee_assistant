#!/usr/bin/env bash

# Exit when any command fails
set -o errexit

# Exit when an undeclared variable is used
set -o nounset

# Exit when a piped command returns a non-zero exit code
set -o pipefail


PROJECT="${PROJECT:-${1:-skeleton}}"
FLASH="${FLASH:-false}"
TEST_HOST="${OTA_HOST:-esp32.local}"
OTA_HOST="${OTA_HOST:-}"
BUILD="${BUILD:-true}"

if [ "$FLASH" == "true" ] && [ "$OTA_HOST" != "" ]; then
    echo "Both FLASH=true and OTA_HOST provided, only one supported at a time" >&2
    exit 1
fi

source ./env.sh 2>/dev/null

if [ "$FQBN" == "" ]; then
    echo "FQBN not defined in .env*" >&2
    exit 1
fi

# spiffs is first because it doesn't trigger a restart if it's done OTA
./spiffs.sh "$PROJECT"

MQTT_IP=${MQTT_IP:-0,0,0,0}
MQTT_PORT=${MQTT_PORT:-1883}
MQTT_URL="mqtt://`echo ${MQTT_IP} | tr ',' '.'`:${MQTT_PORT}"

cat > credentials.h <<- EOF
#define PROJECT         "${PROJECT}"
#define VERSION         "${VERSION:-0.0.1}"
#define BUILD_TIMESTAMP "`date +"%Y-%m-%d %H:%M:%S"`"
#define MDNS_HOST       "${WIFI_HOST:-esp32}"
#define WIFI_SSID       "${WIFI_SSID:-}"
#define WIFI_PASSWORD   "${WIFI_PASSWORD:-}"
#define MQTT_IP         ${MQTT_IP}
#define MQTT_PORT       ${MQTT_PORT}
#define MQTT_URL        "${MQTT_URL}"
EOF

A_CLI=third_party/arduino-cli/arduino-cli
A_CLI="`realpath $A_CLI`"
A_CFG="--config-file `realpath arduino-cli.yml`"
A_BUILD="`realpath $PROJECT/build`"

echo -e "$(tput setaf 0)$(tput setab 2)\n\n  Building \"${PROJECT}\"...\n$(tput sgr0)"

set +o errexit

if [ "$BUILD" == "true" ]; then
    $A_CLI $A_CFG compile \
        --fqbn "$FQBN" \
        --build-path $A_BUILD \
        --build-cache-path $A_BUILD/cache \
        --build-property "build.extra_flags=-DESP32 -DCORE_DEBUG_LEVEL=5" \
        "$PROJECT"
        #build.extra_flags=-DESP32 -DCORE_DEBUG_LEVEL={build.code_debug}
fi


if [ "$?" == "0" ]; then
    echo -e "$(tput setaf 0)$(tput setab 2)\n\n  Build succeeded \n$(tput sgr0)"

    SERIAL_PORT=""
    if [ "$FLASH" == "true" ]; then
        set +o errexit
        SERIAL_PORT=`./find_port.sh`
        set -o errexit
    fi

    if [ "$FLASH" == "true" ] && [ "$SERIAL_PORT" != "" ]; then
        echo -e "$(tput setaf 0)$(tput setab 2)\n\n  Flashing \n$(tput sgr0)"
        PORT="${SERIAL_PORT:-}"
        if [ "$PORT" != "" ]; then
            PORT="--port $PORT"
        fi

        $A_CLI $A_CFG upload \
            $PORT \
            --fqbn "$FQBN" \
            --input-dir $A_BUILD \
            "$PROJECT"

        if [ "$?" == "0" ]; then
            echo -e "$(tput setaf 0)$(tput setab 2)\n\n  Flashing succeeded \n$(tput sgr0)"
        else
            echo -e "$(tput setaf 0)$(tput setab 1)\n\n  Flashing failed \n$(tput sgr0)"
        fi
    fi

    if [ "$OTA_HOST" != "" ]; then
        echo -e "$(tput setaf 0)$(tput setab 2)\n\n  Uploading OTA \n$(tput sgr0)"

        BIN_FILE="$A_BUILD/$PROJECT.ino.bin"
        MD5=`md5sum "$BIN_FILE" | cut -d' ' -f1`
        curl \
            --progress-bar \
            -o /dev/null \
            -F MD5="$MD5" \
            -F firmware=@"$BIN_FILE" \
            http://$OTA_HOST/update

        if [ "$?" == "0" ]; then
            echo -e "$(tput setaf 0)$(tput setab 2)\n\n  OTA succeeded \n$(tput sgr0)"
        else
            echo -e "$(tput setaf 0)$(tput setab 1)\n\n  OTA failed \n$(tput sgr0)"
        fi
    fi
else
    echo -e "$(tput setaf 0)$(tput setab 1)\n\n  Build failed \n$(tput sgr0)"
fi

