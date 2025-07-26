# A robust Nix shell for Rust graphics development (WGPU, Raylib, etc.)
{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    # 1. Rust Toolchain
    rustc
    cargo
    rust-analyzer
    clippy
    rustfmt

    # 2. Fast Build Tools
    pkg-config
    cmake
    gcc
    clang          # Fast C/C++ compiler
    lld            # Fast linker
    sccache        # Compilation cache
    mold           # Even faster linker alternative

    # 3. Dependencies for `rust-bindgen`
    llvmPackages.libclang
    llvmPackages.bintools

    # 4. System Libraries (Development versions)
    glibc.dev

    # 5. Graphics & Windowing System Libraries

    # Vulkan for WGPU (CRITICAL)
    vulkan-loader
    vulkan-validation-layers
    vulkan-tools
    vulkan-headers
    spirv-tools
    shaderc

    # OpenGL Libraries (for raylib and fallback)
    libGL.dev
    libglvnd.dev
    mesa.dev
    mesa.drivers

    # Intel Graphics (CRITICAL for your GPU)
    intel-media-driver      # Intel media driver
    libva                   # Video acceleration
    libva-utils             # VA-API utilities

    # X11 Libraries
    xorg.libX11.dev
    xorg.libXcursor.dev
    xorg.libXrandr.dev
    xorg.libXi.dev
    xorg.libXinerama.dev
    xorg.libXfixes.dev
    xorg.libXrender.dev
    xorg.libXext.dev
    xorg.libXxf86vm.dev
    xorg.libXmu.dev
    xorg.libXpm.dev

    # Wayland Libraries
    wayland.dev
    libxkbcommon.dev
    xorg.libxkbfile.dev

    # Audio
    alsa-lib.dev
    pulseaudio.dev

    # SDL2 Libraries (for SDL2 renderer)
    SDL2.dev
    SDL2_image
    SDL2_ttf
    SDL2_gfx
  ];

  buildInputs = with pkgs; [
    # Runtime Vulkan (CRITICAL for WGPU)
    vulkan-loader
    vulkan-validation-layers
    mesa.drivers

    # Intel Graphics Runtime (CRITICAL for your Intel HD 620)
    intel-media-driver
    libva
    
    # Runtime OpenGL libraries
    libGL
    libglvnd
    
    # Runtime X11 libraries  
    xorg.libX11
    xorg.libXcursor
    xorg.libXrandr
    xorg.libXi
    xorg.libXinerama
    xorg.libXfixes
    xorg.libXrender
    xorg.libXext
    xorg.libXxf86vm

    # Wayland/XKB runtime
    wayland
    libxkbcommon
    xorg.libxkbfile

    # Audio runtime
    alsa-lib
    pulseaudio

    # SDL2 Runtime Libraries
    SDL2
    SDL2_image
    SDL2_ttf
    SDL2_gfx
  ];

  # Environment variables
  VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";
  LIBCLANG_PATH = "${pkgs.llvmPackages.libclang.lib}/lib";
  BINDGEN_EXTRA_CLANG_ARGS = "-I${pkgs.glibc.dev}/include -I${pkgs.gcc.cc}/lib/gcc/${pkgs.stdenv.hostPlatform.config}/${pkgs.gcc.cc.version}/include";

  # LD_LIBRARY_PATH for runtime libraries
  LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath [
    # Vulkan runtime (ESSENTIAL for WGPU)
    pkgs.vulkan-loader
    pkgs.mesa.drivers

    # Intel graphics
    pkgs.intel-media-driver
    pkgs.libva

    # OpenGL
    pkgs.libGL
    pkgs.libglvnd

    # X11 libraries
    pkgs.xorg.libX11
    pkgs.xorg.libXcursor
    pkgs.xorg.libXrandr
    pkgs.xorg.libXi
    pkgs.xorg.libXinerama
    pkgs.xorg.libXfixes
    pkgs.xorg.libXxf86vm

    # Wayland/XKB
    pkgs.wayland
    pkgs.libxkbcommon
    pkgs.xorg.libxkbfile

    # Audio
    pkgs.alsa-lib
    pkgs.pulseaudio

    # SDL2 Runtime Libraries
    pkgs.SDL2
    pkgs.SDL2_image
    pkgs.SDL2_ttf
    pkgs.SDL2_gfx
  ];

  # CRITICAL: Vulkan ICD files for Intel (FIXED PATH)
  VK_ICD_FILENAMES = "${pkgs.mesa.drivers}/share/vulkan/icd.d/intel_icd.x86_64.json";
  
  # Mesa driver paths
  LIBGL_DRIVERS_PATH = "${pkgs.mesa.drivers}/lib/dri";
  LIBVA_DRIVERS_PATH = "${pkgs.intel-media-driver}/lib/dri";
  
  # Force Intel graphics
  MESA_LOADER_DRIVER_OVERRIDE = "i965";  # or "iris" for newer Intel
  
  # Help with display
  DISPLAY = ":0";

  # WGPU specific debugging (CRITICAL)
  WGPU_BACKEND = "vulkan";  # Force Vulkan backend
  WGPU_POWER_PREF = "low";  # Prefer integrated GPU
  
  # Fast compilation with sccache (disabled due to temp dir issues in nix-shell)
  # RUSTC_WRAPPER = "sccache";
  # SCCACHE_DIR = "/tmp/sccache-kryon";

  shellHook = ''
    unset RUST_LOG
    
    # Define absolute paths to kryon tools
    export KRYON_ROOT="/home/wao/lyra/proj/kryonlabs/kryon"
    export KRYON_BINARY="$KRYON_ROOT/target/release/kryon"
    
    # Function to run kryon files with unified binary
    kryon_run() {
      local input_file="$1"
      local renderer="$2"
      
      # Convert relative paths to absolute paths
      if [[ "$input_file" != /* ]]; then
        input_file="$(pwd)/$input_file"
      fi
      
      # Check if file exists
      if [[ ! -f "$input_file" ]]; then
        echo "❌ Error: File $input_file not found" >&2
        return 1
      fi
      
      # Check if file is .kry
      if [[ "$input_file" != *.kry ]]; then
        echo "❌ Error: File must be .kry (unified kryon binary handles compilation automatically)" >&2
        return 1
      fi
      
      # Run with unified kryon binary
      if [[ -n "$renderer" ]]; then
        "$KRYON_BINARY" --renderer "$renderer" "$input_file"
      else
        "$KRYON_BINARY" "$input_file"
      fi
    }
    
    # Quick renderer aliases using unified kryon binary
    kryon-raylib() {
      kryon_run "$1" "raylib"
    }
    
    kryon-wgpu() {
      kryon_run "$1" "wgpu"
    }
    
    kryon-sdl2() {
      kryon_run "$1" "sdl2"
    }
    
    kryon-ratatui() {
      RUST_LOG=off kryon_run "$1" "ratatui"
    }
    
    kryon-ratatui-log() {
      RUST_LOG=debug kryon_run "$1" "ratatui" 2>ratatui.log
    }
    
    kryon-debug() {
      kryon_run "$1" "debug"
    }
    
    # Alias for the unified kryon binary
    kryon() {
      local args=()
      for arg in "$@"; do
        if [[ "$arg" == *.kry ]]; then
          # Convert relative paths to absolute for file arguments
          if [[ "$arg" == /* ]]; then
            args+=("$arg")
          else
            args+=("$(pwd)/$arg")
          fi
        else
          args+=("$arg")
        fi
      done
      "$KRYON_BINARY" "''${args[@]}"
    }
    
    # Legacy alias for compatibility
    kryc() {
      kryon compile "$@"
    }
    
    # Quick navigation helpers
    kryon-root() { cd "$KRYON_ROOT"; }
    kryon-examples() { cd "$KRYON_ROOT/examples"; }
    
    echo "🚀 KRYON SHELL - Unified Binary Edition!"
    echo ""
    echo "🎯 UNIFIED KRYON BINARY:"
    echo "  kryon <file.kry>            - Auto-compile and run with default renderer (Raylib)"
    echo "  kryon --renderer <name> <file> - Run with specific renderer"
    echo "  kryon --compile-only <file> - Just compile to .krb"
    echo "  kryon --list-renderers      - Show available renderers"
    echo ""
    echo "🎮 QUICK RENDERER ALIASES:"
    echo "  kryon-raylib <file>         - Run with Raylib renderer"
    echo "  kryon-wgpu <file>           - Run with WGPU renderer"
    echo "  kryon-sdl2 <file>           - Run with SDL2 renderer"
    echo "  kryon-ratatui <file>        - Run with Ratatui renderer (no logs)"
    echo "  kryon-ratatui-log <file>    - Run with Ratatui renderer (logs to ratatui.log)"
    echo "  kryon-debug <file>          - Run with debug renderer"
    echo ""
    echo "🔧 LEGACY COMPATIBILITY:"
    echo "  kryc <args>                 - Legacy alias for 'kryon compile'"
    echo ""
    echo "📂 NAVIGATION HELPERS:"
    echo "  kryon-root                  - cd to $KRYON_ROOT"
    echo "  kryon-examples              - cd to $KRYON_ROOT/examples"
    echo ""
    echo "📝 EXAMPLES (from any directory!):"
    echo "  kryon ~/path/to/my/app.kry"
    echo "  kryon-raylib $KRYON_ROOT/examples/01_Getting_Started/hello_world.kry"
    echo "  kryon-ratatui $KRYON_ROOT/examples/03_Buttons_and_Events/button.kry"
    echo "  kryon --renderer wgpu --debug ~/my-app/main.kry"
    echo ""
    echo "💡 TIP: The unified binary automatically compiles .kry files and runs them!"
    echo ""
  '';
}
