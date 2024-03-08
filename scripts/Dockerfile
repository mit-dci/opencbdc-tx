FROM ubuntu:22.04

ENV DEBIAN_FRONTEND noninteractive

COPY . .

RUN apt update && apt dist-upgrade -y
RUN ./install-build-tools.sh
RUN ./setup-dependencies.sh
