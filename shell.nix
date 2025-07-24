# Unified Nix shell for Kryon monorepo development (Compiler + Renderer)
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

    # 5. Graphics & Windowing System Libraries
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

    # Intel Graphics
    intel-media-driver
    libva
    libva-utils

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

    # Additional tools for development
    hexdump
    xxd
  ];

  buildInputs = with pkgs; [
    # Runtime Vulkan
    vulkan-loader
    vulkan-validation-layers
    mesa.drivers

    # Intel Graphics Runtime
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

    # Runtime libraries
    glibc
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

    # Runtime libraries
    pkgs.glibc
  ];

  # Vulkan ICD files for Intel
  VK_ICD_FILENAMES = "${pkgs.mesa.drivers}/share/vulkan/icd.d/intel_icd.x86_64.json";
  
  # Mesa driver paths
  LIBGL_DRIVERS_PATH = "${pkgs.mesa.drivers}/lib/dri";
  LIBVA_DRIVERS_PATH = "${pkgs.intel-media-driver}/lib/dri";
  
  # Force Intel graphics
  MESA_LOADER_DRIVER_OVERRIDE = "i965";
  
  # Help with display
  DISPLAY = ":0";

  # WGPU specific settings
  WGPU_BACKEND = "vulkan";
  WGPU_POWER_PREF = "low";

  shellHook = ''
    unset RUST_LOG
    
    # Unified CLI aliases for monorepo  
    alias kryc='cargo run --bin kryon --'
    alias kryon-raylib='cargo run --bin kryon-renderer-raylib --'
    alias kryon-wgpu='cargo run --bin kryon-renderer-wgpu --'
    alias kryon-ratatui='RUST_LOG=off cargo run --bin kryon-renderer-ratatui --'
    alias kryon-debug='cargo run --bin kryon-renderer-debug --'
    alias kryon-bundle='cargo run --bin kryon-bundle --'
    
    # For ratatui with log file redirection
    alias kryon-ratatui-log='RUST_LOG=debug cargo run --bin kryon-renderer-ratatui -- 2>ratatui.log'
    
    # Convenience functions for compile + run workflow
    krun() {
        if [ -z "$1" ]; then
            echo "Usage: krun <file.kry> [renderer]"
            echo "Renderers: raylib (default), wgpu, ratatui, debug"
            return 1
        fi
        
        local kry_file="$1"
        local renderer="$2"
        if [ -z "$renderer" ]; then
            renderer="raylib"
        fi
        local krb_file=$(echo "$kry_file" | sed 's/\.kry$/.krb/')
        
        echo "🔨 Compiling $kry_file → $krb_file"
        kryc "$kry_file" "$krb_file" || return 1
        
        echo "🚀 Running with kryon-$renderer"
        case "$renderer" in
            raylib)   kryon-raylib "$krb_file" ;;
            wgpu)     kryon-wgpu "$krb_file" ;;
            ratatui)  kryon-ratatui "$krb_file" ;;
            debug)    kryon-debug "$krb_file" ;;
            *) echo "Unknown renderer: $renderer"; return 1 ;;
        esac
    }
    
    ktest() {
        if [ -z "$1" ]; then
            echo "Usage: ktest <file.kry>"
            echo "Tests with all backends: debug, ratatui, raylib"
            return 1
        fi
        
        local kry_file="$1"
        local krb_file=$(echo "$kry_file" | sed 's/\.kry$/.krb/')
        
        echo "🔨 Compiling $kry_file → $krb_file"
        kryc "$kry_file" "$krb_file" || return 1
        
        echo "🧪 Testing with debug backend:"
        kryon-debug "$krb_file"
        echo ""
        echo "🧪 Testing with ratatui backend:"
        kryon-ratatui "$krb_file"
        echo ""
        echo "🧪 Testing with raylib backend:"
        kryon-raylib "$krb_file"
    }
    
    kwatch() {
        if [ -z "$1" ]; then
            echo "Usage: kwatch <file.kry> [renderer]"
            echo "Automatically recompiles and runs when file changes"
            return 1
        fi
        
        local kry_file="$1"
        local renderer="$2"
        if [ -z "$renderer" ]; then
            renderer="raylib"
        fi
        
        echo "👁️  Watching $kry_file for changes (Ctrl+C to stop)"
        echo "Press Enter to trigger initial compile + run"
        
        while true; do
            read -t 1 && krun "$kry_file" "$renderer"
            local krb_file=$(echo "$kry_file" | sed 's/\.kry$/.krb/')
            if [ -f "$kry_file" ] && [ "$kry_file" -nt "$krb_file" ]; then
                echo "🔄 File changed, recompiling..."
                krun "$kry_file" "$renderer"
            fi
        done
    }
    
    echo "🚀 KRYON MONOREPO COMMANDS:"
    echo ""
    echo "⚡ QUICK WORKFLOW:"
    echo "  krun <file.kry> [renderer]  - Compile + run (raylib default)"
    echo "  ktest <file.kry>            - Test with all backends"
    echo "  kwatch <file.kry>           - Auto-recompile on changes"
    echo ""
    echo "📦 COMPILER:"
    echo "  kryc input.kry [output.krb] - Compile KRY to KRB"
    echo ""
    echo "🎨 RENDERERS:"
    echo "  kryon-raylib <file.krb>     - Run with Raylib backend"
    echo "  kryon-wgpu <file.krb>       - Run with WGPU backend"
    echo "  kryon-ratatui <file.krb>    - Run with Ratatui backend"
    echo "  kryon-debug <file.krb>      - Run with debug backend"
    echo ""
    echo "🔧 TOOLS:"
    echo "  kryon-bundle <file.krb>     - Bundle into executable"
    echo ""
    echo "📝 EXAMPLES:"
    echo "  krun examples/03_Buttons_and_Events/button.kry"
    echo "  krun examples/01_Getting_Started/hello_world.kry wgpu"
    echo "  ktest examples/04_Layout/flexbox_demo.kry"
    echo "  kwatch examples/05_Scripting/counter.kry"
    echo ""
  '';
}