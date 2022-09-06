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
    "cs2dsb/AsyncElegantOTA"
    "gavinlyonsrepo/TM1638plus"
    "olkal/HX711_ADC"
    "lorol/LITTLEFS"
    "knolleary/pubsubclient"
    "paulstoffregen/OneWire"
    "milesburton/Arduino-Temperature-Control-Library"
)

source ./env.sh 2>/dev/null

# Drop cached creds if there are any
sudo -k

# Try and sudo without password
if sudo -n whoami &>/dev/null; then
    echo "Passwordless sudo is enabled. Running random installation scripts not recommended..." 1>&2
    exit 1
fi

# Install arduino-cli
A_CLI=third_party/arduino-cli/arduino-cli
if ! test -s $A_CLI; then
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
if ! test -s $BROTLI; then
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

ZOPFIL=third_party/zopfli/zopfli
if ! test -s $ZOPFIL; then
    mkdir -p third_party/zopfli
    cd third_party/zopfli

    curl -sL https://api.github.com/repos/google/zopfli/releases/latest \
        | grep tarball_url \
        | cut -d : -f 2,3 \
        | tr -d \" \
        | tr -d , \
        | wget -q -O zopfli.tar.gz -i -
    tar --strip-components=1 -xf zopfli.tar.gz
    rm zopfli.tar.gz

    make

    cd ../..
fi

# MOSQUITTO=third_party/eclipse/mosquitto
# if ! test -s $MOSQUITTO; then
#     mkdir -p third_party/eclipse
#     cd third_party/eclipse

#     curl -sL https://api.github.com/repos/eclipse/mosquitto/tags \
#         | jq '.[0].tarball_url' \
#         | tr -d \" \
#         | wget -q -O mosquitto.tar.gz -i -
#     tar --strip-components=1 -xf mosquitto.tar.gz
#     rm mosquitto.tar.gz

#     make

#     cd ../..
# fi

MKLITTLEFS=third_party/mklittlefs/mklittlefs
if ! test -s $MKLITTLEFS; then
    mkdir -p third_party/mklittlefs/littlefs
    cd third_party/mklittlefs

    curl -sL https://api.github.com/repos/littlefs-project/littlefs/releases/latest \
        | grep tarball_url \
        | cut -d : -f 2,3 \
        | tr -d \" \
        | tr -d , \
        | wget -q -O littlefs.tar.gz -i -
    tar --strip-components=1 -xf littlefs.tar.gz -C littlefs
    rm littlefs.tar.gz

    curl -sL https://api.github.com/repos/earlephilhower/mklittlefs/releases/latest \
        | grep tarball_url \
        | cut -d : -f 2,3 \
        | tr -d \" \
        | tr -d , \
        | wget -q -O mklittlefs.tar.gz -i -
    tar --strip-components=1 -xf mklittlefs.tar.gz


    rm mklittlefs.tar.gz
    make
    cd ../..
fi

NODEJS=third_party/nodejs/bin/node
if ! test -s $NODEJS; then
    mkdir -p third_party/nodejs
    cd third_party/nodejs
    wget -q -O nodejs.tar.xz https://nodejs.org/dist/v14.16.0/node-v14.16.0-linux-x64.tar.xz
    tar --strip-components=1 -xf nodejs.tar.xz
    rm nodejs.tar.xz
fi

# OPENJDK=third_party/openjdk/bin
# if ! test -d $OPENJDK; then
#     mkdir -p third_party/openjdk
#     cd third_party/openjdk

#     wget -q -O openjdk.tar.gz https://download.java.net/java/GA/jdk16/7863447f0ab643c585b9bdebf67c69db/36/GPL/openjdk-16_linux-x64_bin.tar.gz
#     tar --strip-components=1 -xf openjdk.tar.gz
#     rm openjdk.tar.gz
#     cd ../..
# fi
# PATH="$PATH:`realpath $OPENJDK`"

# ESP32FS=third_party/arduino-esp32fs-plugin/esp32fs.jar
# if ! test -s $ESP32FS; then
#     mkdir -p third_party/arduino-esp32fs-plugin
#     cd third_party/arduino-esp32fs-plugin

#     curl -sL https://api.github.com/repos/lorol/arduino-esp32fs-plugin/releases/latest \
#         | grep esp32fs.zip \
#         | grep browser_download_url \
#         | cut -d : -f 2,3 \
#         | tr -d \" \
#         | tr -d , \
#         | wget -q -O esp32fs.zip -i -
#     unzip esp32fs.zip
#     rm esp32fs.zip
#     cd ../../
# fi

# Install esp32 core
BASE_DIR="`realpath .`"
cat > arduino-cli.yml <<- EOF
board_manager:
  additional_urls:
    - https://dl.espressif.com/dl/package_esp32_index.json
    # - https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

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
