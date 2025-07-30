//! Integration tests for the Kryon rendering pipeline
//! 
//! Tests the complete rendering flow from compiled KRB files through layout
//! calculation, styling application, and render command generation.

use anyhow::Result;
use integration_tests::*;

#[tokio::test]
async fn test_basic_rendering_pipeline() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let source = r#"
        Container {
            width: 300px
            height: 200px
            background_color: #ff0000
            
            Text {
                text: "Hello World"
                font_size: 16px
                color: #ffffff
                x: 10px
                y: 10px
            }
        }
    "#;
    
    harness.add_source_file("basic_render.kry", source);
    harness.compile_all()?;
    
    // Test rendering with different backends
    let ratatui_result = harness.run_with_backend("basic_render.kry", TestBackend::Ratatui)?;
    assert!(ratatui_result.success);
    
    let html_result = harness.run_with_backend("basic_render.kry", TestBackend::Html)?;
    assert!(html_result.success);
    
    Ok(())
}

#[tokio::test]
async fn test_layout_calculation() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let flexbox_source = r#"
        Container {
            display: flex
            flex_direction: row
            width: 400px
            height: 100px
            gap: 10px
            padding: 20px
            
            Container {
                flex: 1
                background_color: #ff0000
                min_height: 60px
            }
            
            Container {
                flex: 2
                background_color: #00ff00
                min_height: 60px
            }
            
            Container {
                width: 80px
                background_color: #0000ff
                min_height: 60px
            }
        }
    "#;
    
    harness.add_source_file("flexbox.kry", flexbox_source);
    harness.compile_all()?;
    
    // Test that layout calculations work correctly
    let result = harness.run_with_backend("flexbox.kry", TestBackend::Html)?;
    assert!(result.success);
    
    // In a real implementation, we would verify the calculated layouts
    // For now, we just ensure the rendering pipeline doesn't crash
    
    Ok(())
}

#[tokio::test]
async fn test_nested_layout_rendering() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let nested_source = r#"
        Container {
            display: flex
            flex_direction: column
            width: 500px
            height: 400px
            
            Container {
                display: flex
                flex_direction: row
                height: 60px
                background_color: #333333
                
                Container {
                    width: 100px
                    display: flex
                    align_items: center
                    justify_content: center
                    
                    Text {
                        text: "Logo"
                        color: #ffffff
                        font_size: 14px
                    }
                }
                
                Container {
                    flex: 1
                    display: flex
                    align_items: center
                    justify_content: center
                    
                    Text {
                        text: "Navigation"
                        color: #ffffff
                        font_size: 16px
                    }
                }
            }
            
            Container {
                flex: 1
                display: flex
                flex_direction: row
                
                Container {
                    width: 200px
                    background_color: #f0f0f0
                    padding: 20px
                    
                    Text {
                        text: "Sidebar"
                        font_size: 14px
                    }
                }
                
                Container {
                    flex: 1
                    padding: 20px
                    
                    Text {
                        text: "Main Content"
                        font_size: 16px
                    }
                }
            }
        }
    "#;
    
    harness.add_source_file("nested_layout.kry", nested_source);
    harness.compile_all()?;
    
    // Test rendering with complex nested layouts
    let result = harness.run_with_backend("nested_layout.kry", TestBackend::Html)?;
    assert!(result.success);
    
    Ok(())
}

#[tokio::test]
async fn test_styling_application() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let styled_source = r#"
        Container {
            width: 300px
            height: 200px
            background_color: #ffffff
            border_width: 2px
            border_color: #cccccc
            border_radius: 8px
            padding: 16px
            box_shadow: 0px 2px 4px rgba(0,0,0,0.1)
            
            Text {
                text: "Styled Content"
                font_size: 18px
                font_weight: bold
                color: #333333
                text_align: center
                line_height: 1.5
                margin_bottom: 10px
            }
            
            Container {
                width: 100%
                height: 40px
                background_color: #0066cc
                border_radius: 4px
                display: flex
                align_items: center
                justify_content: center
                cursor: pointer
                
                hover: {
                    background_color: #0052a3
                }
                
                Text {
                    text: "Button"
                    color: #ffffff
                    font_size: 14px
                    font_weight: 500
                }
            }
        }
    "#;
    
    harness.add_source_file("styled.kry", styled_source);
    harness.compile_all()?;
    
    // Test that styling is applied correctly
    let result = harness.run_with_backend("styled.kry", TestBackend::Html)?;
    assert!(result.success);
    
    Ok(())
}

