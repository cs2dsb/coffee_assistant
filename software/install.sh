#!/usr/bin/env bash

# Exit when any command fails
set -o errexit

# Exit when an undeclared variable is used
set -o nounset

# Exit when a piped command returns a non-zero exit code
set -o pipefail

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

(cd third_party/esp-idf; ./install.sh)
