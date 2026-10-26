#!/bin/bash
# Copy the app binaries, for all devices, in the library test folder of the exchange app repo

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo -e "Missing exchange repo directory. Abort!"
    exit 1
fi
exchange_dir=$(realpath -e -- "$1")
dest_dir=$(realpath -e -- "${exchange_dir}/test/python/lib_binaries")
# The destination must stay inside the exchange repo
if [[ "${dest_dir}/" != "${exchange_dir}/"* ]]; then
    echo "Destination ${dest_dir} is outside ${exchange_dir}. Abort!"
    exit 1
fi

DEVICES=(nanos2 nanox stax flex apex_p apex_m)

for dev in "${DEVICES[@]}"; do
    elf_file="build/${dev}/bin/app.elf"
    if [[ -f ${elf_file} ]]; then
        # Replace the destination entry instead of writing through a symlink
        cp --remove-destination --no-target-directory -- "${elf_file}" "${dest_dir}/ethereum_${dev}.elf"
    else
        echo "Ignoring unknown file/device: ${elf_file}"
    fi
done
