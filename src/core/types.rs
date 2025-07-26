// FILE: src/core/types.rs

// Re-export unified types from kryon-shared
pub use kryon_shared::types::*;

// Additional implementation for InputType not in kryon-shared
impl InputType {
    /// Returns true if this input type supports min/max/step properties
    pub fn supports_range(self) -> bool {
        matches!(self, Self::Number | Self::Range | Self::Date | 
                      Self::DatetimeLocal | Self::Month | Self::Time | Self::Week)
    }
    
    /// Returns true if this input type supports the checked property
    pub fn supports_checked(self) -> bool {
        matches!(self, Self::Checkbox | Self::Radio)
    }
}

impl Default for InputType {
    fn default() -> Self {
        Self::Text
    }
}

// Value Types
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum ValueType {
    None = 0x00,
    Byte = 0x01,
    Short = 0x02,
    Color = 0x03,
    String = 0x04,
    Resource = 0x05,
    Percentage = 0x06,
    Rect = 0x07,
    EdgeInsets = 0x08,
    Enum = 0x09,
    Vector = 0x0A,
    Custom = 0x0B,
    // Hint types (for parsing)
    StyleId = 0x0C,
    Float = 0x0D,
    Int = 0x0E,
    Bool = 0x0F,
    
    // Taffy-specific value types
    GridTrack = 0x10,        // fr, px, %, auto units for grid tracks
    GridArea = 0x11,         // Grid area specification (line-based)
    FlexValue = 0x12,        // Flex grow/shrink values
    AlignmentValue = 0x13,   // Alignment enum values
    PositionValue = 0x14,    // Position enum (relative, absolute, etc.)
    LengthPercentage = 0x15, // CSS length-percentage values
    Dimension = 0x16,        // Auto, Length, Percentage dimension
    
    // Transform-specific value types
    Transform = 0x17,        // Transform object with multiple properties
    TransformMatrix = 0x18,  // 4x4 or 2x3 transformation matrix
    CSSUnit = 0x19,          // CSS unit values (px, em, rem, vw, vh, deg, rad, turn)
    Transform2D = 0x1A,      // Optimized 2D transform (scale, translate, rotate)
    Transform3D = 0x1B,      // Full 3D transform data
    
    // Template variable marker (to be resolved during compilation)
    TemplateVariable = 0x1C, // Holds variable name to be substituted
}

// Script language IDs
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u8)]
pub enum ScriptLanguage {
    Lua = 0x01,
    JavaScript = 0x02,
    Python = 0x03,
    Wren = 0x04,
}

impl ScriptLanguage {
    pub fn from_name(name: &str) -> Option<Self> {
        match name.to_lowercase().as_str() {
            "lua" => Some(Self::Lua),
            "javascript" | "js" => Some(Self::JavaScript),
            "python" | "py" => Some(Self::Python),
            "wren" => Some(Self::Wren),
            _ => None,
        }
    }
}

// Resource types
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum ResourceType {
    Image = 0x01,
    Font = 0x02,
    Sound = 0x03,
    Video = 0x04,
    Script = 0x05,
    Custom = 0x06,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum ResourceFormat {
    External = 0x00,
    Inline = 0x01,
}