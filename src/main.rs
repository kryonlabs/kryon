//! Kryon Compiler Binary

use kryon_compiler::{compile_file, CompilerError};
use std::env;
use std::process;

const NAME: &str = "kryon";
const VERSION: &str = "0.1.0";

fn main() {
    
    let args: Vec<String> = env::args().collect();
    
    if args.len() < 2 || args.len() > 3 {
        eprintln!("Usage: {} <input.kry> [output.krb]", args[0]);
        eprintln!("  {NAME} v{VERSION} - Kryon UI Language Compiler");
        eprintln!("  Compiles KRY source files to optimized KRB binary format");
        eprintln!("  If output file is not specified, it will be auto-generated");
        process::exit(1);
    }
    
    let input_file = &args[1];
    let output_file = if args.len() == 3 {
        args[2].clone()
    } else {
        // Auto-generate output filename
        if input_file.ends_with(".kry") {
            input_file.replace(".kry", ".krb")
        } else {
            format!("{}.krb", input_file)
        }
    };
    
    println!("{NAME} v{VERSION}");
    println!("Compiling '{}' to '{}'...", input_file, output_file);
    
    match compile_file(input_file, &output_file) {
        Ok(stats) => {
            println!("Compilation successful!");
            println!("Output size: {} bytes", stats.output_size);
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