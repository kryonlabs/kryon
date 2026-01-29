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
    echo "==================================="
    echo "Kryon Development Environment"
    echo "==================================="
    echo ""
    echo "Available build modes: ${buildModes}"
    echo ""

    ${if hasTaijios then ''
      export TAIJIOS_ROOT="${toString taijiosPath}"
      export PATH="$TAIJIOS_ROOT/Linux/amd64/bin:$PATH"
      echo "✓ TaijiOS detected: $TAIJIOS_ROOT"
      echo "  - mk tool available"
      echo "  - Build: make -f Makefile.taijios"
      echo ""
    '' else ''
      echo "  TaijiOS not found at ${toString taijiosPath}"
      echo "  Install for native Plan 9 builds"
      echo ""
    ''}

    ${if hasInferno then ''
      export INFERNO="${toString infernoPath}"
      echo "✓ Inferno detected: $INFERNO"
      echo "  - lib9 available"
      echo "  - Build: make -f Makefile.inferno"
      echo ""
    '' else ''
      echo "  Inferno not found"
      echo "  Install Inferno for rc shell support:"
      echo "    git clone https://github.com/inferno-os/inferno-os.git /opt/inferno"
      echo "    cd /opt/inferno && ./makemk.sh && mk install"
      echo ""
    ''}

    echo "Standard Linux build: make"
    echo ""
    echo "==================================="
    echo "Available renderers:"
    echo "  • SDL2: $(pkg-config --modversion sdl2 2>/dev/null || echo 'not found')"
    echo "  • Raylib: $(pkg-config --modversion raylib 2>/dev/null || echo 'bundled')"
    echo "  • OpenGL: $(pkg-config --modversion gl 2>/dev/null || echo 'system GL')"
    echo ""
    echo "Quick commands:"
    echo "  make              # Standard Linux build"
    ${if hasInferno then ''
    echo "  make -f Makefile.inferno    # With Inferno support"
    '' else ""}
    ${if hasTaijios then ''
    echo "  make -f Makefile.taijios    # For TaijiOS"
    '' else ""}
    echo "  make clean        # Clean build artifacts"
    echo "  make debug        # Debug build"
    echo ""
    echo "See docs/BUILD.md for detailed build instructions"
    echo "==================================="

    # Set some useful environment variables
    export PKG_CONFIG_PATH="${pkgs.xorg.libX11.dev}/lib/pkgconfig:${pkgs.xorg.libXext.dev}/lib/pkgconfig:${pkgs.libGL.dev}/lib/pkgconfig:$PKG_CONFIG_PATH"

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
