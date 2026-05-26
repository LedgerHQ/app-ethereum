# Base image with clang toolchain
FROM gcr.io/oss-fuzz-base/base-builder:v1

# Install additional package dependencies.
RUN pip3 install --break-system-packages --no-cache-dir pillow>=3.4.0
RUN apt update && apt install -y ninja-build zip libbsd-dev pkg-config git

# The app-builder SDK image does not ship the fuzzing framework. Use the
# framework branch pinned to the same commit as the local campaign workflow.
ARG LEDGER_SECURE_SDK_REPO=https://github.com/LedgerHQ/ledger-secure-sdk.git
ARG LEDGER_SECURE_SDK_REF=86bc16516e2acf8644d20f40ad8cf2a6012b1cc5
RUN git init /ledger-secure-sdk \
    && cd /ledger-secure-sdk \
    && git remote add origin "${LEDGER_SECURE_SDK_REPO}" \
    && git fetch --depth 1 origin "${LEDGER_SECURE_SDK_REF}" \
    && git checkout FETCH_HEAD \
    && test -f fuzzing/cmake/LedgerAppFuzz.cmake \
    && echo "Ledger fuzz framework present at ${LEDGER_SECURE_SDK_REF}"

# Copy the project's source code.
COPY . /app

# Working directory for build.sh
WORKDIR /app

# Copy build.sh into $SRC dir.
COPY ./.clusterfuzzlite/build.sh $SRC/
