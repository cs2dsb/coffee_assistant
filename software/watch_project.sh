#!/usr/bin/env bash

set -o errexit
set -o nounset
set -o pipefail

if ! command -v inotifywait >/dev/null 2>&1; then
    echo "inotifywait missing. Try \"apt-get install inotify-tools\"" 1>&2
    exit 1
fi

PROJECT="${1:-scale}"
FILES="${FILES:-$PROJECT components}"
FLASH="${FLASH:-false}"

function run() {
    echo -e "$(tput setaf 0)$(tput setab 2)\n\n  Building \"${PROJECT}\"...\n$(tput sgr0)"
    set +o errexit
    (cd $PROJECT; idf.py app)

    if [ "$?" == "0" ]; then
        echo -e "$(tput setaf 0)$(tput setab 2)\n\n  Build succeeded \n$(tput sgr0)"
        if [ "$FLASH" == "true" ]; then
            echo -e "$(tput setaf 0)$(tput setab 2)\n\n  Flashing \n$(tput sgr0)"
            (cd $PROJECT; idf.py app-flash)
        fi
    else
        echo -e "$(tput setaf 0)$(tput setab 1)\n\n  Build failed \n$(tput sgr0)"
    fi

    set -o errexit
}

while true; do
    run | tee watch_project.ansi
    inotifywait -q -e close_write -r $FILES
done
