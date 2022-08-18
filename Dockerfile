FROM ubuntu:20.04

# set non-interactive shell
ENV DEBIAN_FRONTEND noninteractive

ENV CMAKE_BUILD_TYPE Release
ENV BUILD_RELEASE 1

RUN mkdir -p /opt/tx-processor/scripts

COPY scripts/configure.sh /opt/tx-processor/scripts/configure.sh

# Set working directory
WORKDIR /opt/tx-processor

RUN scripts/configure.sh

# Copy source
COPY . .

# Update submodules
RUN git submodule init && git submodule update

# Build binaries
RUN mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} .. && \
    make -j$(nproc)
