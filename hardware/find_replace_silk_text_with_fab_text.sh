#!/bin/bash

find . -iname \*.kicad_pcb \
-exec sed \
-i -E "s/(\(fp_text reference[^(]+\([^(]+\)[^(]+\(layer )(:?F.SilkS)\)/\1F.Fab\)/"  {} \;

find . -iname \*.kicad_pcb \
-exec sed \
-i -E "s/(\(fp_text reference[^(]+\([^(]+\)[^(]+\(layer )(:?B.SilkS)\)/\1B.Fab\)/"  {} \;