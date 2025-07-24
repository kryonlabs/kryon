# A Nix shell for Rust compiler development (kryon-compiler)
{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    # 1. Rust Toolchain
    rustc
    cargo
    rust-analyzer
    clippy
    rustfmt

    # 2. Core Build Tools
    pkg-config
    cmake
    gcc

    # 3. Dependencies for `rust-bindgen`
    llvmPackages.libclang
    llvmPackages.bintools

    # 4. System Libraries (Development versions)
    glibc.dev

    # 5. Additional tools for development
    hexdump
    xxd
  ];

  buildInputs = with pkgs; [
    # Runtime libraries if needed
    glibc
  ];

  # Environment variables
  LIBCLANG_PATH = "${pkgs.llvmPackages.libclang.lib}/lib";
  BINDGEN_EXTRA_CLANG_ARGS = "-I${pkgs.glibc.dev}/include -I${pkgs.gcc.cc}/lib/gcc/${pkgs.stdenv.hostPlatform.config}/${pkgs.gcc.cc.version}/include";

  # LD_LIBRARY_PATH for runtime libraries
  LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath [
    pkgs.glibc
  ];

  shellHook = ''
    unset RUST_LOG
    
    echo "🚀 KRYON COMPILER COMMANDS:"
    echo ""
    echo "  # Build compiler:"
    echo '    cargo build --release'
    echo ""
    echo "  # Run compiler:"
    echo '    cargo run -- input.kry output.krb'
    echo '    cargo run -- compile input.kry -o output.krb --optimization aggressive'
    echo ""
    echo "  # Testing:"
    echo '    cargo test'
    echo '    cargo bench'
    echo '    cargo clippy'
    echo '    cargo fmt'
    echo ""
    echo "  # Development tools:"
    echo '    cargo run -- check input.kry --recursive'
    echo '    cargo run -- analyze output.krb --format json'
    echo '    cargo run -- benchmark input.kry --iterations 100'
    echo ""
  '';
}