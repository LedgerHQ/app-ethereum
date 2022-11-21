#!/usr/bin/env bash

set -e

TESTS_PATH=$(dirname "$(realpath "$0")")

# FILL THESE WITH YOUR OWN SDKs PATHS
# NANOS_SDK=
# NANOX_SDK=

# list of apps required by tests that we want to build here
APPS=("ethereum" "ethereum_classic")

# list of SDKS
NANO_SDKS=("$NANOS_SDK" "$NANOX_SDK")
# list of target elf file name suffix
FILE_SUFFIXES=("nanos" "nanox")

# move to the tests directory
cd "$TESTS_PATH" || exit 1

# Do it only now since before the cd command, we might not have been inside the repository
GIT_REPO_ROOT=$(git rev-parse --show-toplevel)

# create elfs directory if it doesn't exist
mkdir -p elfs

# move to repo's root to build apps
cd "$GIT_REPO_ROOT" || exit 1

for ((sdk_idx=0; sdk_idx < "${#NANO_SDKS[@]}"; sdk_idx++))
do
    nano_sdk="${NANO_SDKS[$sdk_idx]}"
    elf_suffix="${FILE_SUFFIXES[$sdk_idx]}"
    echo "* Building elfs for $(basename "$nano_sdk")..."
    for appname in "${APPS[@]}"
    do
       echo "** Building app $appname..."
       make clean BOLOS_SDK="$nano_sdk"
       make -j DEBUG=1 NFT_TESTING_KEY=1 BOLOS_SDK="$nano_sdk" CHAIN="$appname"
       cp bin/app.elf "$TESTS_PATH/elfs/${appname}_${elf_suffix}.elf"
    done
done

echo "done"
