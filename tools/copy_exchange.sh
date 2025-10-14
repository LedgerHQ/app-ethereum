#!/bin/bash
# Copy the app binaries, for all devices, in the library test folder of the exchange app repo

if [[ $# -ne 1 ]]; then
    echo -e "Missing exchange repo directory. Abort!"
    exit
fi
exchange_dir="$1"

DEVICES=(nanos2 nanox stax flex apex_p apex_m)

for dev in "${DEVICES[@]}"; do
    elf_file="build/${dev}/bin/app.elf"
    if [[ -f ${elf_file} ]]; then
        cp "${elf_file}" "${exchange_dir}/test/python/lib_binaries/ethereum_${dev}.elf"
    else
        echo "Ignoring unknown file/device: ${elf_file}"
    fi
done
