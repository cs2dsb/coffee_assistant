#!/usr/bin/env bash

# Exports any variables in .env_shared and .env
# Should be called with `source`

function export_env() {
    if test -s "$1"; then
        while IFS="" read -r line || [ -n "$line" ]; do
            echo "Exporting: $line" 1>&2
            export $line
        done < <(sed '/^\s*$/d' "$1" | sed '/^#/d') # seds remove comments and empty lines
    fi
}

export_env .env_shared
export_env .env
