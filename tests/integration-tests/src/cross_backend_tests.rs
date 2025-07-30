//! Cross-backend consistency tests for Kryon
//! 
//! Ensures that the same KRY source produces consistent results across
//! different rendering backends (Ratatui, HTML, Raylib, WGPU).

use anyhow::Result;
use integration_tests::*;
use std::collections::HashMap;

#[tokio::test]
async fn test_basic_cross_backend_consistency() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let source = r#"
        Container {
            width: 300px
            height: 200px
            background_color: #ff0000
            
            Text {
                text: "Cross-Backend Test"
                font_size: 16px
                color: #ffffff
                x: 50px
                y: 50px
            }
        }
    "#;
    
    harness.add_source_file("cross_backend.kry", source);
    harness.compile_all()?;
    
    // Run with all backends
    let cross_result = harness.cross_backend_test("cross_backend.kry")?;
    
    // Verify all backends succeeded
    for (backend, result) in &cross_result.results {
        assert!(result.success, "Backend {:?} failed: {}", backend, result.output);
    }
    
    // Check consistency
    assert!(cross_result.consistent, "Cross-backend results are not consistent");
    
    Ok(())
}

#[tokio::test]
async fn test_layout_consistency_across_backends() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let layout_source = r#"
        Container {
            display: flex
            flex_direction: row
            width: 500px
            height: 200px
            gap: 20px
            padding: 10px
            
            Container {
                flex: 1
                background_color: #ff0000
                display: flex
                align_items: center
                justify_content: center
                
                Text {
                    text: "Left"
                    color: #ffffff
                    font_size: 18px
                }
            }
            
            Container {
                flex: 2
                background_color: #00ff00
                display: flex
                align_items: center
                justify_content: center
                
                Text {
                    text: "Center"
                    color: #000000
                    font_size: 18px
                }
            }
            
            Container {
                width: 100px
                background_color: #0000ff
                display: flex
                align_items: center
                justify_content: center
                
                Text {
                    text: "Right"
                    color: #ffffff
                    font_size: 18px
                }
            }
        }
    "#;
    
    harness.add_source_file("layout_test.kry", layout_source);
    harness.compile_all()?;
    
    let cross_result = harness.cross_backend_test("layout_test.kry")?;
    
    // All backends should handle flexbox consistently
    for (backend, result) in &cross_result.results {
        assert!(result.success, "Layout failed on backend {:?}: {}", backend, result.output);
    }
    
    Ok(())
}

#[tokio::test]
async fn test_text_rendering_consistency() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let text_source = r#"
        Container {
            width: 400px
            height: 300px
            padding: 20px
            background_color: #ffffff
            
            Text {
                text: "Heading Text"
                font_size: 24px
                font_weight: bold
                color: #333333
                margin_bottom: 20px
            }
            
            Text {
                text: "This is a paragraph of text that should be rendered consistently across all backends. It includes multiple words and should wrap properly."
                font_size: 16px
                line_height: 1.5
                color: #666666
                text_align: justify
                max_width: 360px
                margin_bottom: 15px
            }
            
            Text {
                text: "Monospace Code Text"
                font_family: monospace
                font_size: 14px
                background_color: #f5f5f5
                padding: 8px
                border_radius: 4px
                color: #000000
            }
        }
    "#;
    
    harness.add_source_file("text_consistency.kry", text_source);
    harness.compile_all()?;
    
    let cross_result = harness.cross_backend_test("text_consistency.kry")?;
    
    // Text rendering should be consistent
    for (backend, result) in &cross_result.results {
        assert!(result.success, "Text rendering failed on backend {:?}: {}", backend, result.output);
    }
    
    Ok(())
}

#[tokio::test]
async fn test_color_consistency() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let color_source = r#"
        Container {
            width: 400px
            height: 300px
            display: flex
            flex_direction: row
            flex_wrap: wrap
            
            Container {
                width: 100px
                height: 100px
                background_color: #ff0000
            }
            
            Container {
                width: 100px
                height: 100px
                background_color: #00ff00
            }
            
            Container {
                width: 100px
                height: 100px
                background_color: #0000ff
            }
            
            Container {
                width: 100px
                height: 100px
                background_color: rgba(255, 255, 0, 0.5)
            }
            
            Container {
                width: 100px
                height: 100px
                background_color: hsl(180, 100%, 50%)
            }
            
            Container {
                width: 100px
                height: 100px
                background_color: #663399
            }
        }
    "#;
    
    harness.add_source_file("colors.kry", color_source);
    harness.compile_all()?;
    
    let cross_result = harness.cross_backend_test("colors.kry")?;
    
    // Color representation should be consistent
    for (backend, result) in &cross_result.results {
        assert!(result.success, "Color rendering failed on backend {:?}: {}", backend, result.output);
    }
    
    Ok(())
}

