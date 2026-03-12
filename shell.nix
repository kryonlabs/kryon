{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # SDL2 for display client
    SDL2
    pkg-config

    # Math library
    glibc.dev

    # Network testing
    netcat
  ];

  shellHook = ''
    # Set ROOT to TaijiOS root (parent of kryon directory)
    export ROOT="$(cd "$(dirname "$PWD")" && pwd)"

    # Use bootstrapped toolchain from utils/
    export PLAN9="$ROOT/utils"
    export OBJTYPE=amd64

    # Add bootstrapped tools to PATH
    export PATH="$ROOT/$OBJTYPE:$ROOT/utils:$PATH"

    # Verify toolchain is available
    if [ ! -x "$ROOT/$OBJTYPE/mk" ]; then
      echo "Error: Toolchain not bootstrapped!"
      echo "Run from TaijiOS root: nix-shell"
      exit 1
    fi

    # SDL2 flags for mk
    export SDL2_CFLAGS="${pkgs.SDL2.dev}/include"
    export SDL2_LDFLAGS="-L${pkgs.SDL2}/lib -lSDL2"

    # Combine flags for mk
    export EXTRA_CFLAGS="-I$SDL2_CFLAGS"
    export EXTRA_LDFLAGS="$SDL2_LDFLAGS"

    echo "--- Kryon Labs Dev Environment ---"
    echo "Toolchain   : Bootstrapped (utils/)"
    echo "Build tool   : mk (amd64/mk)"
    echo "Compiler     : 9c (utils/9c)"
    echo "Linker       : 9l (utils/9l)"
    echo "Display      : SDL2"
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
