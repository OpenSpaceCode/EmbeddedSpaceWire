#!/usr/bin/env bash
# Run EmbeddedSpaceWire unit tests (requires Linux with apt)
set -e

# Install Google Test if not already present
if ! dpkg -s libgtest-dev &>/dev/null; then
    echo "Installing libgtest-dev..."
    sudo apt-get update -q
    sudo apt-get install -y libgtest-dev g++
fi

# Initialise submodules (EmbeddedSpacePacket) if not already done
git submodule update --init --recursive

make test
