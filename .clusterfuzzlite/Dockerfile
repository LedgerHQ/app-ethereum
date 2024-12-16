FROM ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest AS LITE_BUILDER

# Base image with clang toolchain
FROM gcr.io/oss-fuzz-base/base-builder:v1

# Copy the project's source code.
COPY . $SRC/app-ethereum
COPY --from=LITE_BUILDER /opt/ledger-secure-sdk $SRC/app-ethereum/BOLOS_SDK

# Add the ethereum-plugin-sdk submodule
RUN git clone https://github.com/LedgerHQ/ethereum-plugin-sdk.git $SRC/app-ethereum/ethereum-plugin-sdk

# Working directory for build.sh
WORKDIR $SRC/app-ethereum

# Copy build.sh into $SRC dir.
COPY ./.clusterfuzzlite/build.sh $SRC/
