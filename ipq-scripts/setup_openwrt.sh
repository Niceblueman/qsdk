#!/bin/bash

# Script to install OpenWrt build prerequisites
# For Debian/Ubuntu-based systems

set -e

echo "Installing OpenWrt build prerequisites..."

# Update package lists
sudo apt-get update

# Install essential packages
sudo apt-get install -y \
    build-essential \
    libncurses5-dev \
    gawk \
    git \
    libssl-dev \
    gettext \
    unzip \
    zlib1g-dev \
    file \
    python3 \
    python3-distutils \
    python3-setuptools \
    rsync \
    wget \
    perl \
    perl-base \
    cpanminus \
    bash \
    patch \
    tar \
    coreutils \
    bzip2 \
    findutils \
    grep \
    diffutils \
    flex \
    bison \
    gcc \
    g++ \
    install-info

# Install GNU utilities (some systems might have BSD versions)
sudo apt-get install -y \
    binutils

echo "Installing required Perl modules..."
# Install Perl modules using cpanm
sudo cpanm --notest \
    Data::Dumper \
    File::Copy \
    File::Compare \
    Thread::Queue \
    FindBin

# For systems using musl libc (uncomment if needed)
#sudo apt-get install -y \
#    musl-dev \
#    musl-tools \
#    argp-standalone \
#    musl-fts-dev \
#    musl-obstack-dev \
#    musl-libintl

# Verify some key requirements
echo "Verifying installations..."

# Check GCC version
gcc_version=$(gcc -dumpversion)
echo "GCC version: $gcc_version"
if ! echo "$gcc_version" | grep -E '^([6-9]|1[0-9])'; then
    echo "Warning: GCC version should be 6 or later"
fi

# Check Python version
python3 --version
if ! python3 -c 'import sys; exit(0 if sys.version_info >= (3, 6) else 1)'; then
    echo "Warning: Python version should be 3.6 or later"
fi

# Check for Git
git --version
# Check for coreutils
if ! command -v realpath &> /dev/null; then
    echo "Warning: realpath (part of coreutils) is not installed"
fi

if ! command -v which &> /dev/null; then
    echo "Warning: which (part of coreutils) is not installed"
fi
# Verify Perl modules
echo "Verifying Perl modules..."
perl -MData::Dumper -e1 || echo "Warning: Data::Dumper not properly installed"
perl -MFile::Copy -e1 || echo "Warning: File::Copy not properly installed"
perl -MFile::Compare -e1 || echo "Warning: File::Compare not properly installed"
perl -MThread::Queue -e1 || echo "Warning: Thread::Queue not properly installed"
perl -MFindBin -e1 || echo "Warning: FindBin not properly installed"

# Verify umask
current_umask=$(umask)
if [[ ! "$current_umask" =~ ^0?0[012][012]$ ]]; then
    echo "Warning: Current umask is $current_umask. It should be 022 for OpenWrt builds."
    echo "You can set it using: umask 022"
fi

# Check filesystem case sensitivity
TEST_DIR=$(mktemp -d)
touch "$TEST_DIR/test.file"
if [ -f "$TEST_DIR/TEST.FILE" ]; then
    echo "Warning: Filesystem is not case-sensitive. OpenWrt requires a case-sensitive filesystem."
fi
rm -f "$TEST_DIR/test.file"
rmdir "$TEST_DIR"

echo "Prerequisites installation completed!"
echo "Note: Please ensure you have set umask 022 before building OpenWrt"
echo "You can do this by running: umask 022"
