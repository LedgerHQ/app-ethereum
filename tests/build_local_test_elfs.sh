#!/bin/bash

# FILL THESE WITH YOUR OWN SDKs PATHS
NANOS_SDK=$TWO
NANOX_SDK=$X

# list of apps required by tests that we want to build here
appnames=("ethereum" "ethereum_classic")

# create elfs folder if it doesn't exist
mkdir -p elfs

# move to repo's root to build apps
cd ..

echo "*Building elfs for Nano S..."
for app in "${appnames[@]}"
do
   echo "**Building $app for Nano S..."
   make clean BOLOS_SDK=$NANOS_SDK
   make -j DEBUG=1 ALLOW_DATA=1 BOLOS_SDK=$NANOS_SDK CHAIN=$app
   cp bin/app.elf "tests/elfs/${app}_nanos.elf"
done

echo "*Building elfs for Nano X..."
for app in "${appnames[@]}"
do
   echo "**Building $app for Nano X..."
   make clean BOLOS_SDK=$NANOX_SDK
   make -j DEBUG=1 ALLOW_DATA=1 BOLOS_SDK=$NANOX_SDK CHAIN=$app
   cp bin/app.elf "tests/elfs/${app}_nanox.elf"
done

echo "done"
