#!/usr/bin/env bash


PROJECT="${PWD##*/}"

cd ..

# SPIFFS_SIZE override is so we don't have to transfer the full partition every
# rebuild loop. If the packer fails, bump this number up a bit
SPIFFS_SIZE=0xF0000 \
TTY_OVERRIDE=${TTY_OVERRIDE:-/dev/ttyUSB1} \
FLASH=${FLASH:-true} \
MONITOR=${MONITOR:-true} \
BUILD_WEB=false \
OTA_HOST=${OTA_HOST:-} \
    ./watch.sh "$PROJECT"
