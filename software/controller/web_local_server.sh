#!/usr/bin/env bash

export PROJECT="${PWD##*/}"
cd ..
source ./export_node_path.sh
cd "${PROJECT}/web"
pwd
yarn start

