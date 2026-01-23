#!/bin/bash
# =============================================================================
# ORIP Release Build Script
# =============================================================================
# Usage: ./scripts/build_release.sh [version]
# Example: ./scripts/build_release.sh v0.1.0
# =============================================================================

set -e

VERSION="${1:-v0.1.0}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
RELEASE_DIR="$PROJECT_ROOT/release/$VERSION"
BUILD_DIR="$PROJECT_ROOT/build_release"

echo "=============================================="
echo "ORIP Release Build - $VERSION"
echo "=============================================="

# Detect platform
detect_platform() {
    case "$(uname -s)" in
        Linux*)     echo "linux";;
        Darwin*)    echo "macos";;
        CYGWIN*|MINGW*|MSYS*) echo "windows";;
        *)          echo "unknown";;
    esac
}

PLATFORM=$(detect_platform)
echo "Platform: $PLATFORM"

# Create directories
mkdir -p "$RELEASE_DIR/$PLATFORM"
mkdir -p "$RELEASE_DIR/include"
mkdir -p "$BUILD_DIR"

# Clean previous build
rm -rf "$BUILD_DIR"/*

# Build static library
echo ""
echo "[1/4] Building static library..."
cd "$BUILD_DIR"
cmake "$PROJECT_ROOT" \
    -DCMAKE_BUILD_TYPE=Release \
    -DORIP_BUILD_TESTS=OFF \
    -DORIP_BUILD_EXAMPLES=OFF \
    -DORIP_BUILD_SHARED=OFF

cmake --build . --config Release --parallel

# Copy static library
if [ "$PLATFORM" = "windows" ]; then
    cp -f "$BUILD_DIR/Release/orip.lib" "$RELEASE_DIR/$PLATFORM/" 2>/dev/null || \
    cp -f "$BUILD_DIR/orip.lib" "$RELEASE_DIR/$PLATFORM/" 2>/dev/null || true
else
    cp -f "$BUILD_DIR/liborip.a" "$RELEASE_DIR/$PLATFORM/"
fi

# Build shared library
echo ""
echo "[2/4] Building shared library..."
rm -rf "$BUILD_DIR"/*
cd "$BUILD_DIR"
cmake "$PROJECT_ROOT" \
    -DCMAKE_BUILD_TYPE=Release \
    -DORIP_BUILD_TESTS=OFF \
    -DORIP_BUILD_EXAMPLES=OFF \
    -DORIP_BUILD_SHARED=ON

cmake --build . --config Release --parallel

# Copy shared library
if [ "$PLATFORM" = "linux" ]; then
    cp -f "$BUILD_DIR/liborip.so"* "$RELEASE_DIR/$PLATFORM/" 2>/dev/null || true
elif [ "$PLATFORM" = "macos" ]; then
    cp -f "$BUILD_DIR/liborip.dylib"* "$RELEASE_DIR/$PLATFORM/" 2>/dev/null || \
    cp -f "$BUILD_DIR/liborip.so"* "$RELEASE_DIR/$PLATFORM/" 2>/dev/null || true
elif [ "$PLATFORM" = "windows" ]; then
    cp -f "$BUILD_DIR/Release/orip.dll" "$RELEASE_DIR/$PLATFORM/" 2>/dev/null || \
    cp -f "$BUILD_DIR/orip.dll" "$RELEASE_DIR/$PLATFORM/" 2>/dev/null || true
fi

# Copy headers
echo ""
echo "[3/4] Copying header files..."
cp -r "$PROJECT_ROOT/include/orip" "$RELEASE_DIR/include/"

# Generate checksums
echo ""
echo "[4/4] Generating checksums..."
cd "$RELEASE_DIR"
if command -v sha256sum &> /dev/null; then
    find "$PLATFORM" -type f -exec sha256sum {} \; > "$PLATFORM/checksums.sha256"
elif command -v shasum &> /dev/null; then
    find "$PLATFORM" -type f -exec shasum -a 256 {} \; > "$PLATFORM/checksums.sha256"
fi

# Summary
echo ""
echo "=============================================="
echo "Release build complete!"
echo "=============================================="
echo "Output directory: $RELEASE_DIR/$PLATFORM"
echo ""
echo "Contents:"
ls -la "$RELEASE_DIR/$PLATFORM"
echo ""
echo "Headers:"
ls -la "$RELEASE_DIR/include/orip"

# Cleanup
rm -rf "$BUILD_DIR"

echo ""
echo "Done!"
