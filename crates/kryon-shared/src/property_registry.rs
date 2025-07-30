//! Property Registry - Single source of truth for all property IDs and enums

#[cfg(feature = "serde_support")]
use serde::{Deserialize, Serialize};

// Use the canonical PropertyId from types.rs - DO NOT DUPLICATE!
pub use crate::types::PropertyId;

// The canonical PropertyId already has all needed methods in types.rs
// This impl block adds additional methods not in the canonical enum
impl PropertyId {
    /// Compatibility method for legacy code that expects as_u8
    pub fn as_u8(self) -> u8 {
        self.to_u8()
    }
    
    /// Check if this property should be handled ONLY in the element header and never as a style property
    pub fn is_element_header_property(key: &str) -> bool {
        matches!(key, "id")
    }
}

// Legacy property compatibility constants and functions
// These help with migration from the old duplicate enum
pub const LEGACY_POSX: PropertyId = PropertyId::Left;
pub const LEGACY_POSY: PropertyId = PropertyId::Top;
pub const LEGACY_INVALID: PropertyId = PropertyId::CustomData;
pub const LEGACY_FLEX: PropertyId = PropertyId::FlexBasis;
pub const LEGACY_RESIZABLE: PropertyId = PropertyId::WindowResizable;
pub const LEGACY_KEEP_ASPECT: PropertyId = PropertyId::WindowFullscreen;
pub const LEGACY_SCALE_FACTOR: PropertyId = PropertyId::WindowTargetFps;
pub const LEGACY_ICON: PropertyId = PropertyId::WindowIcon;
pub const LEGACY_FONT_VARIANT: PropertyId = PropertyId::FontStyle;

/// Helper function to map legacy property names to canonical PropertyId
pub fn legacy_property_id_compat(name: &str) -> PropertyId {
    match name {
        "PosX" => LEGACY_POSX,
        "PosY" => LEGACY_POSY,
        "Invalid" => LEGACY_INVALID,
        "Flex" => LEGACY_FLEX,
        "Resizable" => LEGACY_RESIZABLE,
        "KeepAspect" => LEGACY_KEEP_ASPECT,
        "ScaleFactor" => LEGACY_SCALE_FACTOR,
        "Icon" => LEGACY_ICON,
        "FontVariant" => LEGACY_FONT_VARIANT,
        _ => PropertyId::from_name(name),
    }
}


// Import all enum types from the canonical types.rs
pub use crate::types::TextAlignment;

/// Re-export enum values that were previously in separate enums
/// These are now represented as PropertyId variants in the canonical enum

/// Property registry for managing property inheritance rules
#[derive(Debug, Clone)]
pub struct PropertyRegistry;

impl PropertyRegistry {
    /// Create a new property registry
    pub fn new() -> Self {
        Self
    }
    
    /// Check if a property should be inherited from parent to child
    pub fn is_inheritable(&self, property: PropertyId) -> bool {
        match property {
            // Inheritable text properties
            PropertyId::TextColor
            | PropertyId::FontSize
            | PropertyId::FontWeight
            | PropertyId::TextAlignment
            | PropertyId::FontFamily
            // Inheritable display properties
            | PropertyId::Opacity
            | PropertyId::Visibility
            | PropertyId::Cursor => true,
            
            // Non-inheritable properties
            _ => false,
        }
    }
}

impl Default for PropertyRegistry {
    fn default() -> Self {
        Self::new()
    }
}