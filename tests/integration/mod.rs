//! Integration tests for the complete Kryon pipeline

use std::path::Path;
use kryon_compiler::{compile_file, CompilerOptions};
use kryon_core::KrbFile;
use kryon_runtime::KryonApp;
use kryon_render::MockRenderer;
use tempfile::TempDir;
use insta::assert_snapshot;

/// Helper to compile KRY to KRB and parse it
fn compile_and_parse(kry_content: &str) -> anyhow::Result<KrbFile> {
    let temp_dir = TempDir::new()?;
    let input_path = temp_dir.path().join("test.kry");
    
    std::fs::write(&input_path, kry_content)?;
    
    let options = CompilerOptions::default();
    compile_file(&input_path, &options)?;
    
    let output_path = input_path.with_extension("krb");
    let krb_data = std::fs::read(&output_path)?;
    
    KrbFile::parse(&krb_data)
}

#[test]
fn test_flexbox_alignment_integration() {
    let kry_content = std::fs::read_to_string("tests/fixtures/flexbox_alignment.kry")
        .expect("Failed to read fixture");
    
    let krb = compile_and_parse(&kry_content)
        .expect("Failed to compile and parse");
    
    // Verify elements exist with correct properties
    let elements = &krb.elements;
    
    // Find column_end container
    let column_end = elements.iter()
        .find(|e| e.id == "column_end")
        .expect("Should have column_end container");
    
    // Verify align_items property
    let align_items = column_end.custom_properties.get("align_items")
        .expect("Should have align_items property");
    
    assert_eq!(align_items.as_string().unwrap(), "end");
    
    // Create app and compute layout
    let mut app = KryonApp::new(MockRenderer::new());
    app.load_krb(krb)?;
    app.update(0.016)?; // One frame
    
    // Verify layout computation
    let layout_state = app.get_layout_state();
    assert_snapshot!(layout_state, @r###"
    Container[column_end] at (200, 0) size (100, 100)
      Container[child1] at (250, 0) size (50, 20)  // Right-aligned
      Container[child2] at (230, 20) size (70, 20) // Right-aligned
    "###);
}

#[test]
fn test_property_types_integration() {
    let kry_content = std::fs::read_to_string("tests/fixtures/property_types.kry")
        .expect("Failed to read fixture");
    
    let krb = compile_and_parse(&kry_content)
        .expect("Failed to compile and parse");
    
    // Verify all property types are preserved
    let container = krb.elements.iter()
        .find(|e| e.id == "test_container")
        .expect("Should have test_container");
    
    // Check various property types
    assert!(container.custom_properties.contains_key("display"));
    assert!(container.custom_properties.contains_key("flex_direction"));
    assert!(container.custom_properties.contains_key("border_width"));
    assert!(container.custom_properties.contains_key("opacity"));
    
    // Verify Button element
    let button = krb.elements.iter()
        .find(|e| e.id == "test_button")
        .expect("Should have test_button");
    
    assert_eq!(button.element_type, ElementType::Button);
    assert_eq!(button.cursor, Some(CursorType::Pointer));
}

#[test]
fn test_nested_layout_integration() {
    let kry_content = std::fs::read_to_string("tests/fixtures/nested_layout.kry")
        .expect("Failed to read fixture");
    
    let krb = compile_and_parse(&kry_content)
        .expect("Failed to compile and parse");
    
    // Create app and compute layout
    let mut app = KryonApp::new(MockRenderer::new());
    app.load_krb(krb)?;
    app.update(0.016)?;
    
    // Verify complex nested layout
    let layout_state = app.get_layout_state();
    
    // Sidebar should be fixed width
    assert!(layout_state.contains("Container[sidebar] at (0, 0) size (200, 400)"));
    
    // Main content should fill remaining space
    assert!(layout_state.contains("Container[main] at (200, 0) size (400, 400)"));
    
    // Cards should be evenly distributed
    assert!(layout_state.contains("Container[card1]"));
    assert!(layout_state.contains("Container[card2]"));
    assert!(layout_state.contains("Container[card3]"));
}

#[test]
fn test_event_handling_integration() {
    let kry_content = r#"
        App {
            window_width: 400
            window_height: 300
            
            Button {
                id: "test_btn"
                width: 100
                height: 40
                
                Text {
                    text: "Click Me"
                }
                
                on_click: "handleClick"
            }
        }
    "#;
    
    let krb = compile_and_parse(kry_content)
        .expect("Failed to compile and parse");
    
    let mut app = KryonApp::new(MockRenderer::new());
    app.load_krb(krb)?;
    
    // Simulate click event
    app.handle_click(Vec2::new(50.0, 20.0))?;
    
    // Verify event was handled (would need event tracking in mock renderer)
}

#[test]
fn test_border_shorthand_integration() {
    let kry_content = r#"
        App {
            window_width: 200
            window_height: 200
            
            Container {
                width: 100
                height: 100
                border: "2px solid #ff0000ff"
            }
        }
    "#;
    
    let krb = compile_and_parse(kry_content)
        .expect("Failed to compile and parse");
    
    let container = krb.elements.iter()
        .find(|e| e.element_type == ElementType::Container)
        .expect("Should have container");
    
    // Border shorthand should be expanded
    assert_eq!(container.border_width, 2);
    assert!(container.custom_properties.contains_key("border_color"));
}