#!/bin/bash

DEVICES=(nanos nanos2 nanox stax flex)

for dev in "${DEVICES[@]}"; do
    elf_file="build/${dev}/bin/app.elf"
    if [[ -f ${elf_file} ]]; then
        mkdir -p "tests/ragger/.test_dependencies/clone/build/${dev}/bin/"
        cp "${elf_file}" "tests/ragger/.test_dependencies/clone/build/${dev}/bin/"
    else
        echo "Ignoring unknown file/dev: ${elf_file}"
    fi
done
