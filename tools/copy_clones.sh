#!/bin/bash
# Copy the app binaries, for all devices, in test dependencies folder.

set -euo pipefail

DEVICES=(nanos2 nanox stax flex apex_p apex_m)
copied=0

for dev in "${DEVICES[@]}"; do
    elf_file="build/${dev}/bin/app.elf"
    if [[ -f ${elf_file} ]]; then
        mkdir -p "tests/functional/.test_dependencies/clone/build/${dev}/bin/"
        cp "${elf_file}" "tests/functional/.test_dependencies/clone/build/${dev}/bin/"
        copied=$((copied + 1))
    else
        echo "Ignoring unknown file/device: ${elf_file}"
    fi
done

if [[ ${copied} -eq 0 ]]; then
    echo "No app binary found. Abort!"
    exit 1
fi
