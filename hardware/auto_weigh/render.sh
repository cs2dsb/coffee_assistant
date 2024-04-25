#!/usr/bin/env bash

set -e

outdir="output"

mkdir -p $outdir
if [ "$1" == "rm" ]; then
    rm -f ${outdir}/*
fi

in_file=main.scad

parts=(
    "frame_bottom"
    "frame_side"
    "bottom_gear"
    "top_gear"
    "side_gear"
    "stirrer"
    "part_guide_left"
    "part_guide_top"
    "bottom_gear_shaft"
    "plate"
    "plate_cup"
    "motor_cup"
)

for part in ${parts[@]}; do
    openscad \
        -D print=true \
        -D part=\"${part}\" \
        -D cross_section=false \
        -o ${outdir}/${part}.stl \
        ${in_file} &

done

echo "Rendering `jobs -r | wc -l` objects..."

wait < <(jobs -p)

echo "Done"

