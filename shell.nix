{ pkgs ? import <nixpkgs> {} }:

let
  # Detect TaijiOS installation
  taijiosPath = /home/wao/Projects/TaijiOS;
  hasTaijios = builtins.pathExists taijiosPath;

  # Detect Inferno installation (check common paths)
  infernoSearchPaths = [
    /opt/inferno
    /usr/local/inferno
    /usr/inferno
  ];
  # Add home directory paths (using builtins.getEnv)
  homeDir = builtins.getEnv "HOME";
  homePaths =
    if homeDir != "" then [
      (homeDir + "/inferno")
      (homeDir + "/Projects/inferno")
    ] else [];
  allInfernoPaths = infernoSearchPaths ++ homePaths;

  # Find first existing Inferno path
  findInferno = paths:
    if paths == [] then null
    else if builtins.pathExists (builtins.head paths)
      then builtins.head paths
      else findInferno (builtins.tail paths);

  infernoPath = findInferno allInfernoPaths;
  hasInferno = infernoPath != null;

  # Build mode message
  buildModes =
    if hasTaijios && hasInferno then "Standard Linux, Inferno, and TaijiOS"
    else if hasTaijios then "Standard Linux and TaijiOS (install Inferno for rc shell support)"
    else if hasInferno then "Standard Linux and Inferno (install TaijiOS for native Plan 9)"
    else "Standard Linux only";

in pkgs.mkShell {
  buildInputs = with pkgs; [
    # Build system
    gnumake
    pkg-config

    # Compilers and tools
    gcc
    clang
    gdb
    valgrind

    # Core libraries
    stdenv.cc.libc

    # Graphics and windowing
    xorg.libX11
    xorg.libXext
    xorg.libXrandr
    xorg.libXinerama
    xorg.libXcursor
    xorg.libXi
    xorg.libXdmcp
    xorg.libxcb
    libGL
    libGLU
    mesa

    # SDL2 for one of our renderers
    SDL2
    SDL2_image
    SDL2_ttf
    SDL2_mixer

    # Raylib for another renderer
    raylib

    # Font rendering
    freetype
    fontconfig

    # Audio (optional)
    alsa-lib
    pulseaudio

    # Networking
    curl
    openssl

    # Image processing
    libpng
    libjpeg

    # Text processing and Unicode
    icu

    # Math libraries
    gsl

    # Development tools
    bear  # for compile_commands.json generation
    clang-tools  # clang-format, clang-tidy
    doxygen  # documentation

    # Testing
    cunit

    # Optional: JavaScript engine (for HTML renderer)
    nodejs

    # Optional: Python (if needed for tools)
    python3

    # Git (for development)
    git
  ];

  shellHook = ''
    echo "=========================================="
    echo "Kryon Development Environment"
    echo "=========================================="
    echo ""
    echo "Build modes: ${buildModes}"
    echo ""

    ${if hasTaijios then ''
      export TAIJIOS_ROOT="${toString taijiosPath}"
      export PATH="$TAIJIOS_ROOT/Linux/amd64/bin:$PATH"
      echo "✓ TaijiOS detected: $TAIJIOS_ROOT"
      echo "  - Enables: Limbo language plugin"
      echo "  - Build: make PLUGINS=limbo"
      echo ""
    '' else ''
      echo "  TaijiOS not found at ${toString taijiosPath}"
      echo "  Install for Limbo language support"
      echo ""
    ''}

    ${if hasInferno then ''
      export INFERNO="${toString infernoPath}"
      echo "✓ Inferno detected: $INFERNO"
      echo "  - Enables: sh (Inferno shell) language plugin"
      echo "  - Provides: lib9 compatibility layer"
      echo "  - Build: make (auto-detected)"
      echo ""
    '' else ''
      echo "  Inferno not found"
      echo "  Install for sh language plugin:"
      echo "    git clone https://github.com/inferno-os/inferno-os.git /opt/inferno"
      echo "    cd /opt/inferno && ./makemk.sh && mk install"
      echo ""
    ''}

    echo "=========================================="
    echo "Supported Targets:"
    echo "  • sdl2    - Desktop GUI via SDL2"
    echo "  • raylib  - Desktop GUI via Raylib"
    echo "  • web     - Static HTML/CSS/JS output"
    echo "  • emu     - TaijiOS emu via KRBVIEW"
    echo ""
    echo "Supported Languages (Plugins):"
    echo "  • native  - Kryon built-in (always available)"
    ${if hasInferno then ''
    echo "  • sh      - Inferno shell scripts (available)"
    '' else ''
    echo "  • sh      - Inferno shell scripts (not available)"
    ''}
    ${if hasTaijios then ''
    echo "  • limbo   - TaijiOS Limbo modules (available)"
    '' else ''
    echo "  • limbo   - TaijiOS Limbo modules (not available)"
    ''}
    echo ""
    echo "Available Renderers:"
    echo "  • SDL2:   $(pkg-config --modversion sdl2 2>/dev/null || echo 'not found')"
    echo "  • Raylib: $(pkg-config --modversion raylib 2>/dev/null || echo 'not found')"
    echo "  • Web:    always available (HTML/CSS/JS)"
    echo ""
    echo "Build Commands:"
    echo "  make              # Standard build (auto-detects Inferno)"
    echo "  make debug        # Debug build"
    echo "  make clean        # Clean build artifacts"
    ${if hasTaijios then ''
    echo "  make PLUGINS=limbo    # Enable Limbo plugin"
    '' else ""}
    echo ""
    echo "Run Commands:"
    echo "  ./build/bin/kryon compile app.kry"
    echo "  ./build/bin/kryon run app.krb --target=sdl2"
    echo "  ./build/bin/kryon run app.krb --target=web"
    echo ""
    echo "See docs/BUILD.md for detailed instructions"
    echo "=========================================="

    # Set some useful environment variables
    export PKG_CONFIG_PATH="${pkgs.SDL2.dev}/lib/pkgconfig:${pkgs.raylib}/lib/pkgconfig:${pkgs.xorg.libX11.dev}/lib/pkgconfig:${pkgs.xorg.libXext.dev}/lib/pkgconfig:${pkgs.libGL.dev}/lib/pkgconfig:${pkgs.fontconfig.dev}/lib/pkgconfig:$PKG_CONFIG_PATH"

    # Ensure proper library paths
    export LD_LIBRARY_PATH="${pkgs.libGL}/lib:${pkgs.xorg.libX11}/lib:${pkgs.SDL2}/lib:${pkgs.raylib}/lib:$LD_LIBRARY_PATH"

    # For development convenience
    alias build="make"
    alias clean="make clean"
    alias rebuild="make clean && make"
  '';
  
  # Ensure we have the right C compiler flags
  NIX_CFLAGS_COMPILE = [
    "-I${pkgs.SDL2.dev}/include"
    "-I${pkgs.raylib}/include"
    "-I${pkgs.freetype.dev}/include/freetype2"
    "-I${pkgs.libGL.dev}/include"
    "-I${pkgs.xorg.libX11.dev}/include"
  ];
  
  NIX_LDFLAGS = [
    "-L${pkgs.SDL2}/lib"
    "-L${pkgs.raylib}/lib"
    "-L${pkgs.libGL}/lib"
    "-L${pkgs.xorg.libX11}/lib"
  ];
}
