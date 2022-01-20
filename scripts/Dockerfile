FROM ubuntu:20.04

ENV DEBIAN_FRONTEND noninteractive

COPY . .

RUN apt update && apt dist-upgrade -y
RUN ./configure.sh
