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

    # Lua frontend support (LuaJIT for FFI)
    luajit

    # TypeScript/JavaScript frontend support
    bun
    nodejs  # For npm compatibility if needed

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

    # Plan 9/9front development (for Plan 9 backend)
    plan9port  # Plan 9 from User Space
    # 9base     # Lightweight alternative (uncomment if needed)
  ];

  shellHook = ''
    # Set Plan 9 environment variables (for Plan 9 backend development)
    export PLAN9="${pkgs.plan9port}/plan9"
    export PATH="$PATH:$PLAN9/bin"  # Add to END of PATH to avoid conflicts
    export MKPATH="$PLAN9/bin/mk"

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

    echo "Kryon Development Environment"
    echo "============================="
    echo "LuaJIT version: $(luajit -v 2>/dev/null || echo 'not available')"
    echo "Bun version: $(bun --version 2>/dev/null || echo 'not available')"
    echo "Plan 9 tools: $([ -d "$PLAN9" ] && echo 'available' || echo 'not available')"
    echo ""
    echo "Quick start:"
    echo "  Build C Core: cd core && make"
    echo "  Build SDL3 renderer: cd renderers/sdl3 && make"
    echo "  Build Terminal renderer: cd renderers/terminal && make"
    echo "  Run Lua examples: ./run_example.sh hello_world lua sdl3"
    echo "  Run TypeScript examples: ./run_example.sh hello_world ts"
    echo "  Run terminal examples: ./run_example.sh hello_world lua terminal"
    echo "  Run web examples: ./run_example.sh hello_world ts web"
    echo "  Android: ./cli/kryon run --target=android examples/kry/hello_world.kry"
    echo ""
    echo "Android SDK: $ANDROID_HOME"
    echo "Android NDK: $ANDROID_NDK_HOME"
    echo ""
    echo "Available frontends: kry, typescript (ts), lua, c"
    echo "Available renderers: sdl3, raylib, terminal, framebuffer, plan9"
    echo "Available codegen targets: web, android"
    echo ""
  '';
}
