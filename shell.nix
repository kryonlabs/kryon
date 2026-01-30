# shell.nix (Kryon development environment with Android support)
{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # Core toolchain
    gcc
    gnumake
    pkg-config

    # SDL3 for rendering backend
    sdl3
    sdl3-ttf
    sdl3-image

    # Raylib for 3D rendering backend
    raylib

    # Text layout and shaping
    harfbuzz
    freetype
    fribidi

    # Terminal rendering backend
    libtickit

    # TypeScript/JavaScript frontend support
    bun
    nodejs  # For npm compatibility if needed

    # Python bindings support
    python3
    python3Packages.cffi
    python3Packages.build
    python3Packages.wheel
    python3Packages.pytest

    # Android development (uses system-installed SDK)
    jdk17
    gradle

    # System libraries
    libGL
    libglvnd
    xorg.libX11
    xorg.libXrandr
    xorg.libXi
    xorg.libXcursor
    libxkbcommon

    # Development tools
    git
    gdb
    which
    tree
  ];

  shellHook = ''
    # Set Android environment variables (using user's existing SDK)
    export ANDROID_HOME="$HOME/Android/Sdk"
    export ANDROID_SDK_ROOT="$ANDROID_HOME"

    # Find NDK directory
    if [ -d "$ANDROID_HOME/ndk" ]; then
      # Use the first NDK version found
      export ANDROID_NDK_HOME="$(find "$ANDROID_HOME/ndk" -maxdepth 1 -type d | grep -v "^$ANDROID_HOME/ndk$" | head -1)"
    fi

    # Find build-tools version and set GRADLE_OPTS
    if [ -d "$ANDROID_HOME/build-tools" ]; then
      BUILD_TOOLS_VERSION="$(ls "$ANDROID_HOME/build-tools" | sort -V | tail -1)"
      export GRADLE_OPTS="-Dorg.gradle.project.android.aapt2FromMavenOverride=$ANDROID_HOME/build-tools/$BUILD_TOOLS_VERSION/aapt2"
    fi

    # Kryon environment
    export KRYON_ROOT="$(pwd)"
    export PATH="$KRYON_ROOT/build:$HOME/.local/bin:$PATH"
    export LD_LIBRARY_PATH="$KRYON_ROOT/build:$HOME/.local/lib:''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export PKG_CONFIG_PATH="$KRYON_ROOT/build:$HOME/.local/lib/pkgconfig:''${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

    # TaijiOS integration
    export TAIJI_PATH="''${TAIJI_PATH:-/home/wao/Projects/TaijiOS}"

    # Validate TaijiOS installation
    if [ -d "$TAIJI_PATH" ]; then
        # Add TaijiOS tools to PATH (limbo compiler, emu, etc.)
        if [ -d "$TAIJI_PATH/Linux/amd64/bin" ]; then
            export PATH="$TAIJI_PATH/Linux/amd64/bin:$PATH"
        fi

        # Set ROOT for Inferno tools (required by emu)
        export ROOT="$TAIJI_PATH"
    else
        echo "Warning: TaijiOS not found at $TAIJI_PATH"
        echo "  Set TAIJI_PATH environment variable or install TaijiOS"
    fi

    echo "Kryon Development Environment"
    echo "============================="
    echo "Bun version: $(bun --version 2>/dev/null || echo 'not available')"
    echo ""
    echo "Quick start:"
    echo "  Build C Core: cd core && make"
    echo "  Build SDL3 renderer: cd renderers/sdl3 && make"
    echo "  Build Terminal renderer: cd renderers/terminal && make"
    echo "  Run KRY examples: ./cli/kryon run examples/kry/hello_world.kry"
    echo "  Run TypeScript examples: ./run_example.sh hello_world ts"
    echo "  Run web examples: ./run_example.sh hello_world ts web"
    echo "  Android: ./cli/kryon run --target=android examples/kry/hello_world.kry"
    echo ""
    echo "Android SDK: $ANDROID_HOME"
    echo "Android NDK: $ANDROID_NDK_HOME"
    echo ""
    echo "Available frontends: kry, typescript (ts), c, hare"
    echo "Available renderers: sdl3, raylib, terminal, framebuffer"
    echo "Available targets: web, desktop, android, limbo"
    echo ""
    echo "Desktop dependencies:"
    echo "  SDL3: $(pkg-config --modversion sdl3 2>/dev/null || echo 'not found')"
    echo "  HarfBuzz: $(pkg-config --modversion harfbuzz 2>/dev/null || echo 'not found')"
    echo ""
  '';
}