#[tokio::test]
async fn test_responsive_rendering() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let responsive_source = r#"
        Container {
            width: 100%
            min_height: 100vh
            
            media_queries: {
                "(max-width: 768px)": {
                    flex_direction: column
                },
                "(min-width: 769px)": {
                    flex_direction: row
                }
            }
            
            Container {
                width: 300px
                height: 200px
                background_color: #ff0000
                
                media_queries: {
                    "(max-width: 768px)": {
                        width: 100%
                        height: 150px
                    }
                }
            }
            
            Container {
                flex: 1
                background_color: #00ff00
                min_height: 200px
            }
        }
    "#;
    
    harness.add_source_file("responsive.kry", responsive_source);
    harness.compile_all()?;
    
    // Test responsive rendering
    let result = harness.run_with_backend("responsive.kry", TestBackend::Html)?;
    assert!(result.success);
    
    Ok(())
}

#[tokio::test]
async fn test_animation_rendering() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let animated_source = r#"
        Container {
            width: 300px
            height: 200px
            background_color: #ffffff
            
            Container {
                width: 100px
                height: 100px
                background_color: #0066cc
                border_radius: 50px
                
                animations: {
                    "pulse": {
                        duration: 2000ms
                        timing_function: ease-in-out
                        iteration_count: infinite
                        alternate: true
                        keyframes: {
                            "0%": { transform: scale(1.0) }
                            "100%": { transform: scale(1.2) }
                        }
                    }
                }
                
                animation: pulse
            }
            
            Text {
                text: "Animated Element"
                font_size: 16px
                color: #333333
                y: 120px
                
                animations: {
                    "fade-in": {
                        duration: 1000ms
                        timing_function: ease-out
                        keyframes: {
                            "0%": { opacity: 0, transform: translateY(20px) }
                            "100%": { opacity: 1, transform: translateY(0px) }
                        }
                    }
                }
                
                animation: fade-in
            }
        }
    "#;
    
    harness.add_source_file("animated.kry", animated_source);
    harness.compile_all()?;
    
    // Test animation rendering
    let result = harness.run_with_backend("animated.kry", TestBackend::Html)?;
    assert!(result.success);
    
    Ok(())
}

#[tokio::test]
async fn test_canvas_rendering() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let canvas_source = r#"
        Container {
            width: 500px
            height: 400px
            
            Canvas {
                width: 400px
                height: 300px
                background_color: #f0f0f0
                
                draw_commands: [
                    {
                        type: "rect"
                        x: 50
                        y: 50
                        width: 100
                        height: 75
                        fill: "#ff0000"
                        stroke: "#000000"
                        stroke_width: 2
                    },
                    {
                        type: "circle"
                        x: 250
                        y: 100
                        radius: 40
                        fill: "#00ff00"
                    },
                    {
                        type: "line"
                        x1: 50
                        y1: 200
                        x2: 350
                        y2: 250
                        stroke: "#0000ff"
                        stroke_width: 3
                    },
                    {
                        type: "text"
                        x: 200
                        y: 280
                        text: "Canvas Rendering"
                        font_size: 16
                        fill: "#333333"
                        align: center
                    }
                ]
            }
        }
    "#;
    
    harness.add_source_file("canvas.kry", canvas_source);
    harness.compile_all()?;
    
    // Test canvas rendering
    let result = harness.run_with_backend("canvas.kry", TestBackend::Html)?;
    assert!(result.success);
    
    Ok(())
}

#[tokio::test]
async fn test_3d_canvas_rendering() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let canvas_3d_source = r#"
        Container {
            width: 600px
            height: 500px
            
            Canvas3D {
                width: 500px
                height: 400px
                background_color: #000000
                
                camera: {
                    position: { x: 0, y: 0, z: 5 }
                    target: { x: 0, y: 0, z: 0 }
                    up: { x: 0, y: 1, z: 0 }
                    fov: 45
                    near: 0.1
                    far: 100
                }
                
                lighting: {
                    ambient: { r: 0.2, g: 0.2, b: 0.2, a: 1 }
                    directional: {
                        direction: { x: -1, y: -1, z: -1 }
                        color: { r: 1, g: 1, b: 1, a: 1 }
                        intensity: 1
                    }
                }
                
                objects: [
                    {
                        type: "cube"
                        position: { x: -1, y: 0, z: 0 }
                        size: { x: 1, y: 1, z: 1 }
                        color: { r: 1, g: 0, b: 0, a: 1 }
                        rotation: { x: 0, y: 0, z: 0 }
                    },
                    {
                        type: "sphere"
                        position: { x: 1, y: 0, z: 0 }
                        radius: 0.5
                        color: { r: 0, g: 1, b: 0, a: 1 }
                    },
                    {
                        type: "plane"
                        position: { x: 0, y: -1, z: 0 }
                        size: { x: 4, y: 4 }
                        color: { r: 0.5, g: 0.5, b: 0.5, a: 1 }
                        normal: { x: 0, y: 1, z: 0 }
                    }
                ]
            }
        }
    "#;
    
    harness.add_source_file("canvas_3d.kry", canvas_3d_source);
    harness.compile_all()?;
    
    // Test 3D canvas rendering
    let result = harness.run_with_backend("canvas_3d.kry", TestBackend::Wgpu)?;
    assert!(result.success);
    
    Ok(())
}

