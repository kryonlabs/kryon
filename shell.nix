{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # Build system
    cmake
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
    
    # Lua scripting engine
    lua5_4
    
    # Git (for development)
    git
  ];
  
  shellHook = ''
    echo "🚀 Kryon-C Development Environment"
    echo "================================="
    echo "Available renderers:"
    echo "  • SDL2: $(pkg-config --modversion sdl2 2>/dev/null || echo 'not found')"
    echo "  • Raylib: Available"
    echo "  • OpenGL: $(pkg-config --modversion gl 2>/dev/null || echo 'system GL')"
    echo ""
    echo "Build with:"
    echo "  mkdir build && cd build"
    echo "  cmake .."
    echo "  make"
    echo ""
    echo "Environment ready! 🎉"
    
    # Set some useful environment variables
    export CMAKE_BUILD_TYPE=Debug
    export PKG_CONFIG_PATH="${pkgs.xorg.libX11.dev}/lib/pkgconfig:${pkgs.xorg.libXext.dev}/lib/pkgconfig:${pkgs.libGL.dev}/lib/pkgconfig:$PKG_CONFIG_PATH"
    
    # Ensure proper library paths
    export LD_LIBRARY_PATH="${pkgs.libGL}/lib:${pkgs.xorg.libX11}/lib:${pkgs.SDL2}/lib:${pkgs.raylib}/lib:$LD_LIBRARY_PATH"
    
    # For development convenience
    alias build="mkdir -p build && cd build && cmake .. && make"
    alias clean="rm -rf build && rm -f *.o"
    alias test="cd build && ctest"
    alias run-examples="cd build/bin/examples"
  '';
  
  # Ensure we have the right C compiler flags
  NIX_CFLAGS_COMPILE = [
    "-I${pkgs.SDL2.dev}/include"
    "-I${pkgs.raylib}/include"
    "-I${pkgs.freetype.dev}/include/freetype2"
    "-I${pkgs.libGL.dev}/include"
    "-I${pkgs.xorg.libX11.dev}/include"
    "-I${pkgs.lua5_4}/include"
  ];
  
  NIX_LDFLAGS = [
    "-L${pkgs.SDL2}/lib"
    "-L${pkgs.raylib}/lib"
    "-L${pkgs.libGL}/lib"
    "-L${pkgs.xorg.libX11}/lib"
    "-L${pkgs.lua5_4}/lib"
  ];
}