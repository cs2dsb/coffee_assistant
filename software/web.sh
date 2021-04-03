#!/usr/bin/env bash

# Only run one at a time
[ "${FLOCKER}" != "$0" ] && exec env FLOCKER="$0" flock -e "$0" "$0" "$@" || :

# Exit when any command fails
set -o errexit

# Exit when an undeclared variable is used
set -o nounset

# Exit when a piped command returns a non-zero exit code
set -o pipefail

PROJECT="${1:-skeleton}"
WATCH="${WATCH:-false}"

source ./env.sh 2>/dev/null

PROJECT_DIR="`realpath \"${PROJECT}\"`"
TP_DIR="`realpath third_party`"

NODE_BIN_DIR="${TP_DIR}/nodejs/bin"

WEB="${PROJECT_DIR}/web"
STATIC="${PROJECT_DIR}/static"
YARN="${WEB}/node_modules/yarn/bin/yarn"

if test -d "$WEB"; then
    cd "$WEB"

    export PATH="$NODE_BIN_DIR:$PATH"

    if ! test -s "$YARN"; then
        npm install yarn
        rm package-lock.json
    fi

    npx yarn install

    if [ "$WATCH" == "true" ]; then
        npx yarn build:watch
    else
        npx yarn build
    fi

    cd ../..
fi
