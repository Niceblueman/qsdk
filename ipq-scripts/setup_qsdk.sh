#!/bin/bash

# Copyright (c) 2020, The Linux Foundation. All rights reserved.
# Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# It is recommended to save any standard error messages by running script as
# 'bash setup_qsdk.sh 2>&1 | tee /var/tmp/setup_qsdk_run.log'

thisscript=$(basename "$0")
timestring=$(date '+%Y%m%d_%H%M%S%z')
logdir="/var/tmp/qsdk_setup_${timestring}"
mkdir -pv "$logdir"

echo "[$(date '+%D %T')] Start QSDK Setup Script: $thisscript"
echo "Log files will be stored at: $logdir"

# Check for root permissions
if [[ $(id -u) -ne 0 ]] ; then 
    echo "ERROR: You need sudo rights to proceed. Run this script as sudo user or root!" 
    exit 1
fi

# Check if git is installed
if ! command -v git &> /dev/null; then
    echo "ERROR: git is not installed. Exiting the setup script."
    exit 1
else
    echo "$(git --version) found. Proceeding with setup..."
fi

echo "Found $(lsb_release -d | awk -F: '{print $2}') OS installed on this system"

# Log current package list
dpkg --list > "${logdir}/pkgs_found_before.log" 2>&1

# Install packages needed for building QSDK code
DEBIAN_FRONTEND=noninteractive apt-get -y install \
    build-essential \
    libncurses5-dev \
    gawk \
    git \
    subversion \
    libssl-dev \
    python3 \
    python3-distutils \
    zlib1g-dev \
    file \
    wget \
    unzip \
    python3-pip \
    rsync \
    ocaml-findlib \
    ocaml-base-nox \
    ocaml-base \
    autoconf \
    automake \
    ccache \
    cgroupfs-mount \
    gcc \
    g++ \
    binutils \
    patch \
    bzip2 \
    flex \
    make \
    gettext \
    pkg-config \
    sharutils \
    curl \
    libxml-parser-perl \
    python3-yaml \
    ocaml-nox \
    ocaml \
    libfdt-dev \
    device-tree-compiler \
    u-boot-tools \
    parallel \
    freetds-dev \
    ocaml-native-compilers \
    libpcre-ocaml \
    libpcre-ocaml-dev \
    libpycaml-ocaml-dev \
    lzop \
    cproto

# Additional packages for Ubuntu 18.04
if [[ $(lsb_release -rs) == "18.04" ]]; then
    apt-get -y install libssl1.0-dev
fi

# Install or update Coccinelle based on Ubuntu version
if [[ $(lsb_release -rs) == "22.04" ]]; then
    apt-get -y install coccinelle
else
    # Remove any existing Coccinelle and re-install version 1.1.1 from source for compatibility
    apt-get -y remove --purge libparmap-ocaml coccinelle || true
    echo "Installing Coccinelle 1.1.1 from source"
    cd /tmp || exit
    rm -rf coccinelle
    git clone https://github.com/coccinelle/coccinelle.git
    cd coccinelle || exit
    git checkout 1.1.1
    ./autogen
    ./configure
    make
    make install
    cd /tmp || exit
    rm -rf coccinelle
fi

# Downgrade 'make' version if on Ubuntu 20.04 or 22.04
if [[ $(lsb_release -rs) == "20.04" || $(lsb_release -rs) == "22.04" ]]; then
    wget http://archive.ubuntu.com/ubuntu/pool/main/m/make-dfsg/make_4.1-9.1ubuntu1_amd64.deb
    dpkg -i make_4.1-9.1ubuntu1_amd64.deb
    rm make_4.1-9.1ubuntu1_amd64.deb
fi

# Log package list after installation
dpkg --list > "${logdir}/pkgs_found_after.log" 2>&1

# Compress log files
gzip -9 "${logdir}"/*

echo "[$(date '+%D %T')] End QSDK Setup Script: $thisscript"
