#!/usr/bin/env bash

set -e

SCAD=scale.scad
OUT=render

mkdir -p ${OUT}
if [ "$1" == "rm" ]; then
    rm -f ${OUT}/*
fi

function bounce() {
    name="${1:-}"
    if [ "${name}" == "" ]; then
        echo "bounce called with no part name..." 1>&2
        exit 1
    fi

    openscad \
        -D print=true \
        -D explode=false \
        -D part=\"${name}\" \
        -o ${OUT}/${name}.stl \
        ${SCAD} \
    &
}

bounce base
bounce posts
bounce lid

echo "Rendering `jobs -r | wc -l` objects..."

wait < <(jobs -p)

echo "Done"

