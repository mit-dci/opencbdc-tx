# Define image base arg
ARG IMAGE_VERSION="ubuntu:20.04"

# Define base image repo name
ARG BASE_IMAGE="ghcr.io/mit-dci/opencbdc-tx-base:latest"

# Create Base Image
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

# Create Build Image
FROM $BASE_IMAGE AS builder

# Copy source
COPY . .

# Build binaries
RUN mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} .. && \
    make -j$(nproc)

# Create 2PC Deployment Image
FROM $IMAGE_VERSION AS twophase

# Set working directory
WORKDIR /opt/tx-processor

# Install necessary dependencies
RUN apt-get update -y && \
    apt-get install -y libaio1 gcc make unzip && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Copy liboracleDB.so shared library
COPY --from=builder /opt/tx-processor/build/src/util/oracle/liboracleDB.so ./build/src/util/oracle/liboracleDB.so

# Copy the instantclient folder and key.txt
COPY --from=builder /opt/tx-processor/build/src/util/oracle ./build/src/util/oracle/instantclient-basic.zip
COPY --from=builder /opt/tx-processor/build/src/util/oracle ./build/src/util/oracle/instantclient-sdk.zip

# unzip the instantclient
# goto the instantclient folder
WORKDIR /opt/tx-processor/build/src/util/oracle
# print working directory and wait for 5 seconds
RUN pwd && ls -la && sleep 5
RUN unzip /opt/tx-processor/build/src/util/oracle/instantclient-basic.zip -d /opt/tx-processor/build/src/util/oracle && \
    unzip /opt/tx-processor/build/src/util/oracle/instantclient-sdk.zip -d /opt/tx-processor/build/src/util/oracle && \
    mv /opt/tx-processor/build/src/util/oracle/instantclient_21_11 /opt/tx-processor/build/src/util/oracle/instantclient
WORKDIR /opt/tx-processor

COPY --from=builder /opt/tx-processor/build/src/util/oracle ./build/src/util/oracle/key.txt

# Set LD_LIBRARY_PATH to include oracledb and instantclient
ENV LD_LIBRARY_PATH /opt/tx-processor/build/src/util/oracle:/opt/tx-processor/src/util/oracle/instantclient:${LD_LIBRARY_PATH}

# Only copy essential binaries
COPY --from=builder /opt/tx-processor/build/src/uhs/twophase/sentinel_2pc/sentineld-2pc ./build/src/uhs/twophase/sentinel_2pc/sentineld-2pc
COPY --from=builder /opt/tx-processor/build/src/uhs/twophase/coordinator/coordinatord ./build/src/uhs/twophase/coordinator/coordinatord
COPY --from=builder /opt/tx-processor/build/src/uhs/twophase/locking_shard/locking-shardd ./build/src/uhs/twophase/locking_shard/locking-shardd
COPY --from=builder /opt/tx-processor/build/tools/bench/twophase-gen ./build/tools/bench/twophase-gen

# Copy minimal test transactions script
COPY --from=builder /opt/tx-processor/scripts/test-transaction.sh ./scripts/test-transaction.sh

# Copy Client CLI
COPY --from=builder /opt/tx-processor/build/src/uhs/client/client-cli ./build/src/uhs/client/client-cli

# Copy 2PC config
COPY --from=builder /opt/tx-processor/2pc-compose.cfg ./2pc-compose.cfg

# Create Atomizer Deployment Image
FROM $IMAGE_VERSION AS atomizer

# Set working directory
WORKDIR /opt/tx-processor

# Only copy essential binaries
COPY --from=builder /opt/tx-processor/build/src/uhs/atomizer/atomizer/atomizer-raftd ./build/src/uhs/atomizer/atomizer/atomizer-raftd
COPY --from=builder /opt/tx-processor/build/src/uhs/atomizer/watchtower/watchtowerd ./build/src/uhs/atomizer/watchtower/watchtowerd
COPY --from=builder /opt/tx-processor/build/src/uhs/atomizer/archiver/archiverd ./build/src/uhs/atomizer/archiver/archiverd
COPY --from=builder /opt/tx-processor/build/src/uhs/atomizer/shard/shardd ./build/src/uhs/atomizer/shard/shardd
COPY --from=builder /opt/tx-processor/build/src/uhs/atomizer/sentinel/sentineld ./build/src/uhs/atomizer/sentinel/sentineld

# Copy minimal test transactions script
COPY --from=builder /opt/tx-processor/scripts/test-transaction.sh ./scripts/test-transaction.sh

# Copy Client CLI
COPY --from=builder /opt/tx-processor/build/src/uhs/client/client-cli ./build/src/uhs/client/client-cli

# Copy atomizer config
COPY --from=builder /opt/tx-processor/atomizer-compose.cfg ./atomizer-compose.cfg

# Create PArSEC Deployment Image
FROM $IMAGE_VERSION AS parsec

# Set working directory
WORKDIR /opt/tx-processor

# Only copy essential binaries
COPY --from=builder /opt/tx-processor/build/src/parsec/agent/agentd ./build/src/parsec/agent/agentd
COPY --from=builder /opt/tx-processor/build/src/parsec/runtime_locking_shard/runtime_locking_shardd ./build/src/parsec/runtime_locking_shard/runtime_locking_shardd
COPY --from=builder /opt/tx-processor/build/src/parsec/ticket_machine/ticket_machined ./build/src/parsec/ticket_machine/ticket_machined

# Copy load generators
COPY --from=builder /opt/tx-processor/build/tools/bench/parsec/evm/evm_bench ./build/tools/bench/parsec/evm/evm_bench
COPY --from=builder /opt/tx-processor/build/tools/bench/parsec/lua/lua_bench ./build/tools/bench/parsec/lua/lua_bench

# Copy wait script
COPY --from=builder /opt/tx-processor/scripts/wait-for-it.sh ./scripts/wait-for-it.sh
