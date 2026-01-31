#!/bin/bash
# Build all vendored third-party libraries
# Usage: ./build_vendored.sh [clean]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
THIRD_PARTY="$PROJECT_ROOT/third_party"

mkdir -p "$BUILD_DIR"

echo "====================================="
echo "Building Vendored Libraries"
echo "====================================="
echo ""

# Clean if requested
if [ "$1" = "clean" ]; then
    echo "Cleaning previous builds..."
    rm -rf "$BUILD_DIR/harfbuzz"
    rm -rf "$BUILD_DIR/freetype"
    rm -rf "$BUILD_DIR/fribidi"
    echo "Done!"
    echo ""
fi

# Check for required tools
echo "Checking build tools..."
for tool in cmake meson ninja python3; do
    if ! command -v $tool &> /dev/null; then
        echo "ERROR: $tool not found. Please install it."
        echo "  On Ubuntu/Debian: apt install cmake meson ninja-build python3"
        echo "  On Alpine: apk add cmake meson ninja python3"
        echo "  On Nix: nix-shell"
        exit 1
    fi
    echo "  ✓ $tool"
done
echo ""

# Build FreeType
echo "====================================="
echo "Building FreeType 2.13.3..."
echo "====================================="
mkdir -p "$BUILD_DIR/freetype"
cmake -S "$THIRD_PARTY/freetype" -B "$BUILD_DIR/freetype" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DFT_DISABLE_ZLIB=OFF \
    -DFT_DISABLE_BZIP2=OFF \
    -DFT_DISABLE_PNG=OFF \
    -DFT_DISABLE_HARFBUZZ=ON
cmake --build "$BUILD_DIR/freetype" --parallel
echo "✓ FreeType built: $BUILD_DIR/freetype/libfreetype.a"
echo ""

# Build FriBidi
echo "====================================="
echo "Building FriBidi 1.0.16..."
echo "====================================="
mkdir -p "$BUILD_DIR/fribidi"
meson setup "$BUILD_DIR/fribidi" "$THIRD_PARTY/fribidi" \
    --buildtype=release \
    --default-library=static \
    -Ddocs=false \
    -Dbin=false \
    -Dtests=false
ninja -C "$BUILD_DIR/fribidi"
echo "✓ FriBidi built: $BUILD_DIR/fribidi/libfribidi.a"
echo ""

# Build HarfBuzz (depends on FreeType)
echo "====================================="
echo "Building HarfBuzz 12.3.0..."
echo "====================================="
mkdir -p "$BUILD_DIR/harfbuzz"
meson setup "$BUILD_DIR/harfbuzz" "$THIRD_PARTY/harfbuzz" \
    --buildtype=release \
    --default-library=static \
    -Dhave_freetype=true \
    -Dfreetype=enabled \
    -Dhave_graphite2=false \
    -Dhave_glib=false \
    -Dhave_icu=false \
    -Dintrospection=disabled \
    -Dtests=disabled \
    -Ddocs=disabled \
    -Dutilities=disabled
ninja -C "$BUILD_DIR/harfbuzz"
echo "✓ HarfBuzz built: $BUILD_DIR/harfbuzz/libharfbuzz.a"
echo ""

echo "====================================="
echo "All vendored libraries built successfully!"
echo "====================================="
echo ""
echo "Output libraries:"
echo "  FreeType: $BUILD_DIR/freetype/libfreetype.a"
echo "  FriBidi:  $BUILD_DIR/fribidi/libfribidi.a"
echo "  HarfBuzz: $BUILD_DIR/harfbuzz/libharfbuzz.a"
echo ""
echo "You can now build Kryon IR:"
echo "  cd ir && make"
