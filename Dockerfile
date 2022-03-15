ARG IMAGE_VERSION="ubuntu:20.04"

FROM $IMAGE_VERSION AS base

# set non-interactive shell
ENV DEBIAN_FRONTEND noninteractive
ENV CMAKE_BUILD_TYPE Release
ENV BUILD_RELEASE 1

RUN mkdir -p /opt/tx-processor/scripts

COPY scripts/configure.sh /opt/tx-processor/scripts/configure.sh

# Set working directory
WORKDIR /opt/tx-processor

RUN scripts/configure.sh

# Build base
FROM base AS builder

# Copy source
COPY . .

# Update submodules
RUN git submodule init && git submodule update

# Build binaries
RUN mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} .. && \
    make -j$(nproc)

# Deployment Image
FROM $IMAGE_VERSION AS deploy

WORKDIR /opt/tx-processor

# Copy files
COPY --from=builder  /opt/tx-processor .
