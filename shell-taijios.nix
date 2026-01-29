{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # Core build tools (required by TaijiOS mk)
    gcc
    binutils
    gnumake  # May be needed for initial TaijiOS build

    # Standard utilities
    coreutils
    bash

    # Debugging (optional but useful)
    gdb
    valgrind
  ];

  shellHook = ''
    echo "Kryon TaijiOS Build Environment"
    echo "================================"

    # Set TaijiOS root directory
    export TAIJIOS_ROOT=/home/wao/Projects/TaijiOS
    export ROOT="$TAIJIOS_ROOT"

    # Add TaijiOS tools to PATH
    # mk is the Plan 9 build tool
    # Linux/amd64/bin contains built utilities
    export PATH="$TAIJIOS_ROOT/utils/mk:$TAIJIOS_ROOT/Linux/amd64/bin:$PATH"

    echo "TaijiOS root: $TAIJIOS_ROOT"
    echo "mk tool: $(which mk 2>/dev/null || echo 'NOT FOUND - build TaijiOS first')"
    echo ""
    echo "Build commands:"
    echo "  mk all      - Compile Kryon for TaijiOS"
    echo "  mk install  - Install to TaijiOS/Linux/amd64/bin/"
    echo "  mk clean    - Clean build artifacts"
    echo ""
    echo "Run commands (from TaijiOS directory):"
    echo "  cd $TAIJIOS_ROOT"
    echo "  emu Linux/amd64/bin/kryon --version"
    echo "  emu Linux/amd64/bin/kryon run app.kry"
  '';

  # Disable hardening for compatibility with TaijiOS build
  hardeningDisable = [ "all" ];
}
