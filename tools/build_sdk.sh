#!/bin/bash

# Clean the sdk
find ./ethereum-plugin-sdk/ -mindepth 1 -maxdepth 1 ! -name .git -exec rm -r {} \;

# Copy exclusive files
cp -r src_plugin_sdk/* ./ethereum-plugin-sdk/

# Copy common sources
cp -r src_common/* ./ethereum-plugin-sdk/src/
