FROM ubuntu:20.04

ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai
WORKDIR /home/mitsuba
COPY . /home/mitsuba

RUN apt update -y && apt upgrade -y && apt install -y build-essential pkg-config apt-utils python3 python3-pip cmake libxxf86vm-dev libpcrecpp0v5 libglu1-mesa-dev mesa-common-dev git vim curl sudo \
    && pip3 install scons-compiledb conan SCons \
    && conan profile new default --detect \
    && conan profile update settings.compiler.libcxx=libstdc++11 default \
    && apt clean \
    && cd /home/mitsuba \
    && cp build/config-linux-gcc.py config.py \
    && echo 'source /home/mitsuba/setpath.sh' >> ~/.bashrc \
    && cd /home/mitsuba/build \
    && CONAN_REVISIONS_ENABLED=1 CONAN_SYSREQUIRES_MODE=enabled conan install .. --build=missing \
    && cd /home/mitsuba/ \
    && scons -j 8

