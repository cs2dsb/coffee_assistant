#!/usr/bin/env bash

mkdir -p hardware
mkdir -p software/third_party

git submodule add git@github.com:cs2dsb/kicad_custom.git hardware/kicad_custom
git submodule add git@github.com:espressif/esp-idf-template.git software/third_party/esp-idf-template
git submodule add git@github.com:espressif/esp-idf.git software/third_party/esp-idf
git submodule add git@github.com:tonyp7/esp32-wifi-manager.git software/third_party/esp32-wifi-manager
git submodule add git@github.com:Hossein-M98/TM1638.git software/third_party/TM1638

git submodule update --init --recursive
