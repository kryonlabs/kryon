//! Code generation and binary format tests

use kryon_compiler::compiler::backend::krb_generator::KrbGenerator;
use kryon_core::KrbFile;

#[test]
fn test_krb_header_generation() {
    let generator = KrbGenerator::new();
    let krb_data = generator.generate_empty();
    
    // Verify magic header
    assert_eq!(&krb_data[0..4], b"KRB1");
    
    // Verify version
    assert_eq!(krb_data[4], 1); // Version 1
    
    // Parse to verify structure
    let krb = KrbFile::parse(&krb_data).unwrap();
    assert_eq!(krb.elements.len(), 0);
    assert_eq!(krb.styles.len(), 0);
}

#[test]
fn test_string_table_generation() {
    let mut generator = KrbGenerator::new();
    
    // Add strings
    let idx1 = generator.add_string("hello");
    let idx2 = generator.add_string("world");
    let idx3 = generator.add_string("hello"); // Duplicate
    
    assert_eq!(idx1, 0);
    assert_eq!(idx2, 1);
    assert_eq!(idx3, 0); // Should reuse existing string
    
    let krb_data = generator.generate();
    let krb = KrbFile::parse(&krb_data).unwrap();
    
    assert_eq!(krb.strings.len(), 2);
    assert_eq!(krb.strings[0], "hello");
    assert_eq!(krb.strings[1], "world");
}

#[test]
fn test_element_encoding() {
    let mut generator = KrbGenerator::new();
    
    // Create a test element
    let mut element = generator.create_element(ElementType::Container);
    element.id = generator.add_string("test_container");
    element.width = 100;
    element.height = 200;
    
    generator.add_element(element);
    
    let krb_data = generator.generate();
    let krb = KrbFile::parse(&krb_data).unwrap();
    
    assert_eq!(krb.elements.len(), 1);
    let parsed = &krb.elements[0];
    assert_eq!(parsed.element_type, ElementType::Container);
    assert_eq!(parsed.id, "test_container");
    assert_eq!(parsed.width, 100);
    assert_eq!(parsed.height, 200);
}

#[test]
fn test_property_encoding() {
    let mut generator = KrbGenerator::new();
    
    // Test various property encodings
    let test_cases = vec![
        (PropertyId::AlignItems, "end", vec![0x47, 0x07, 0x01, 0x01]),
        (PropertyId::Display, "flex", vec![0x40, 0x07, 0x01, 0x02]),
        (PropertyId::Width, "100", vec![0x19, 0x02, 0x02, 0x64, 0x00]),
    ];
    
    for (prop_id, value, expected_bytes) in test_cases {
        let encoded = generator.encode_property(prop_id, value);
        assert_eq!(encoded[0..expected_bytes.len()], expected_bytes,
            "Property {:?} with value {} should encode correctly", prop_id, value);
    }
}

#[test]
fn test_style_deduplication() {
    let mut generator = KrbGenerator::new();
    
    // Create multiple elements with same style
    for i in 0..3 {
        let mut element = generator.create_element(ElementType::Container);
        element.style_properties = vec![
            ("background_color", "#ff0000ff"),
            ("border_width", "2"),
            ("border_radius", "4"),
        ];
        generator.add_element(element);
    }
    
    let krb_data = generator.generate();
    let krb = KrbFile::parse(&krb_data).unwrap();
    
    // Should deduplicate to single style
    assert_eq!(krb.styles.len(), 1);
    
    // All elements should reference same style
    assert_eq!(krb.elements[0].style_id, krb.elements[1].style_id);
    assert_eq!(krb.elements[1].style_id, krb.elements[2].style_id);
}