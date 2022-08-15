# Define image base arg
ARG IMAGE_VERSION="ubuntu:20.04"

# Create Base Image
FROM $IMAGE_VERSION AS base

# set non-interactive shell
ENV DEBIAN_FRONTEND noninteractive

# install base packages
RUN apt update \
    apt -y --quiet install \
    build-essential \
    wget \
    cmake \
    libgtest-dev \
    libgmock-dev \
    net-tools \
    lcov \
    git \
    && apt -y autoremove \
    && apt clean autoclean \
    && rm -rf /var/lib/apt/lists/{apt,dpkg,cache,log} /tmp/* /var/tmp/*

# Args
ARG CMAKE_BUILD_TYPE="Release"
ARG LEVELDB_VERSION="1.22"
ARG NURAFT_VERSION="1.3.0"

# Install LevelDB
RUN wget https://github.com/google/leveldb/archive/${LEVELDB_VERSION}.tar.gz && \
    tar xzvf ${LEVELDB_VERSION}.tar.gz && \
    rm -f ${LEVELDB_VERSION}.tar.gz && \
    cd leveldb-${LEVELDB_VERSION} && \
    cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DLEVELDB_BUILD_TESTS=0 -DLEVELDB_BUILD_BENCHMARKS=0 -DBUILD_SHARED_LIBS=0 . && \
    make -j$(nproc) && \
    make install

# Install NuRaft
RUN wget https://github.com/eBay/NuRaft/archive/v${NURAFT_VERSION}.tar.gz && \
    tar xzvf v${NURAFT_VERSION}.tar.gz && \
    rm v${NURAFT_VERSION}.tar.gz && \
    cd "NuRaft-${NURAFT_VERSION}" && \
    ./prepare.sh && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DDISABLE_SSL=1 .. && \
    make -j$(nproc) static_lib && \
    cp libnuraft.a /usr/local/lib && \
    cp -r ../include/libnuraft /usr/local/include

# Set working directory
WORKDIR /opt/tx-processor

# Create Build Image
FROM base AS builder

# Copy source
COPY . .

# Update submodules and run configure.sh
RUN git submodule init && git submodule update

# Build binaries
RUN mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} .. && \
    make -j$(nproc)

# Create 2PC Deployment Image
FROM $IMAGE_VERSION AS two-pc

# Set working directory
WORKDIR /opt/tx-processor

# Only copy essential binaries
COPY --from=builder  /opt/tx-processor/build/src/uhs/twophase/sentinel_2pc/sentineld-2pc ./build/src/uhs/twophase/sentinel_2pc/sentineld-2pc
COPY --from=builder  /opt/tx-processor/build/src/uhs/twophase/coordinator/coordinatord ./build/src/uhs/twophase/coordinator/coordinatord
COPY --from=builder  /opt/tx-processor/build/src/uhs/twophase/locking_shard/locking-shardd ./build/src/uhs/twophase/locking_shard/locking-shardd

# Copy Client CLI
COPY --from=builder  /opt/tx-processor/build/src/uhs/client/client-cli ./build/src/uhs/client/client-cli

# Copy 2PC config
COPY --from=builder  /opt/tx-processor/2pc-compose.cfg ./2pc-compose.cfg

# Create Atomizer Deployment Image
FROM $IMAGE_VERSION AS atomizer

# Set working directory
WORKDIR /opt/tx-processor

# Only copy essential binaries
COPY --from=builder /opt/tx-processor/build/src/uhs/atomizer/atomizer/atomizer-raftd ./build/src/uhs/atomizer/atomizer/atomizer-raftd
COPY --from=builder /opt/tx-processor/build/src/uhs/atomizer/watchtower/watchtowerd ./build/src/uhs/atomizer/watchtower/watchtowerd
COPY --from=builder /opt/tx-processor/build/src/uhs/atomizer/watchtower/watchtowerd ./build/src/uhs/atomizer/watchtower/watchtowerd
COPY --from=builder /opt/tx-processor/build/src/uhs/atomizer/archiver/archiverd ./build/src/uhs/atomizer/archiver/archiverd
COPY --from=builder /opt/tx-processor/build/src/uhs/atomizer/shard/shardd ./build/src/uhs/atomizer/shard/shardd
COPY --from=builder /opt/tx-processor/build/src/uhs/atomizer/sentinel/sentineld ./build/src/uhs/atomizer/sentinel/sentineld

# Copy Client CLI
COPY --from=builder  /opt/tx-processor/build/src/uhs/client/client-cli ./build/src/uhs/client/client-cli

# Copy atomizer config
COPY --from=builder  /opt/tx-processor/atomizer-compose.cfg ./atomizer-compose.cfg
