//! Kryon CLI - Intelligent Compiler and Runner

use kryon_compiler::{compile_file, CompilerError};
use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::{self, Command};

const NAME: &str = "kryon";
const VERSION: &str = "0.1.0";

fn main() {
    let args: Vec<String> = env::args().collect();
    
    if args.len() < 2 {
        print_usage(&args[0]);
        process::exit(1);
    }

    let input_path = Path::new(&args[1]);
    
    // Handle different file types intelligently
    if input_path.extension().and_then(|s| s.to_str()) == Some("kry") {
        // Handle .kry files - compile and optionally run
        let should_run = args.len() > 2 && args[2] == "--run";
        handle_kry_file(input_path, should_run);
    } else if input_path.extension().and_then(|s| s.to_str()) == Some("krb") {
        // Handle .krb files - just run
        run_krb_file(input_path);
    } else {
        eprintln!("Error: Unsupported file type. Expected .kry or .krb");
        process::exit(1);
    }
}

fn print_usage(program_name: &str) {
    eprintln!("Usage:");
    eprintln!("  {} <file.kry> [--run]    Compile .kry file (and run if --run specified)", program_name);
    eprintln!("  {} <file.krb>            Run compiled .krb file", program_name);
    eprintln!();
    eprintln!("  {NAME} v{VERSION} - Kryon UI Language Intelligent CLI");
    eprintln!("  Automatically compiles KRY source files and manages build artifacts");
}

fn handle_kry_file(input_path: &Path, should_run: bool) {
    // Determine build directory and output paths
    let build_dir = get_build_directory(input_path);
    let output_file = build_dir.join(input_path.file_stem().unwrap()).with_extension("krb");
    
    println!("{NAME} v{VERSION}");
    println!("Compiling '{}' to '{}'...", input_path.display(), output_file.display());
    
    // Create build directory
    if let Err(e) = fs::create_dir_all(&build_dir) {
        eprintln!("Error creating build directory: {}", e);
        process::exit(1);
    }
    
    // Copy assets to build directory
    copy_assets_to_build(input_path, &build_dir);
    
    // Compile the .kry file
    match compile_file(input_path.to_str().unwrap(), output_file.to_str().unwrap()) {
        Ok(stats) => {
            println!("✓ Compilation successful!");
            println!("  Output: {}", output_file.display());
            println!("  Size: {} bytes", stats.output_size);
            
            if should_run {
                println!();
                run_krb_file(&output_file);
            }
        }
        Err(CompilerError::Io(e)) => {
            eprintln!("IO Error: {}", e);
            process::exit(1);
        }
        Err(e) => {
            eprintln!("Compilation failed: {}", e);
            process::exit(1);
        }
    }
}

fn get_build_directory(input_path: &Path) -> PathBuf {
    if let Some(parent) = input_path.parent() {
        // If in examples folder, use examples/.build
        if parent.to_string_lossy().contains("examples") {
            // Find the examples root
            let mut current = parent;
            while let Some(p) = current.parent() {
                if p.file_name().and_then(|s| s.to_str()) == Some("examples") {
                    return p.join(".build");
                }
                current = p;
            }
            // Fallback to examples/.build in current dir
            parent.join(".build")
        } else {
            // For other locations, use .build in the same directory
            parent.join(".build")
        }
    } else {
        PathBuf::from(".build")
    }
}

fn copy_assets_to_build(input_path: &Path, build_dir: &Path) {
    if let Some(source_dir) = input_path.parent() {
        // Copy common asset types
        let asset_extensions = ["ttf", "png", "jpg", "jpeg", "svg", "ico"];
        
        for extension in &asset_extensions {
            if let Ok(entries) = fs::read_dir(source_dir) {
                for entry in entries.flatten() {
                    let path = entry.path();
                    if path.extension().and_then(|s| s.to_str()) == Some(extension) {
                        let dest = build_dir.join(path.file_name().unwrap());
                        if let Err(e) = fs::copy(&path, &dest) {
                            println!("Warning: Failed to copy asset {}: {}", path.display(), e);
                        } else {
                            println!("  Copied asset: {}", path.file_name().unwrap().to_string_lossy());
                        }
                    }
                }
            }
        }
    }
}

fn run_krb_file(krb_path: &Path) {
    println!("Running '{}'...", krb_path.display());
    
    // Try to find the raylib renderer
    let renderer_paths = [
        "./target/release/kryon-renderer-raylib",
        "./target/debug/kryon-renderer-raylib", 
        "kryon-renderer-raylib",
    ];
    
    for renderer_path in &renderer_paths {
        if Path::new(renderer_path).exists() || Command::new(renderer_path).arg("--help").output().is_ok() {
            println!("Using renderer: {}", renderer_path);
            
            let mut cmd = Command::new(renderer_path);
            cmd.arg(krb_path);
            
            // Set working directory to the build directory so assets are found
            if let Some(build_dir) = krb_path.parent() {
                cmd.current_dir(build_dir);
            }
            
            match cmd.status() {
                Ok(status) => {
                    if !status.success() {
                        eprintln!("Renderer exited with code: {:?}", status.code());
                    }
                }
                Err(e) => {
                    eprintln!("Error running renderer: {}", e);
                    process::exit(1);
                }
            }
            return;
        }
    }
    
    eprintln!("Error: Could not find kryon-renderer-raylib");
    eprintln!("Please build the project first: cargo build --release");
    process::exit(1);
}