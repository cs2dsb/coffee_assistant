#!/usr/bin/env bash

PROJECT="${PROJECT:-${1:-skeleton}}"
WATCH="${WATCH:-false}"
BUILD_WEB="${BUILD_WEB:-true}"

source ./env.sh 2>/dev/null

PROJECT_DIR="`realpath \"${PROJECT}\"`"
TP_DIR="`realpath third_party`"
NODE_BIN_DIR="${TP_DIR}/nodejs/bin"
WEB="${PROJECT_DIR}/web"
YARN_BIN_DIR="${WEB}/node_modules/yarn/bin"

export PATH="$NODE_BIN_DIR:$YARN_BIN_DIR:$PATH"
