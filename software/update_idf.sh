#!/usr/bin/env bash

# Exit when any command fails
set -o errexit

# Exit when an undeclared variable is used
set -o nounset

# Exit when a piped command returns a non-zero exit code
set -o pipefail

git submodule update --init --recursive
(cd third_party/esp-idf; git pull)
(cd third_party/esp-idf-template; git pull)