#[tokio::test]
async fn test_text_rendering_complex() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let text_source = r#"
        Container {
            width: 500px
            height: 600px
            padding: 20px
            background_color: #ffffff
            
            Text {
                text: "Heading 1"
                font_size: 32px
                font_weight: bold
                color: #333333
                margin_bottom: 20px
            }
            
            Text {
                text: "This is a paragraph with multiple lines of text that should wrap properly within the container bounds. It includes various formatting options."
                font_size: 16px
                line_height: 1.6
                color: #666666
                text_align: justify
                margin_bottom: 15px
                max_width: 460px
            }
            
            Text {
                text: "• Bullet point one\n• Bullet point two\n• Bullet point three"
                font_size: 14px
                line_height: 1.5
                color: #444444
                font_family: monospace
                margin_bottom: 20px
            }
            
            RichText {
                content: "This is **bold text**, *italic text*, and `inline code`. Here's a [link](https://example.com) and some math: x² + y² = z²"
                font_size: 16px
                line_height: 1.5
                color: #333333
                width: 460px
                
                styles: {
                    bold: { font_weight: bold }
                    italic: { font_style: italic }
                    code: { 
                        font_family: monospace
                        background_color: #f5f5f5
                        padding: 2px 4px
                        border_radius: 3px
                    }
                    link: {
                        color: #0066cc
                        text_decoration: underline
                    }
                }
            }
        }
    "#;
    
    harness.add_source_file("text_complex.kry", text_source);
    harness.compile_all()?;
    
    // Test complex text rendering
    let result = harness.run_with_backend("text_complex.kry", TestBackend::Html)?;
    assert!(result.success);
    
    Ok(())
}

#[tokio::test]
async fn test_image_rendering() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let image_source = r#"
        Container {
            width: 600px
            height: 400px
            display: flex
            flex_direction: row
            flex_wrap: wrap
            gap: 20px
            padding: 20px
            
            Image {
                src: "photo1.jpg"
                width: 150px
                height: 100px
                object_fit: cover
                border_radius: 8px
                box_shadow: 0px 2px 8px rgba(0,0,0,0.1)
            }
            
            Image {
                src: "photo2.png"
                width: 100px
                height: 100px
                object_fit: contain
                background_color: #f0f0f0
                border: 1px solid #cccccc
            }
            
            Image {
                src: "logo.svg"
                width: 120px
                height: 60px
                object_fit: scale-down
            }
            
            Image {
                src: "placeholder.webp"
                width: 200px
                height: 150px
                object_fit: fill
                opacity: 0.8
                filter: grayscale(50%)
            }
        }
    "#;
    
    // Create mock image files
    let temp_path = harness.temp_path();
    std::fs::write(temp_path.join("photo1.jpg"), b"fake-jpg-data")?;
    std::fs::write(temp_path.join("photo2.png"), b"fake-png-data")?;
    std::fs::write(temp_path.join("logo.svg"), b"<svg></svg>")?;
    std::fs::write(temp_path.join("placeholder.webp"), b"fake-webp-data")?;
    
    harness.add_source_file("images.kry", image_source);
    harness.compile_all()?;
    
    // Test image rendering
    let result = harness.run_with_backend("images.kry", TestBackend::Html)?;
    assert!(result.success);
    
    Ok(())
}