#[tokio::test]
async fn test_input_control_consistency() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let input_source = r#"
        Container {
            width: 500px
            height: 400px
            padding: 20px
            display: flex
            flex_direction: column
            gap: 20px
            
            TextInput {
                width: 300px
                height: 32px
                placeholder: "Enter text here"
                border_width: 1px
                border_color: #cccccc
                border_radius: 4px
                padding: 8px
            }
            
            Button {
                text: "Click Me"
                width: 120px
                height: 36px
                background_color: #0066cc
                color: #ffffff
                border_radius: 4px
                font_size: 14px
            }
            
            Checkbox {
                text: "Check this option"
                checked: false
                font_size: 14px
            }
            
            Slider {
                width: 200px
                height: 24px
                min: 0
                max: 100
                value: 50
                track_color: #e0e0e0
                thumb_color: #0066cc
            }
            
            Container {
                display: flex
                flex_direction: row
                gap: 10px
                
                RadioButton {
                    text: "Option A"
                    name: "options"
                    value: "a"
                    checked: true
                }
                
                RadioButton {
                    text: "Option B"
                    name: "options"
                    value: "b"
                    checked: false
                }
                
                RadioButton {
                    text: "Option C"
                    name: "options"
                    value: "c"
                    checked: false
                }
            }
        }
    "#;
    
    harness.add_source_file("inputs.kry", input_source);
    harness.compile_all()?;
    
    let cross_result = harness.cross_backend_test("inputs.kry")?;
    
    // Input controls should render consistently
    for (backend, result) in &cross_result.results {
        assert!(result.success, "Input controls failed on backend {:?}: {}", backend, result.output);
    }
    
    Ok(())
}

#[tokio::test]
async fn test_animation_consistency() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let animation_source = r#"
        Container {
            width: 400px
            height: 300px
            background_color: #f0f0f0
            
            Container {
                width: 100px
                height: 100px
                background_color: #0066cc
                x: 50px
                y: 50px
                border_radius: 50px
                
                animations: {
                    "rotate": {
                        duration: 3000ms
                        timing_function: linear
                        iteration_count: infinite
                        keyframes: {
                            "0%": { transform: rotate(0deg) }
                            "100%": { transform: rotate(360deg) }
                        }
                    }
                }
                
                animation: rotate
            }
            
            Container {
                width: 80px
                height: 80px
                background_color: #ff6600
                x: 200px
                y: 100px
                
                animations: {
                    "bounce": {
                        duration: 2000ms
                        timing_function: ease-in-out
                        iteration_count: infinite
                        alternate: true
                        keyframes: {
                            "0%": { transform: translateY(0px) }
                            "100%": { transform: translateY(-50px) }
                        }
                    }
                }
                
                animation: bounce
            }
        }
    "#;
    
    harness.add_source_file("animations.kry", animation_source);
    harness.compile_all()?;
    
    let cross_result = harness.cross_backend_test("animations.kry")?;
    
    // Animation setup should be consistent (actual animation may vary by backend)
    for (backend, result) in &cross_result.results {
        assert!(result.success, "Animation setup failed on backend {:?}: {}", backend, result.output);
    }
    
    Ok(())
}

#[tokio::test]
async fn test_responsive_consistency() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let responsive_source = r#"
        Container {
            width: 100%
            height: 100vh
            display: flex
            
            media_queries: {
                "(max-width: 768px)": {
                    flex_direction: column
                },
                "(min-width: 769px)": {
                    flex_direction: row
                }
            }
            
            Container {
                background_color: #ff0000
                
                media_queries: {
                    "(max-width: 768px)": {
                        width: 100%
                        height: 200px
                    },
                    "(min-width: 769px)": {
                        width: 300px
                        height: 100%
                    }
                }
                
                Text {
                    text: "Responsive Panel"
                    color: #ffffff
                    font_size: 18px
                }
            }
            
            Container {
                flex: 1
                background_color: #00ff00
                padding: 20px
                
                Text {
                    text: "Main content area that adapts to screen size"
                    font_size: 16px
                    line_height: 1.5
                }
            }
        }
    "#;
    
    harness.add_source_file("responsive.kry", responsive_source);
    harness.compile_all()?;
    
    let cross_result = harness.cross_backend_test("responsive.kry")?;
    
    // Responsive behavior should be consistent
    for (backend, result) in &cross_result.results {
        assert!(result.success, "Responsive layout failed on backend {:?}: {}", backend, result.output);
    }
    
    Ok(())
}

