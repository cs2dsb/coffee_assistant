#!/usr/bin/env bash

PROJECT="${PWD##*/}"

cd ..

WATCH=true \
    ./web.sh "$PROJECT"
