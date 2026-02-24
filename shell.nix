{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # The Compiler
    tinycc
    gcc  # Alternative compiler for better warnings

    # Build Tools
    gnumake
    pkg-config

    # SDL2 for display client (required)
    SDL2
    SDL2.dev

    # Math library
    glibc.dev

    # Testing Tools (provides 9mount, 9p, etc.)
    plan9port

    # Network testing
    netcat
  ];

  shellHook = ''
    # Add plan9port to PATH (both bin and plan9/bin)
    export PLAN9="${pkgs.plan9port}/plan9"
    export PATH="$PLAN9/bin:${pkgs.plan9port}/bin:$PATH"

    echo "--- Kryon Labs Dev Environment ---"
    echo "Compiler: tcc (C89/C90)"
    echo "SDL2: Loaded for display client"
    echo "Testing tools: plan9port loaded"
    echo "----------------------------------"
    echo ""
    echo "Build: make"
    echo "Run server: ./bin/kryon-server --port 17019"
    echo "Run display: ./bin/kryon-display"
    echo "Test 9P: 9p -a 'tcp!127.0.0.1!17019' ls /"
    echo "Note: Use -a flag for direct TCP connection"
    echo "      Binaries have RPATH set and work outside nix-shell"
  '';
}
