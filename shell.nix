{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # Build tool + Plan 9 compiler toolchain (mk, 9c, 9l, 9ar, ...)
    plan9port

    # SDL2 for display client
    SDL2
    pkg-config

    # Math library
    glibc.dev

    # Network testing
    netcat
  ];

  shellHook = ''
    export PLAN9="${pkgs.plan9port}/plan9"
    export PATH="$PLAN9/bin:${pkgs.plan9port}/bin:$PATH"

    echo "--- Kryon Labs Dev Environment ---"
    echo "Build tool : mk (Plan 9 make)"
    echo "Compiler   : 9c (Plan 9 C compiler)"
    echo "Linker     : 9l (Plan 9 linker)"
    echo "Display    : Plan 9 libdraw (via plan9port)"
    echo ""
    echo "Commands:"
    echo "  mk           # build everything"
    echo "  mk clean     # clean"
    echo "  9c -c foo.c  # compile single file with 9c"
    echo "----------------------------------"
    echo ""
    echo "Run display:  ./bin/kryon-view"
    echo "Run WM:       ./bin/kryon-wm"
    echo "Run scripts:  ./bin/lu9 script.lua"
    echo "Test 9P:      9p -a 'tcp!127.0.0.1!17010' ls /"
  '';
}
