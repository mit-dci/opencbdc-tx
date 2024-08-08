#!/usr/bin/env bash

set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
DOCKER_IMAGE_TAG_BASE=${DOCKER_IMAGE_TAG:-opencbdc-tx-base}
DOCKER_IMAGE_TAG_BUILDER=${DOCKER_IMAGE_TAG:-opencbdc-tx-builder}
DOCKER_IMAGE_TAG_ATOMIZER=${DOCKER_IMAGE_TAG:-opencbdc-tx-atomizer}
DOCKER_IMAGE_TAG_TWOPHASE=${DOCKER_IMAGE_TAG:-opencbdc-tx-twophase}

# Update submodules before building
git submodule init && git submodule update

# Build docker image
docker build --target base -t "$DOCKER_IMAGE_TAG_BASE" -f "$SCRIPT_DIR"/../Dockerfile "$SCRIPT_DIR"/..
docker build --target builder --build-arg BASE_IMAGE=base -t "$DOCKER_IMAGE_TAG_BUILDER" -f "$SCRIPT_DIR"/../Dockerfile "$SCRIPT_DIR"/..
docker build --target twophase --build-arg BASE_IMAGE=base -t "$DOCKER_IMAGE_TAG_TWOPHASE" -f "$SCRIPT_DIR"/../Dockerfile "$SCRIPT_DIR"/..
docker build --target atomizer --build-arg BASE_IMAGE=base -t "$DOCKER_IMAGE_TAG_ATOMIZER" -f "$SCRIPT_DIR"/../Dockerfile "$SCRIPT_DIR"/..
