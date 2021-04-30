#!/usr/bin/env bash

# Exit when any command fails
set -o errexit

# Exit when an undeclared variable is used
set -o nounset

# Exit when a piped command returns a non-zero exit code
set -o pipefail

RENAME=`realpath "${BASH_SOURCE%/*}/kicad_custom/rename_gbrs.sh"`
PROJECTS=(mcu)

for t in ${PROJECTS[@]}; do
    WD="${BASH_SOURCE%/*}/$t"
    if test -d "$WD"; then
        echo -e "$(tput setaf 0)$(tput setab 2)\n\n  Renaming gerbers for \"$t\"\n$(tput sgr0)"
        (cd "$WD"; "$RENAME")
    fi
done

echo -e "$(tput setaf 0)$(tput setab 2)\n\n  All done\n$(tput sgr0)"
