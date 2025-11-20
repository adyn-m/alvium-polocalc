#!/usr/bin/env bash
set -e

BUILD_DIR="build"

if [ $# -ge 1 ]; then
	BUILD_DIR=$1
fi

echo "Using build directory: $BUILD_DIR"

if [ -d "$BUILD_DIR" ]; then
	echo "Removing old build directory..."
	rm -rf "$BUILD_DIR"
fi

if [ -d "=" ]; then
	rm -rf "="
fi

mkdir = "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j$(nproc)

echo "Build finished in $BUILD_DIR/"
