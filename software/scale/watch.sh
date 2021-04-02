#!/usr/bin/env bash


PROJECT="${PWD##*/}"

cd ..

# SPIFFS_SIZE override is so we don't have to transfer the full partition every
# rebuild loop. If the packer fails, bump this number up a bit
SPIFFS_SIZE=0x40000 \
    ./watch.sh "$PROJECT"
