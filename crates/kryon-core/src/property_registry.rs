// crates/kryon-core/src/property_registry.rs

// Removed unused imports

// Import unified property types from kryon-shared
pub use kryon_shared::types::PropertyId;

/// Unified property registry that consolidates all property handling
/// Replaces the 4 separate property mapping systems:
/// 1. KRB Parser Mapping (krb.rs:669-1212)
/// 2. Style Computer Mapping (style.rs:152-228)
/// 3. Style Inheritance Mapping (style.rs:78-104)
/// 4. Legacy Layout Flags (flexbox.rs:38-64)

/// Property metadata for registry
#[derive(Debug, Clone)]
pub struct PropertyMetadata {
    pub id: PropertyId,
    pub name: &'static str,
    pub inheritable: bool,
}

/// Property registry for managing all property metadata
#[derive(Debug, Clone)]
pub struct PropertyRegistry {
    properties: Vec<PropertyMetadata>,
}

impl PropertyRegistry {
    /// Create a new property registry with all known properties
    pub fn new() -> Self {
        let properties = vec![
            // Visual Properties
            PropertyMetadata {
                id: PropertyId::BackgroundColor,
                name: "background-color",
                inheritable: false,
            },
            PropertyMetadata {
                id: PropertyId::TextColor,
                name: "color",
                inheritable: true,
            },
            PropertyMetadata {
                id: PropertyId::BorderColor,
                name: "border-color",
                inheritable: false,
            },
            PropertyMetadata {
                id: PropertyId::BorderWidth,
                name: "border-width",
                inheritable: false,
            },
            PropertyMetadata {
                id: PropertyId::BorderRadius,
                name: "border-radius",
                inheritable: false,
            },
            PropertyMetadata {
                id: PropertyId::FontSize,
                name: "font-size",
                inheritable: true,
            },
            PropertyMetadata {
                id: PropertyId::FontWeight,
                name: "font-weight",
                inheritable: true,
            },
            PropertyMetadata {
                id: PropertyId::TextAlignment,
                name: "text-align",
                inheritable: true,
            },
            PropertyMetadata {
                id: PropertyId::ListStyleType,
                name: "list-style-type",
                inheritable: true,
            },
            PropertyMetadata {
                id: PropertyId::WhiteSpace,
                name: "white-space",
                inheritable: true,
            },
            PropertyMetadata {
                id: PropertyId::FontFamily,
                name: "font-family",
                inheritable: true,
            },
            PropertyMetadata {
                id: PropertyId::Opacity,
                name: "opacity",
                inheritable: true,
            },
            PropertyMetadata {
                id: PropertyId::ZIndex,
                name: "z-index",
                inheritable: false,
            },
            PropertyMetadata {
                id: PropertyId::Visibility,
                name: "visibility",
                inheritable: true,
            },
            PropertyMetadata {
                id: PropertyId::Shadow,
                name: "box-shadow",
                inheritable: false,
            },
            PropertyMetadata {
                id: PropertyId::Cursor,
                name: "cursor",
                inheritable: true,
            },
            PropertyMetadata {
                id: PropertyId::Width,
                name: "width",
                inheritable: false,
            },
            PropertyMetadata {
                id: PropertyId::Height,
                name: "height",
                inheritable: false,
            },
            PropertyMetadata {
                id: PropertyId::Gap,
                name: "gap",
                inheritable: false,
            },
            PropertyMetadata {
                id: PropertyId::StyleId,
                name: "style",
                inheritable: false,
            },
            // Add more properties as needed...
        ];

        Self { properties }
    }

    /// Check if a property is inheritable
    pub fn is_inheritable(&self, property_id: PropertyId) -> bool {
        self.properties
            .iter()
            .find(|p| p.id == property_id)
            .map(|p| p.inheritable)
            .unwrap_or(false)
    }

    /// Get property metadata by ID
    pub fn get_property_metadata(&self, property_id: PropertyId) -> Option<&PropertyMetadata> {
        self.properties.iter().find(|p| p.id == property_id)
    }

    /// Get property metadata by name
    pub fn get_property_by_name(&self, name: &str) -> Option<&PropertyMetadata> {
        self.properties.iter().find(|p| p.name == name)
    }
}

impl Default for PropertyRegistry {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_property_registry() {
        let registry = PropertyRegistry::new();
        
        // Test property lookup
        let bg_color = registry.get_property_metadata(PropertyId::BackgroundColor);
        assert!(bg_color.is_some());
        assert_eq!(bg_color.unwrap().name, "background-color");
        
        // Test inheritance
        assert!(registry.is_inheritable(PropertyId::TextColor));
        assert!(!registry.is_inheritable(PropertyId::BackgroundColor));
        
        // Test u8 conversion
        let prop_from_u8 = registry.get_property_metadata(PropertyId::from_u8(0x01));
        assert!(prop_from_u8.is_some());
        assert_eq!(prop_from_u8.unwrap().id, PropertyId::BackgroundColor);
    }
    
    #[test]
    fn test_property_id_conversion() {
        // Test basic conversions
        assert_eq!(PropertyId::from_u8(0x01), PropertyId::BackgroundColor);
        assert_eq!(PropertyId::from_u8(0x02), PropertyId::TextColor);
        assert_eq!(PropertyId::from_u8(0xFF), PropertyId::Custom(0xFF));
    }
}