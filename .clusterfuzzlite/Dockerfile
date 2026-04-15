FROM ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest AS app-builder

# Base image with clang toolchain
FROM gcr.io/oss-fuzz-base/base-builder:v1

# Install additional package dependencies.
RUN pip3 install --break-system-packages --no-cache-dir pillow>=3.4.0
RUN apt update && apt install -y ninja-build zip libbsd-dev pkg-config

# Copy the project's source code.
COPY . /app
COPY --from=app-builder /opt/flex-secure-sdk /ledger-secure-sdk

# Working directory for build.sh
WORKDIR /app

# Copy build.sh into $SRC dir.
COPY ./.clusterfuzzlite/build.sh $SRC/
