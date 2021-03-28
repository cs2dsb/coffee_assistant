#!/usr/bin/env bash

# Exit when any command fails
set -o errexit

# Exit when an undeclared variable is used
set -o nounset

# Exit when a piped command returns a non-zero exit code
set -o pipefail

# These git libs (mostly) don't have releases and/or packages so we're just
# using master. At some point for more reliable builds it would be a good idea
# to use submodules or at least add commit hashes in here.
GIT_LIBRARIES=(
    "me-no-dev/AsyncTCP"
    "me-no-dev/ESPAsyncWebServer"
    "ayushsharma82/AsyncElegantOTA"
    "gavinlyonsrepo/TM1638plus"
    "olkal/HX711_ADC"
)

source ./env.sh 2>/dev/null

# Drop cached creds if there are any
sudo -k

# # Try and sudo without password
# if sudo -n whoami &>/dev/null; then
#     echo "Passwordless sudo is enabled. Running random installation scripts not recommended..." 1>&2
#     exit 1
# fi

# Install arduino-cli
A_CLI=third_party/arduino-cli/arduino-cli
if ! test -s $A_CLI &>/dev/null; then
    mkdir -p third_party/arduino-cli
    cd third_party/arduino-cli

    curl -sL https://api.github.com/repos/arduino/arduino-cli/releases/latest \
        | grep "Linux_64bit.tar.gz" \
        | grep browser_download_url \
        | cut -d : -f 2,3 \
        | tr -d \" \
        | tr -d , \
        | wget -q -O arduino-cli.tar.gz -i -
    tar -xf arduino-cli.tar.gz
    rm arduino-cli.tar.gz
    cd ../..
fi

BROTLI=third_party/brotli/brotli
if ! test -s $BROTLI &>/dev/null; then
    mkdir -p third_party/brotli
    cd third_party/brotli

    curl -sL https://api.github.com/repos/google/brotli/releases/latest \
        | grep tarball_url \
        | cut -d : -f 2,3 \
        | tr -d \" \
        | tr -d , \
        | wget -q -O brotli.tar.gz -i -
    tar --strip-components=1 -xf brotli.tar.gz
    rm brotli.tar.gz
    ./configure-cmake
    make
    cd ../..
fi

# Install esp32 core
BASE_DIR="`realpath .`"
cat > arduino-cli.yml <<- EOF
board_manager:
  additional_urls:
    - https://dl.espressif.com/dl/package_esp32_index.json

directories:
  user: "$BASE_DIR"
  sketchbook: "$BASE_DIR"
  data: "$BASE_DIR/.arduino15"
  downloads: "$BASE_DIR/.arduino15/staging"

logging:
    level: info
EOF

A_CLI="`realpath $A_CLI`"
A_CFG="--config-file `realpath arduino-cli.yml`"

A_LIB_PATH="`$A_CLI $A_CFG config dump | grep sketchbook | sed 's/.*\ //'`/libraries"
mkdir -p "$A_LIB_PATH"
for lib in ${GIT_LIBRARIES[@]}; do
    echo "LIB: $lib"
    url=https://github.com/$lib/archive/refs/heads/master.zip
    wget -q -O /tmp/library.zip $url
    unzip -d "$A_LIB_PATH" -o /tmp/library.zip
done

$A_CLI $A_CFG core update-index
$A_CLI $A_CFG core install esp32:esp32 --config-file arduino-cli.yml

./build.sh skeleton
