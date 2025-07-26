//! Property validation and encoding tests

use kryon_compiler::compiler::middle_end::style_resolver::resolve_style_value;
use kryon_shared::PropertyId;

#[test]
fn test_align_items_encoding() {
    // Test that align_items values encode correctly
    let test_cases = vec![
        ("start", 0u8),
        ("flex-start", 0u8),
        ("end", 1u8),
        ("flex-end", 1u8),
        ("center", 2u8),
        ("stretch", 3u8),
        ("baseline", 4u8),
    ];
    
    for (input, expected) in test_cases {
        let encoded = resolve_style_value(PropertyId::AlignItems, input);
        assert_eq!(encoded, vec![expected], 
            "align_items: {} should encode to {}", input, expected);
    }
}

#[test]
fn test_justify_content_encoding() {
    let test_cases = vec![
        ("start", 0u8),
        ("flex-start", 0u8),
        ("end", 1u8),
        ("flex-end", 1u8),
        ("center", 2u8),
        ("space-between", 3u8),
        ("space-around", 4u8),
        ("space-evenly", 5u8),
    ];
    
    for (input, expected) in test_cases {
        let encoded = resolve_style_value(PropertyId::JustifyContent, input);
        assert_eq!(encoded, vec![expected],
            "justify_content: {} should encode to {}", input, expected);
    }
}

#[test]
fn test_flex_direction_encoding() {
    let test_cases = vec![
        ("row", 0u8),
        ("column", 1u8),
        ("row-reverse", 2u8),
        ("column-reverse", 3u8),
    ];
    
    for (input, expected) in test_cases {
        let encoded = resolve_style_value(PropertyId::FlexDirection, input);
        assert_eq!(encoded, vec![expected],
            "flex_direction: {} should encode to {}", input, expected);
    }
}

#[test]
fn test_display_encoding() {
    let test_cases = vec![
        ("none", 0u8),
        ("block", 1u8),
        ("flex", 2u8),
        ("grid", 3u8),
    ];
    
    for (input, expected) in test_cases {
        let encoded = resolve_style_value(PropertyId::Display, input);
        assert_eq!(encoded, vec![expected],
            "display: {} should encode to {}", input, expected);
    }
}

#[test]
fn test_color_parsing() {
    let test_cases = vec![
        ("#ff0000ff", vec![255u8, 0, 0, 255]),
        ("#00ff00ff", vec![0, 255, 0, 255]),
        ("#0000ffff", vec![0, 0, 255, 255]),
        ("#000000ff", vec![0, 0, 0, 255]),
        ("#ffffffff", vec![255, 255, 255, 255]),
    ];
    
    for (input, expected) in test_cases {
        let encoded = resolve_style_value(PropertyId::BackgroundColor, input);
        assert_eq!(encoded, expected,
            "Color {} should parse correctly", input);
    }
}

#[test]
fn test_numeric_properties() {
    // Test integer properties
    let width_encoded = resolve_style_value(PropertyId::Width, "100");
    assert_eq!(width_encoded.len(), 2); // Should be encoded as u16
    
    let padding_encoded = resolve_style_value(PropertyId::Padding, "20");
    assert_eq!(padding_encoded.len(), 4); // Should be encoded as f32
    
    let opacity_encoded = resolve_style_value(PropertyId::Opacity, "0.5");
    assert_eq!(opacity_encoded.len(), 4); // Should be encoded as f32
}

#[test]
fn test_percentage_values() {
    let width_pct = resolve_style_value(PropertyId::Width, "50%");
    // Should encode as a percentage value type
    assert!(!width_pct.is_empty());
    
    let height_pct = resolve_style_value(PropertyId::Height, "100%");
    assert!(!height_pct.is_empty());
}