#[tokio::test]
async fn test_complex_nested_consistency() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let complex_source = r#"
        Container {
            width: 800px
            height: 600px
            display: flex
            flex_direction: column
            
            Container {
                height: 80px
                background_color: #333333
                display: flex
                align_items: center
                padding: 0px 20px
                
                Container {
                    width: 60px
                    height: 40px
                    background_color: #0066cc
                    border_radius: 4px
                    display: flex
                    align_items: center
                    justify_content: center
                    
                    Text {
                        text: "LOGO"
                        color: #ffffff
                        font_size: 12px
                        font_weight: bold
                    }
                }
                
                Container {
                    flex: 1
                    display: flex
                    justify_content: center
                    
                    Container {
                        display: flex
                        gap: 30px
                        
                        Text {
                            text: "Home"
                            color: #ffffff
                            font_size: 14px
                            cursor: pointer
                        }
                        
                        Text {
                            text: "About"
                            color: #cccccc
                            font_size: 14px
                            cursor: pointer
                        }
                        
                        Text {
                            text: "Contact"
                            color: #cccccc
                            font_size: 14px
                            cursor: pointer
                        }
                    }
                }
                
                Button {
                    text: "Login"
                    width: 80px
                    height: 32px
                    background_color: #0066cc
                    color: #ffffff
                    border_radius: 4px
                    font_size: 14px
                }
            }
            
            Container {
                flex: 1
                display: flex
                
                Container {
                    width: 250px
                    background_color: #f8f9fa
                    padding: 20px
                    
                    Text {
                        text: "Sidebar"
                        font_size: 18px
                        font_weight: bold
                        margin_bottom: 20px
                    }
                    
                    Container {
                        display: flex
                        flex_direction: column
                        gap: 10px
                        
                        Container {
                            padding: 10px
                            background_color: #ffffff
                            border_radius: 4px
                            border: 1px solid #e0e0e0
                            cursor: pointer
                            
                            Text {
                                text: "Menu Item 1"
                                font_size: 14px
                            }
                        }
                        
                        Container {
                            padding: 10px
                            background_color: #0066cc
                            border_radius: 4px
                            cursor: pointer
                            
                            Text {
                                text: "Menu Item 2"
                                color: #ffffff
                                font_size: 14px
                            }
                        }
                        
                        Container {
                            padding: 10px
                            background_color: #ffffff
                            border_radius: 4px
                            border: 1px solid #e0e0e0
                            cursor: pointer
                            
                            Text {
                                text: "Menu Item 3"
                                font_size: 14px
                            }
                        }
                    }
                }
                
                Container {
                    flex: 1
                    padding: 40px
                    
                    Text {
                        text: "Main Content Area"
                        font_size: 32px
                        font_weight: bold
                        margin_bottom: 20px
                        color: #333333
                    }
                    
                    Text {
                        text: "This is a complex nested layout that should render consistently across all backends. It includes multiple levels of containers with various styling properties."
                        font_size: 16px
                        line_height: 1.6
                        color: #666666
                        max_width: 400px
                        margin_bottom: 30px
                    }
                    
                    Container {
                        display: flex
                        gap: 20px
                        
                        Container {
                            width: 150px
                            height: 100px
                            background_color: #ff6b6b
                            border_radius: 8px
                            display: flex
                            align_items: center
                            justify_content: center
                            
                            Text {
                                text: "Card 1"
                                color: #ffffff
                                font_size: 16px
                                font_weight: bold
                            }
                        }
                        
                        Container {
                            width: 150px
                            height: 100px
                            background_color: #4ecdc4
                            border_radius: 8px
                            display: flex
                            align_items: center
                            justify_content: center
                            
                            Text {
                                text: "Card 2"
                                color: #ffffff
                                font_size: 16px
                                font_weight: bold
                            }
                        }
                        
                        Container {
                            width: 150px
                            height: 100px
                            background_color: #45b7d1
                            border_radius: 8px
                            display: flex
                            align_items: center
                            justify_content: center
                            
                            Text {
                                text: "Card 3"
                                color: #ffffff
                                font_size: 16px
                                font_weight: bold
                            }
                        }
                    }
                }
            }
        }
    "#;
    
    harness.add_source_file("complex_nested.kry", complex_source);
    harness.compile_all()?;
    
    let cross_result = harness.cross_backend_test("complex_nested.kry")?;
    
    // Complex nested layouts should be consistent
    for (backend, result) in &cross_result.results {
        assert!(result.success, "Complex layout failed on backend {:?}: {}", backend, result.output);
    }
    
    assert!(cross_result.consistent, "Complex nested layout is not consistent across backends");
    
    Ok(())
}

