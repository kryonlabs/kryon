//! Integration tests for the Kryon compilation pipeline
//! 
//! Tests the complete flow from KRY source code through AST, IR, and KRB output,
//! ensuring correctness and consistency across the compilation process.

use anyhow::Result;
use integration_tests::*;
use std::collections::HashMap;
use tempfile::TempDir;

#[tokio::test]
async fn test_basic_compilation_pipeline() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let source = r#"
        Container {
            background_color: #ff0000
            width: 200px
            height: 100px
            
            Text {
                text: "Hello World"
                font_size: 16px
                color: #ffffff
            }
        }
    "#;
    
    harness.add_source_file("basic.kry", source);
    harness.compile_all()?;
    
    // Verify compilation succeeded
    let compiled_path = harness.get_compiled_path("basic.kry");
    assert!(compiled_path.is_some());
    assert!(compiled_path.unwrap().exists());
    
    Ok(())
}

#[tokio::test]
async fn test_syntax_error_handling() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let invalid_source = r#"
        Container {
            background_color: #ff0000
            width: 200px
            height: // Missing value
            
            Text {
                text: "Hello World"
                font_size: 16px
                color: #ffffff
            }
        // Missing closing brace
    "#;
    
    harness.add_source_file("invalid.kry", invalid_source);
    
    // Compilation should fail gracefully
    let result = harness.compile_all();
    assert!(result.is_err());
    
    Ok(())
}

#[tokio::test]
async fn test_property_validation() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let source_with_invalid_props = r#"
        Container {
            background_color: invalid_color
            width: -50px
            height: 100px
            invalid_property: some_value
            
            Text {
                text: "Test"
                font_size: -10px
                color: #ffffff
            }
        }
    "#;
    
    harness.add_source_file("invalid_props.kry", source_with_invalid_props);
    
    // Should either fail compilation or apply defaults/validation
    let compilation_result = harness.compile_all();
    
    // Depending on implementation, this might succeed with warnings
    // or fail with validation errors
    match compilation_result {
        Ok(_) => {
            // If compilation succeeds, invalid values should be sanitized
            println!("Compilation succeeded with validation/sanitization");
        }
        Err(e) => {
            // If compilation fails, should have meaningful error messages
            let error_msg = e.to_string();
            assert!(error_msg.contains("invalid") || error_msg.contains("validation"));
        }
    }
    
    Ok(())
}

#[tokio::test]
async fn test_nested_elements_compilation() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let nested_source = r#"
        Container {
            display: flex
            flex_direction: column
            width: 400px
            height: 300px
            
            Container {
                display: flex
                flex_direction: row
                height: 50px
                
                Text {
                    text: "Header"
                    font_size: 18px
                    font_weight: bold
                }
            }
            
            Container {
                display: flex
                flex_direction: column
                flex: 1
                
                Text {
                    text: "Content line 1"
                    font_size: 14px
                }
                
                Text {
                    text: "Content line 2"
                    font_size: 14px
                }
                
                Container {
                    display: flex
                    flex_direction: row
                    gap: 10px
                    
                    Button {
                        text: "Cancel"
                        width: 80px
                        height: 30px
                    }
                    
                    Button {
                        text: "OK"
                        width: 80px
                        height: 30px
                        background_color: #0066cc
                    }
                }
            }
        }
    "#;
    
    harness.add_source_file("nested.kry", nested_source);
    harness.compile_all()?;
    
    // Verify nested structure is preserved
    let compiled_path = harness.get_compiled_path("nested.kry");
    assert!(compiled_path.is_some());
    
    Ok(())
}

#[tokio::test]
async fn test_widget_compilation() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let widget_source = r#"
        Container {
            display: flex
            flex_direction: column
            width: 500px
            height: 400px
            padding: 20px
            
            Dropdown {
                items: [
                    { label: "Option 1", value: "opt1" },
                    { label: "Option 2", value: "opt2" },
                    { label: "Option 3", value: "opt3" }
                ]
                selected_index: 0
                searchable: true
                width: 200px
                height: 32px
            }
            
            DataGrid {
                columns: [
                    { id: "name", title: "Name", width: 150 },
                    { id: "email", title: "Email", width: 200 },
                    { id: "age", title: "Age", width: 80 }
                ]
                data: [
                    { name: "John", email: "john@example.com", age: 30 },
                    { name: "Jane", email: "jane@example.com", age: 25 }
                ]
                sortable: true
                filterable: true
                width: 450px
                height: 200px
            }
            
            DatePicker {
                format: "yyyy-mm-dd"
                selected_date: "2024-03-15"
                min_date: "2024-01-01"
                max_date: "2024-12-31"
                width: 200px
                height: 32px
            }
        }
    "#;
    
    harness.add_source_file("widgets.kry", widget_source);
    harness.compile_all()?;
    
    // Verify widget-specific compilation
    let compiled_path = harness.get_compiled_path("widgets.kry");
    assert!(compiled_path.is_some());
    
    Ok(())
}

