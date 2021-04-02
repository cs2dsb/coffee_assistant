#!/usr/bin/env bash

# Exports any variables in .env_shared and .env
# Should be called with `source`

function export_env() {
    if test -s "$1"; then
        while IFS="" read -r line || [ -n "$line" ]; do
            VAR=`echo "$line" | cut -d'=' -f1 | xargs`
            VAL=`echo "$line" | cut -d'=' -f2 | xargs`
            echo "Exporting: $VAR=\"$VAL\"" 1>&2
            export $VAR="$VAL"
        done < <(sed '/^\s*$/d' "$1" | sed '/^#/d') # seds remove comments and empty lines
    fi
}

export_env .env_shared
export_env .env