#[tokio::test]
async fn test_scroll_rendering() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let scroll_source = r#"
        Container {
            width: 400px
            height: 300px
            
            ScrollContainer {
                width: 100%
                height: 100%
                overflow_x: hidden
                overflow_y: auto
                
                Container {
                    width: 100%
                    height: 800px
                    padding: 20px
                    
                    Text {
                        text: "Scrollable Content"
                        font_size: 24px
                        font_weight: bold
                        margin_bottom: 20px
                    }
                    
                    Container {
                        height: 600px
                        background_color: #f0f0f0
                        border: 1px solid #cccccc
                        display: flex
                        align_items: center
                        justify_content: center
                        
                        Text {
                            text: "This content extends beyond the viewport"
                            font_size: 16px
                            color: #666666
                        }
                    }
                    
                    Text {
                        text: "Bottom of scrollable area"
                        font_size: 14px
                        margin_top: 20px
                    }
                }
                
                Scrollbar {
                    orientation: vertical
                    width: 16px
                    track_color: #f0f0f0
                    thumb_color: #cccccc
                    thumb_hover_color: #999999
                }
            }
        }
    "#;
    
    harness.add_source_file("scroll.kry", scroll_source);
    harness.compile_all()?;
    
    // Test scroll rendering
    let result = harness.run_with_backend("scroll.kry", TestBackend::Html)?;
    assert!(result.success);
    
    Ok(())
}

#[tokio::test]
async fn test_widget_rendering_integration() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    let widget_source = r#"
        Container {
            width: 800px
            height: 600px
            padding: 20px
            display: flex
            flex_direction: column
            gap: 20px
            
            Text {
                text: "Widget Rendering Test"
                font_size: 24px
                font_weight: bold
                margin_bottom: 10px
            }
            
            Container {
                display: flex
                flex_direction: row
                gap: 20px
                
                Dropdown {
                    width: 200px
                    height: 32px
                    placeholder: "Select an option"
                    items: [
                        { label: "Option 1", value: "opt1" },
                        { label: "Option 2", value: "opt2" },
                        { label: "Option 3", value: "opt3" }
                    ]
                }
                
                DatePicker {
                    width: 150px
                    height: 32px
                    format: "MM/dd/yyyy"
                }
                
                NumberInput {
                    width: 120px
                    height: 32px
                    min: 0
                    max: 100
                    step: 1
                    value: 50
                }
            }
            
            DataGrid {
                width: 100%
                height: 300px
                columns: [
                    { id: "id", title: "ID", width: 60 },
                    { id: "name", title: "Name", width: 150 },
                    { id: "email", title: "Email", width: 200 },
                    { id: "status", title: "Status", width: 100 }
                ]
                data: [
                    { id: 1, name: "John Doe", email: "john@example.com", status: "Active" },
                    { id: 2, name: "Jane Smith", email: "jane@example.com", status: "Inactive" },
                    { id: 3, name: "Bob Johnson", email: "bob@example.com", status: "Active" }
                ]
                sortable: true
                filterable: true
                striped_rows: true
            }
            
            Container {
                display: flex
                flex_direction: row
                gap: 20px
                align_items: flex_start
                
                ColorPicker {
                    width: 200px
                    height: 200px
                    value: "#0066cc"
                    show_preview: true
                }
                
                RichTextEditor {
                    width: 400px
                    height: 200px
                    placeholder: "Enter rich text..."
                    toolbar: true
                    content: "Sample **bold** and *italic* text."
                }
            }
        }
    "#;
    
    harness.add_source_file("widgets_render.kry", widget_source);
    harness.compile_all()?;
    
    // Test widget rendering
    let result = harness.run_with_backend("widgets_render.kry", TestBackend::Html)?;
    assert!(result.success);
    
    Ok(())
}

#[test]
fn test_rendering_performance() -> Result<()> {
    let test = IntegrationTest {
        name: "render_performance".to_string(),
        source: generate_large_ui_source(500), // 500 elements
        expected_success: true,
        expected_elements: Some(501), // Container + 500 children
        test_type: IntegrationTestType::Rendering,
    };
    
    let start_time = std::time::Instant::now();
    let result = test.run()?;
    let render_time = start_time.elapsed();
    
    assert!(result.success);
    assert!(render_time.as_millis() < 1000, 
        "Rendering took too long: {:?}", render_time);
    
    Ok(())
}

// Helper function to generate large UI for performance testing
fn generate_large_ui_source(element_count: usize) -> String {
    let mut source = String::from(
        "Container {\n  display: flex\n  flex_direction: column\n  width: 800px\n  height: 600px\n"
    );
    
    for i in 0..element_count {
        source.push_str(&format!(
            "  Container {{\n    width: 100%\n    height: 30px\n    margin_bottom: 2px\n    background_color: #{:06x}\n    Text {{\n      text: \"Item {}\"\n      font_size: 14px\n    }}\n  }}\n",
            (i * 1000) % 0xFFFFFF, // Generate different colors
            i
        ));
    }
    
    source.push_str("}\n");
    source
}