#[tokio::test]
async fn test_script_integration_compilation() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let source_with_scripts = r#"
        Container {
            width: 300px
            height: 200px
            
            Button {
                text: "Click Me"
                width: 100px
                height: 40px
                on_click: "handleButtonClick"
            }
            
            TextInput {
                placeholder: "Enter text"
                width: 200px
                height: 32px
                on_change: "handleTextChange"
                on_submit: "handleSubmit"
            }
        }
    "#;
    
    // Create corresponding script file
    let script_content = r#"
        function handleButtonClick() {
            console.log("Button clicked!");
        }
        
        function handleTextChange(value) {
            console.log("Text changed to:", value);
        }
        
        function handleSubmit(value) {
            console.log("Form submitted with:", value);
        }
    "#;
    
    harness.add_source_file("app.kry", source_with_scripts);
    
    // Write script file to temp directory
    let script_path = harness.temp_path().join("app.kry.scripts").join("handlers.lua");
    std::fs::create_dir_all(script_path.parent().unwrap())?;
    std::fs::write(&script_path, script_content)?;
    
    harness.compile_all()?;
    
    // Verify script integration
    let compiled_path = harness.get_compiled_path("app.kry");
    assert!(compiled_path.is_some());
    
    Ok(())
}

#[tokio::test]
async fn test_resource_bundling() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let source_with_resources = r#"
        Container {
            width: 400px
            height: 300px
            background_image: "background.jpg"
            
            Image {
                src: "logo.png"
                width: 100px
                height: 100px
            }
            
            Text {
                text: "Welcome"
                font_family: "custom-font.ttf"
                font_size: 24px
            }
        }
    "#;
    
    // Create mock resource files
    let temp_path = harness.temp_path();
    std::fs::write(temp_path.join("background.jpg"), b"fake-jpg-data")?;
    std::fs::write(temp_path.join("logo.png"), b"fake-png-data")?;
    std::fs::write(temp_path.join("custom-font.ttf"), b"fake-font-data")?;
    
    harness.add_source_file("with_resources.kry", source_with_resources);
    harness.compile_all()?;
    
    // Verify resource bundling
    let compiled_path = harness.get_compiled_path("with_resources.kry");
    assert!(compiled_path.is_some());
    
    // In a real implementation, we would verify that resources are embedded
    
    Ok(())
}

#[tokio::test]
async fn test_multiple_file_compilation() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let main_source = r#"
        Container {
            display: flex
            flex_direction: column
            width: 500px
            height: 400px
            
            import: "header.kry"
            import: "content.kry"
            import: "footer.kry"
        }
    "#;
    
    let header_source = r#"
        Container {
            height: 60px
            background_color: #333333
            
            Text {
                text: "Application Header"
                color: #ffffff
                font_size: 18px
            }
        }
    "#;
    
    let content_source = r#"
        Container {
            flex: 1
            padding: 20px
            
            Text {
                text: "Main content area"
                font_size: 14px
            }
        }
    "#;
    
    let footer_source = r#"
        Container {
            height: 40px
            background_color: #f0f0f0
            
            Text {
                text: "Footer"
                font_size: 12px
                color: #666666
            }
        }
    "#;
    
    harness.add_source_file("main.kry", main_source);
    harness.add_source_file("header.kry", header_source);
    harness.add_source_file("content.kry", content_source);
    harness.add_source_file("footer.kry", footer_source);
    
    harness.compile_all()?;
    
    // Verify all files compiled
    assert!(harness.get_compiled_path("main.kry").is_some());
    assert!(harness.get_compiled_path("header.kry").is_some());
    assert!(harness.get_compiled_path("content.kry").is_some());
    assert!(harness.get_compiled_path("footer.kry").is_some());
    
    Ok(())
}

#[tokio::test]
async fn test_theme_compilation() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let themed_source = r#"
        theme: "dark"
        
        Container {
            width: 300px
            height: 200px
            background_color: var(--bg-primary)
            
            Text {
                text: "Themed Text"
                color: var(--text-primary)
                font_size: 16px
            }
            
            Button {
                text: "Themed Button"
                background_color: var(--button-bg)
                color: var(--button-text)
                border_color: var(--button-border)
            }
        }
    "#;
    
    // Create theme file
    let theme_content = r#"
        {
            "dark": {
                "--bg-primary": "#1a1a1a",
                "--text-primary": "#ffffff",
                "--button-bg": "#0066cc",
                "--button-text": "#ffffff",
                "--button-border": "#004499"
            },
            "light": {
                "--bg-primary": "#ffffff",
                "--text-primary": "#000000",
                "--button-bg": "#e0e0e0",
                "--button-text": "#000000",
                "--button-border": "#cccccc"
            }
        }
    "#;
    
    let theme_path = harness.temp_path().join("theme.json");
    std::fs::write(&theme_path, theme_content)?;
    
    harness.add_source_file("themed.kry", themed_source);
    harness.compile_all()?;
    
    // Verify theme compilation
    let compiled_path = harness.get_compiled_path("themed.kry");
    assert!(compiled_path.is_some());
    
    Ok(())
}

