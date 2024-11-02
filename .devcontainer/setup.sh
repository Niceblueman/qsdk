#!/bin/bash

# Update package lists and install development dependencies
apt-get update

# Base development packages
apt-get install -y build-essential gcc g++ flex bison gettext texinfo \
                   patch ncurses-dev perl automake autoconf libtool \
                   cmake zlib1g-dev liblzo2-dev uuid-dev openssl libssl-dev \
                   binutils binutils-gold bzip2 make pkg-config libc6-dev \
                   subversion libncurses5-dev sharutils curl libxml-parser-perl \
                   ocaml-nox ocaml-findlib libpcre3-dev dropbear-bin gradle \
                   mtd-utils u-boot-tools device-tree-compiler autopoint \
                   libmosquitto1 libmosquitto-dev libprotobuf-c1 libprotobuf-c-dev \
                   libev4 libev-dev libjansson4 libjansson-dev patchutils \
                   openvswitch-switch python python3-dev python3-pip python-pexpect \
                   python-tabulate python-termcolor python-paramiko python3-paramiko \
                   wget unzip rsync git-core gawk dos2unix vim file

# Install 32-bit libraries if required for QSDK tools
dpkg --add-architecture i386
apt-get update
apt-get install -y libc6:i386 libstdc++6:i386 zlib1g:i386 libc6-dev-i386

# Install Python dependencies for OpenWRT/QSDK
pip3 install kconfiglib 'MarkupSafe<2.0.0' 'Jinja2<3.0.0'

echo "Development environment setup complete."
