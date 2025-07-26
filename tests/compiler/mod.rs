//! Compiler test suite for Kryon

mod parser_tests;
mod semantic_tests;
mod codegen_tests;
mod property_tests;

use std::path::Path;
use kryon_compiler::{compile_file, CompilerOptions};
use tempfile::TempDir;
use assert_fs::prelude::*;

#[test]
fn test_compile_basic_app() {
    let temp_dir = TempDir::new().unwrap();
    let input_path = temp_dir.path().join("test.kry");
    
    std::fs::write(&input_path, r#"
        App {
            window_width: 800
            window_height: 600
            background_color: "#000000ff"
            
            Text {
                text: "Hello World"
                text_color: "#ffffffff"
            }
        }
    "#).unwrap();
    
    let options = CompilerOptions::default();
    let result = compile_file(&input_path, &options);
    
    assert!(result.is_ok(), "Compilation should succeed");
    
    let output_path = input_path.with_extension("krb");
    assert!(output_path.exists(), "Output file should exist");
    
    // Verify binary format
    let krb_data = std::fs::read(&output_path).unwrap();
    assert!(krb_data.starts_with(b"KRB1"), "Should have KRB1 magic header");
}

#[test]
fn test_compile_with_errors() {
    let temp_dir = TempDir::new().unwrap();
    let input_path = temp_dir.path().join("error.kry");
    
    std::fs::write(&input_path, r#"
        App {
            window_width: "not a number"  // This should fail
            background_color: "#000000ff"
        }
    "#).unwrap();
    
    let options = CompilerOptions::default();
    let result = compile_file(&input_path, &options);
    
    assert!(result.is_err(), "Compilation should fail with type error");
}

#[test]
fn test_property_validation() {
    let temp_dir = TempDir::new().unwrap();
    let input_path = temp_dir.path().join("properties.kry");
    
    std::fs::write(&input_path, r#"
        App {
            window_width: 800
            window_height: 600
            
            Container {
                display: flex
                flex_direction: column
                align_items: end      // Should compile to correct value
                justify_content: center
                
                width: 100%           // Percentage support
                height: 50%
                padding: 20
                gap: 10
                
                background_color: "#ff0000ff"
                border_width: 2
                border_color: "#000000ff"
                border_radius: 8
            }
        }
    "#).unwrap();
    
    let options = CompilerOptions::default();
    let result = compile_file(&input_path, &options);
    
    assert!(result.is_ok(), "All properties should be valid");
}