#!/bin/bash
# Build script for uConsole (ARM64 Debian 12)

set -e

echo "Building nx for uConsole (ARM64 Debian 12)..."

# Check if we're on the right platform
if [[ "$(uname -m)" != "aarch64" ]]; then
    echo "Warning: This script is designed for ARM64 systems"
fi

# Install dependencies
echo "Installing build dependencies..."
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    curl \
    zip \
    unzip \
    tar \
    libsqlite3-dev \
    libspdlog-dev \
    libyaml-cpp-dev \
    nlohmann-json3-dev \
    libgit2-dev \
    libcurl4-openssl-dev

# Install vcpkg if not present
if [[ ! -d "/opt/vcpkg" ]]; then
    echo "Installing vcpkg..."
    sudo git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg
    cd /opt/vcpkg
    sudo ./bootstrap-vcpkg.sh
    sudo ln -sf /opt/vcpkg/vcpkg /usr/local/bin/vcpkg || true
fi

# Install vcpkg packages
echo "Installing vcpkg dependencies..."
cd /opt/vcpkg
sudo ./vcpkg install cli11 tomlplusplus ftxui

# Build nx
echo "Building nx..."
cd "$(dirname "$0")"
pwd  # Show current directory for debugging

# Configure
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build

# Run tests
echo "Running tests..."
ctest --test-dir build --output-on-failure || echo "Some tests failed, but continuing..."

# Create package
echo "Creating DEB package..."
cd build
cpack

echo "Build complete!"
echo "Package created: $(ls -1 *.deb | head -1)"
echo ""
echo "To install: sudo dpkg -i $(ls -1 *.deb | head -1)"