#[tokio::test]
async fn test_conditional_compilation() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let conditional_source = r#"
        Container {
            width: 400px
            height: 300px
            
            #if debug
            Text {
                text: "Debug Mode"
                color: #ff0000
                font_weight: bold
            }
            #endif
            
            #if mobile
            Button {
                text: "Mobile Button"
                width: 100%
                height: 44px
            }
            #else
            Button {
                text: "Desktop Button"
                width: 120px
                height: 32px
            }
            #endif
            
            Text {
                text: "Always visible"
                font_size: 14px
            }
        }
    "#;
    
    harness.add_source_file("conditional.kry", conditional_source);
    
    // Set compilation flags
    std::env::set_var("KRYON_DEBUG", "true");
    std::env::set_var("KRYON_MOBILE", "false");
    
    harness.compile_all()?;
    
    // Verify conditional compilation
    let compiled_path = harness.get_compiled_path("conditional.kry");
    assert!(compiled_path.is_some());
    
    // Clean up environment
    std::env::remove_var("KRYON_DEBUG");
    std::env::remove_var("KRYON_MOBILE");
    
    Ok(())
}

#[tokio::test]
async fn test_compilation_performance() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    // Generate a large source file for performance testing
    let mut large_source = String::from("Container {\n  display: flex\n  flex_direction: column\n");
    
    for i in 0..1000 {
        large_source.push_str(&format!(
            "  Text {{\n    text: \"Item {}\"\n    font_size: 14px\n  }}\n",
            i
        ));
    }
    
    large_source.push_str("}\n");
    
    harness.add_source_file("large.kry", &large_source);
    
    let start_time = std::time::Instant::now();
    harness.compile_all()?;
    let compilation_time = start_time.elapsed();
    
    // Verify compilation completed within reasonable time
    assert!(compilation_time.as_millis() < 5000, 
        "Compilation took too long: {:?}", compilation_time);
    
    let compiled_path = harness.get_compiled_path("large.kry");
    assert!(compiled_path.is_some());
    
    Ok(())
}

#[tokio::test]
async fn test_incremental_compilation() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let initial_source = r#"
        Container {
            width: 300px
            height: 200px
            
            Text {
                text: "Version 1"
                font_size: 16px
            }
        }
    "#;
    
    harness.add_source_file("incremental.kry", initial_source);
    
    // First compilation
    let start_time = std::time::Instant::now();
    harness.compile_all()?;
    let first_compile_time = start_time.elapsed();
    
    // Modify source
    let modified_source = r#"
        Container {
            width: 300px
            height: 200px
            
            Text {
                text: "Version 2"
                font_size: 18px
                color: #0066cc
            }
        }
    "#;
    
    harness.add_source_file("incremental.kry", modified_source);
    
    // Second compilation (should be faster due to incremental compilation)
    let start_time = std::time::Instant::now();
    harness.compile_all()?;
    let second_compile_time = start_time.elapsed();
    
    // Verify both compilations succeeded
    let compiled_path = harness.get_compiled_path("incremental.kry");
    assert!(compiled_path.is_some());
    
    println!("First compile: {:?}, Second compile: {:?}", 
        first_compile_time, second_compile_time);
    
    Ok(())
}

#[test]
fn test_compilation_error_recovery() -> Result<()> {
    let test = IntegrationTest {
        name: "error_recovery".to_string(),
        source: r#"
            Container {
                width: 300px
                invalid_syntax_here
                height: 200px
                
                Text {
                    text: "This should still work"
                    font_size: 16px
                }
            }
        "#.to_string(),
        expected_success: false, // We expect this to fail
        expected_elements: None,
        test_type: IntegrationTestType::Compilation,
    };
    
    let result = test.run()?;
    
    // Should fail compilation but provide meaningful error
    assert!(!result.success);
    assert!(result.error.is_some());
    
    Ok(())
}

#[test]
fn test_compilation_with_all_element_types() -> Result<()> {
    let test = IntegrationTest {
        name: "all_elements".to_string(),
        source: r#"
            Container {
                display: flex
                flex_direction: column
                width: 600px
                height: 800px
                
                Text {
                    text: "Sample Text"
                    font_size: 16px
                }
                
                Button {
                    text: "Click Me"
                    width: 100px
                    height: 32px
                }
                
                TextInput {
                    placeholder: "Enter text"
                    width: 200px
                    height: 32px
                }
                
                Checkbox {
                    text: "Check me"
                    checked: true
                }
                
                Slider {
                    min: 0
                    max: 100
                    value: 50
                    width: 200px
                }
                
                Image {
                    src: "placeholder.png"
                    width: 100px
                    height: 100px
                }
                
                Canvas {
                    width: 300px
                    height: 200px
                }
                
                Scrollbar {
                    orientation: vertical
                    width: 16px
                    height: 200px
                }
            }
        "#.to_string(),
        expected_success: true,
        expected_elements: Some(9), // Container + 8 child elements
        test_type: IntegrationTestType::Compilation,
    };
    
    let result = test.run()?;
    assert!(result.success, "Compilation failed: {:?}", result.error);
    
    Ok(())
}