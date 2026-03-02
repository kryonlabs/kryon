#!/usr/bin/env bash
# WebAssembly Build Script for Kryon
# Compiles Kryon to WebAssembly using Emscripten

set -euo pipefail

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info() { echo -e "${GREEN}[WASM Build]${NC} $1"; }
warn() { echo -e "${YELLOW}[WASM Build]${NC} $1"; }

# Check for Emscripten
if ! command -v emcc >/dev/null 2>&1; then
    warn "Emscripten not found!"
    echo "Install from: https://emscripten.org/docs/getting_started/downloads.html"
    echo ""
    echo "Or activate emscripten env:"
    echo "  source /path/to/emsdk/emsdk_env.sh"
    echo "  emcc activate latest"
    exit 1
fi

info "Emscripten found: $(emcc -version)"

# Project root
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
KRYON_ROOT="$PROJECT_ROOT/kryon"
MARROW_ROOT="$PROJECT_ROOT/marrow"

info "Project root: $PROJECT_ROOT"
info "Kryon root: $KRYON_ROOT"

# Build directory
BUILD_DIR="$KRYON_ROOT/build/wasm"
mkdir -p "$BUILD_DIR"

# Compiler flags
EMCC_CFLAGS="-std=c89 -Wall -Wpedantic -g"
EMCC_FLAGS="-s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MAX_WEBGL_VERSION=2"
EMCC_FLAGS="$EMCC_FLAGS -s USE_SDL=2 -s SDL2_IMAGE_FORMATS='[\"png\"]'"
EMCC_FLAGS="$EMCC_FLAGS -s USE_SSL=1 -lws"
EMCC_FLAGS="$EMCC_FLAGS -s EXPORTED_FUNCTIONS=['_main'] -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
EMCC_FLAGS="$EMCC_FLAGS --embed-file ../marrow/bin/marrow@/marrow"

# TODO: Build Marrow as WebAssembly first
# For now, skip Marrow build

info "Building Kryon for WebAssembly..."

cd "$KRYON_ROOT"

# Build Kryon sources
info "Compiling Kryon sources..."
emcc $EMCC_CFLAGS $EMCC_FLAGS \
    -Iinclude \
    -Imarrow/include \
    src/core/*.c \
    src/kryon/*.c \
    src/lib/client/*.c \
    ../marrow/lib/9p/*.c \
    ../marrow/lib/graphics/*.c \
    ../marrow/lib/registry/*.c \
    ../marrow/sys/*.c \
    ../marrow/lib/platform/*.c \
    ../marrow/lib/auth/*.c \
    -L../marrow/build \
    -lmarrow \
    -lm \
    -o "$BUILD_DIR/kryon.html"

if [ $? -eq 0 ]; then
    info "WebAssembly build successful!"
    info "Output: $BUILD_DIR/kryon.html"
    info ""
    info "Serve with: cd $BUILD_DIR && python3 -m http.server 8000"
    info "Then open: http://localhost:8000/kryon.html"
else
    warn "Build failed!"
    warn "This is expected - WebAssembly support is experimental"
fi

info "WebAssembly build script complete"
