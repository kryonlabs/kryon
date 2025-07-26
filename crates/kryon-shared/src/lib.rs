//! Kryon Shared Types Library
//!
//! This crate provides unified type definitions, property mappings, and constants
//! used by both the Kryon compiler and renderer to ensure consistency across the monorepo.

// UNIFIED TYPES - SINGLE SOURCE OF TRUTH
pub mod types;

// Shared type modules from the unified codebase
pub mod elements;
pub mod events;
pub mod layout_units;
pub mod properties;
pub mod property_registry;
pub mod resources;
pub mod style;
pub mod text;
pub mod css_transforms;

// Legacy modules for backward compatibility (will be deprecated)
pub mod encoding;
pub mod krb_format;
pub mod errors;

// Re-export unified types - SINGLE SOURCE OF TRUTH
pub use types::*;

// Re-export commonly used types for convenience
pub use elements::*;
pub use events::*;
pub use layout_units::*;
pub use properties::*;
pub use resources::*;
pub use style::*;
pub use text::*;
pub use css_transforms::*;

// Re-export property registry with explicit names to avoid conflicts
pub use property_registry::{PropertyId, PropertyRegistry, JustifyContent, AlignItems, FlexDirection, Display, Position};
// Re-export TextAlignment explicitly to resolve ambiguity
pub use property_registry::TextAlignment as PropertyTextAlignment;

// Legacy re-exports
pub use encoding::*;
pub use krb_format::*;
pub use errors::*;

// Common type aliases
pub type ElementId = u32;

/// Version information for the Kryon framework
pub const KRYON_VERSION: &str = env!("CARGO_PKG_VERSION");

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_text_alignment_encoding() {
        // Test the critical bug fix: "center" should encode as 1, not 5
        let center_alignment = TextAlignment::from_string("center").unwrap();
        assert_eq!(center_alignment.to_u8(), 1);
        
        // Test round-trip conversion
        let decoded = TextAlignment::from_u8(1).unwrap();
        assert_eq!(decoded, TextAlignment::Center);
        assert_eq!(decoded.to_string(), "center");
        
        // Test all values
        assert_eq!(TextAlignment::Start.to_u8(), 0);
        assert_eq!(TextAlignment::Center.to_u8(), 1);
        assert_eq!(TextAlignment::End.to_u8(), 2);
        assert_eq!(TextAlignment::Justify.to_u8(), 3);
    }
    
    #[test]
    fn test_justify_content_encoding() {
        // Test that JustifyContent::SpaceEvenly is indeed 5
        let space_evenly = JustifyContent::from_string("space-evenly").unwrap();
        assert_eq!(space_evenly.to_u8(), 5);
        
        // Test center is 2 for JustifyContent
        let center = JustifyContent::from_string("center").unwrap();
        assert_eq!(center.to_u8(), 2);
    }
    
    #[test]
    fn test_property_id_uniqueness() {
        // Verify that TextAlignment and JustifyContent have different property IDs
        assert_eq!(PropertyId::TextAlignment.as_u8(), 0x0B);
        assert_eq!(PropertyId::JustifyContent.as_u8(), 0x4A);
        
        // These should never be equal!
        assert_ne!(PropertyId::TextAlignment.as_u8(), PropertyId::JustifyContent.as_u8());
    }
    
    #[test]
    fn test_encoding_utility_functions() {
        // Test the utility function that was probably causing the bug
        let result = encode_property_value(PropertyId::TextAlignment, "center").unwrap();
        assert_eq!(result, 1); // Should be 1, not 5
        
        let decoded = decode_property_value(PropertyId::TextAlignment, 1).unwrap();
        assert_eq!(decoded, "center");
    }
}