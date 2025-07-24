//! KRB binary format constants and definitions

/// KRB magic bytes
pub const KRB_MAGIC: &[u8] = b"KRB1";

/// KRB version constants
pub const KRB_VERSION_MAJOR: u8 = 0;
pub const KRB_VERSION_MINOR: u8 = 5;

/// Value type identifiers for property values
#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
#[repr(u8)]
pub enum ValueType {
    None = 0x00,
    Color = 0x01,
    Short = 0x02,
    Int = 0x03,
    String = 0x04,
    Float = 0x05,
    Bool = 0x06,
    Bytes = 0x07,
    
    // Additional compiler-needed types
    Byte = 0x08,
    Resource = 0x09,
    Percentage = 0x0A,
    Rect = 0x0B,
    EdgeInsets = 0x0C,
    Enum = 0x0D,
    Vector = 0x0E,
    Custom = 0x0F,
    StyleId = 0x10,
    
    // Taffy-specific value types
    GridTrack = 0x11,
    GridArea = 0x12,
    FlexValue = 0x13,
    AlignmentValue = 0x14,
    PositionValue = 0x15,
    LengthPercentage = 0x16,
    Dimension = 0x17,
    Transform = 0x18,
    TransformMatrix = 0x19,
    CSSUnit = 0x1A,
    Expression = 0x1B,
    Transform2D = 0x1C,
    Transform3D = 0x1D,
    TemplateVariable = 0x1E,
}

impl ValueType {
    pub fn from_u8(value: u8) -> Option<Self> {
        match value {
            0x00 => Some(ValueType::None),
            0x01 => Some(ValueType::Color),
            0x02 => Some(ValueType::Short),
            0x03 => Some(ValueType::Int),
            0x04 => Some(ValueType::String),
            0x05 => Some(ValueType::Float),
            0x06 => Some(ValueType::Bool),
            0x07 => Some(ValueType::Bytes),
            0x08 => Some(ValueType::Byte),
            0x09 => Some(ValueType::Resource),
            0x0A => Some(ValueType::Percentage),
            0x0B => Some(ValueType::Rect),
            0x0C => Some(ValueType::EdgeInsets),
            0x0D => Some(ValueType::Enum),
            0x0E => Some(ValueType::Vector),
            0x0F => Some(ValueType::Custom),
            0x10 => Some(ValueType::StyleId),
            0x11 => Some(ValueType::GridTrack),
            0x12 => Some(ValueType::GridArea),
            0x13 => Some(ValueType::FlexValue),
            0x14 => Some(ValueType::AlignmentValue),
            0x15 => Some(ValueType::PositionValue),
            0x16 => Some(ValueType::LengthPercentage),
            0x17 => Some(ValueType::Dimension),
            0x18 => Some(ValueType::Transform),
            0x19 => Some(ValueType::TransformMatrix),
            0x1A => Some(ValueType::CSSUnit),
            0x1B => Some(ValueType::Expression),
            0x1C => Some(ValueType::Transform2D),
            0x1D => Some(ValueType::Transform3D),
            0x1E => Some(ValueType::TemplateVariable),
            _ => None,
        }
    }
    
    pub fn as_u8(self) -> u8 {
        self as u8
    }
}