#[tokio::test]
async fn test_performance_consistency() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    // Generate performance test with many elements
    let mut performance_source = String::from(
        "Container {\n  display: flex\n  flex_direction: column\n  width: 800px\n  height: 600px\n"
    );
    
    for i in 0..200 {
        performance_source.push_str(&format!(
            "  Container {{\n    display: flex\n    height: 30px\n    background_color: #{:06x}\n    align_items: center\n    Text {{\n      text: \"Performance Item {}\"\n      font_size: 14px\n      margin_left: 10px\n    }}\n  }}\n",
            (i * 1234) % 0xFFFFFF,
            i
        ));
    }
    
    performance_source.push_str("}\n");
    
    harness.add_source_file("performance.kry", &performance_source);
    harness.compile_all()?;
    
    let cross_result = harness.cross_backend_test("performance.kry")?;
    
    // All backends should handle large UIs
    for (backend, result) in &cross_result.results {
        assert!(result.success, "Performance test failed on backend {:?}: {}", backend, result.output);
        
        // Check execution time is reasonable
        assert!(result.execution_time.as_millis() < 2000,
            "Backend {:?} took too long: {:?}", backend, result.execution_time);
    }
    
    Ok(())
}

#[tokio::test]
async fn test_backend_specific_features() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let backend_specific_source = r#"
        Container {
            width: 600px
            height: 400px
            
            Canvas {
                width: 400px
                height: 300px
                background_color: #f0f0f0
                
                backend_specific: {
                    html: {
                        use_canvas_api: true
                    },
                    wgpu: {
                        enable_antialiasing: true
                        sample_count: 4
                    },
                    raylib: {
                        enable_vsync: true
                    },
                    ratatui: {
                        use_unicode_chars: true
                    }
                }
                
                draw_commands: [
                    {
                        type: "rect"
                        x: 50
                        y: 50
                        width: 100
                        height: 75
                        fill: "#0066cc"
                    }
                ]
            }
        }
    "#;
    
    harness.add_source_file("backend_specific.kry", backend_specific_source);
    harness.compile_all()?;
    
    let cross_result = harness.cross_backend_test("backend_specific.kry")?;
    
    // All backends should handle backend-specific configurations gracefully
    for (backend, result) in &cross_result.results {
        assert!(result.success, "Backend-specific features failed on {:?}: {}", backend, result.output);
    }
    
    Ok(())
}

#[test]
fn test_cross_backend_widget_consistency() -> Result<()> {
    let test = IntegrationTest {
        name: "widget_consistency".to_string(),
        source: r#"
            Container {
                width: 600px
                height: 500px
                padding: 20px
                display: flex
                flex_direction: column
                gap: 20px
                
                Dropdown {
                    width: 200px
                    height: 32px
                    items: [
                        { label: "Item 1", value: "1" },
                        { label: "Item 2", value: "2" },
                        { label: "Item 3", value: "3" }
                    ]
                    placeholder: "Select item"
                }
                
                NumberInput {
                    width: 150px
                    height: 32px
                    min: 0
                    max: 100
                    step: 1
                    value: 42
                    precision: 0
                }
                
                DatePicker {
                    width: 180px
                    height: 32px
                    format: "yyyy-mm-dd"
                    value: "2024-03-15"
                }
                
                ColorPicker {
                    width: 200px
                    height: 150px
                    value: "#ff6b6b"
                    show_preview: true
                }
            }
        "#.to_string(),
        expected_success: true,
        expected_elements: Some(5), // Container + 4 widgets
        test_type: IntegrationTestType::CrossBackend,
    };
    
    let result = test.run()?;
    assert!(result.success, "Widget cross-backend test failed: {:?}", result.error);
    
    Ok(())
}

// Helper function to analyze cross-backend results
fn analyze_cross_backend_differences(results: &HashMap<TestBackend, TestRunResult>) -> Vec<String> {
    let mut differences = Vec::new();
    
    // Compare execution times
    let times: Vec<_> = results.values().map(|r| r.execution_time.as_millis()).collect();
    let min_time = times.iter().min().unwrap_or(&0);
    let max_time = times.iter().max().unwrap_or(&0);
    
    if max_time > min_time * 3 {
        differences.push(format!("Execution time varies significantly: {}ms to {}ms", min_time, max_time));
    }
    
    // Compare success rates
    let success_count = results.values().filter(|r| r.success).count();
    if success_count != results.len() && success_count > 0 {
        differences.push("Inconsistent success/failure across backends".to_string());
    }
    
    // Compare output lengths (rough approximation of render complexity)
    let output_lengths: Vec<_> = results.values().map(|r| r.output.len()).collect();
    let min_output = output_lengths.iter().min().unwrap_or(&0);
    let max_output = output_lengths.iter().max().unwrap_or(&0);
    
    if max_output > min_output * 2 && *min_output > 0 {
        differences.push("Significant differences in output complexity".to_string());
    }
    
    differences
}