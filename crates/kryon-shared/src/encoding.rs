//! Encoding and decoding utilities for property values

use crate::errors::EncodingError;
use crate::property_registry::*;

/// Text alignment encoding and decoding
impl TextAlignment {
    /// Convert string value to TextAlignment enum
    pub fn from_string(s: &str) -> Result<Self, EncodingError> {
        match s.to_lowercase().as_str() {
            "start" | "left" => Ok(TextAlignment::Start),
            "center" => Ok(TextAlignment::Center),
            "end" | "right" => Ok(TextAlignment::End),
            "justify" => Ok(TextAlignment::Justify),
            _ => Err(EncodingError::InvalidTextAlignment(s.to_string())),
        }
    }
    
    /// Convert to u8 value for binary encoding
    pub fn to_u8(self) -> u8 {
        self as u8
    }
    
    /// Convert from u8 value from binary decoding
    pub fn from_u8(value: u8) -> Result<Self, EncodingError> {
        match value {
            0 => Ok(TextAlignment::Start),
            1 => Ok(TextAlignment::Center),
            2 => Ok(TextAlignment::End),
            3 => Ok(TextAlignment::Justify),
            _ => Err(EncodingError::InvalidU8Value { 
                property: "TextAlignment", 
                value 
            }),
        }
    }
    
    /// Convert to string representation
    pub fn to_string(self) -> &'static str {
        match self {
            TextAlignment::Start => "start",
            TextAlignment::Center => "center",
            TextAlignment::End => "end",
            TextAlignment::Justify => "justify",
        }
    }
}

/// JustifyContent encoding and decoding
/* REMOVED: JustifyContent is now PropertyId::JustifyContent variant
impl JustifyContent {
    pub fn from_string(s: &str) -> Result<Self, EncodingError> {
        match s.to_lowercase().as_str() {
            "flex-start" | "start" => Ok(JustifyContent::FlexStart),
            "flex-end" | "end" => Ok(JustifyContent::FlexEnd),
            "center" => Ok(JustifyContent::Center),
            "space-between" => Ok(JustifyContent::SpaceBetween),
            "space-around" => Ok(JustifyContent::SpaceAround),
            "space-evenly" => Ok(JustifyContent::SpaceEvenly),
            _ => Err(EncodingError::InvalidJustifyContent(s.to_string())),
        }
    }
    
    pub fn to_u8(self) -> u8 {
        self as u8
    }
    
    pub fn from_u8(value: u8) -> Result<Self, EncodingError> {
        match value {
            0 => Ok(JustifyContent::FlexStart),
            1 => Ok(JustifyContent::FlexEnd),
            2 => Ok(JustifyContent::Center),
            3 => Ok(JustifyContent::SpaceBetween),
            4 => Ok(JustifyContent::SpaceAround),
            5 => Ok(JustifyContent::SpaceEvenly),
            _ => Err(EncodingError::InvalidU8Value { 
                property: "JustifyContent", 
                value 
            }),
        }
    }
    
    pub fn to_string(self) -> &'static str {
        match self {
            JustifyContent::FlexStart => "flex-start",
            JustifyContent::FlexEnd => "flex-end",
            JustifyContent::Center => "center",
            JustifyContent::SpaceBetween => "space-between",
            JustifyContent::SpaceAround => "space-around",
            JustifyContent::SpaceEvenly => "space-evenly",
        }
    }
}
*/

/* REMOVED: AlignItems is now PropertyId::AlignItems variant
impl AlignItems {
    pub fn from_string(s: &str) -> Result<Self, EncodingError> {
        match s.to_lowercase().as_str() {
            "flex-start" | "start" => Ok(AlignItems::FlexStart),
            "flex-end" | "end" => Ok(AlignItems::FlexEnd),
            "center" => Ok(AlignItems::Center),
            "stretch" => Ok(AlignItems::Stretch),
            "baseline" => Ok(AlignItems::Baseline),
            _ => Err(EncodingError::InvalidAlignItems(s.to_string())),
        }
    }
    
    pub fn to_u8(self) -> u8 {
        self as u8
    }
    
    pub fn from_u8(value: u8) -> Result<Self, EncodingError> {
        match value {
            0 => Ok(AlignItems::FlexStart),
            1 => Ok(AlignItems::FlexEnd),
            2 => Ok(AlignItems::Center),
            3 => Ok(AlignItems::Stretch),
            4 => Ok(AlignItems::Baseline),
            _ => Err(EncodingError::InvalidU8Value { 
                property: "AlignItems", 
                value 
            }),
        }
    }
    
    pub fn to_string(self) -> &'static str {
        match self {
            AlignItems::FlexStart => "flex-start",
            AlignItems::FlexEnd => "flex-end",
            AlignItems::Center => "center",
            AlignItems::Stretch => "stretch",
            AlignItems::Baseline => "baseline",
        }
    }
}
*/

/* REMOVED: FlexDirection is now PropertyId::FlexDirection variant
impl FlexDirection {
    pub fn from_string(s: &str) -> Result<Self, EncodingError> {
        match s.to_lowercase().as_str() {
            "row" => Ok(FlexDirection::Row),
            "column" => Ok(FlexDirection::Column),
            "row-reverse" => Ok(FlexDirection::RowReverse),
            "column-reverse" => Ok(FlexDirection::ColumnReverse),
            _ => Err(EncodingError::InvalidFlexDirection(s.to_string())),
        }
    }
    
    pub fn to_u8(self) -> u8 {
        self as u8
    }
    
    pub fn from_u8(value: u8) -> Result<Self, EncodingError> {
        match value {
            0 => Ok(FlexDirection::Row),
            1 => Ok(FlexDirection::Column),
            2 => Ok(FlexDirection::RowReverse),
            3 => Ok(FlexDirection::ColumnReverse),
            _ => Err(EncodingError::InvalidU8Value { 
                property: "FlexDirection", 
                value 
            }),
        }
    }
    
    pub fn to_string(self) -> &'static str {
        match self {
            FlexDirection::Row => "row",
            FlexDirection::Column => "column",
            FlexDirection::RowReverse => "row-reverse",
            FlexDirection::ColumnReverse => "column-reverse",
        }
    }
}
*/

/* REMOVED: Display is now PropertyId::Display variant
impl Display {
    pub fn from_string(s: &str) -> Result<Self, EncodingError> {
        match s.to_lowercase().as_str() {
            "none" => Ok(Display::None),
            "block" => Ok(Display::Block),
            "flex" => Ok(Display::Flex),
            "grid" => Ok(Display::Grid),
            "inline" => Ok(Display::Inline),
            "inline-block" => Ok(Display::InlineBlock),
            _ => Err(EncodingError::InvalidDisplay(s.to_string())),
        }
    }
    
    pub fn to_u8(self) -> u8 {
        self as u8
    }
    
    pub fn from_u8(value: u8) -> Result<Self, EncodingError> {
        match value {
            0 => Ok(Display::None),
            1 => Ok(Display::Block),
            2 => Ok(Display::Flex),
            3 => Ok(Display::Grid),
            4 => Ok(Display::Inline),
            5 => Ok(Display::InlineBlock),
            _ => Err(EncodingError::InvalidU8Value { 
                property: "Display", 
                value 
            }),
        }
    }
    
    pub fn to_string(self) -> &'static str {
        match self {
            Display::None => "none",
            Display::Block => "block",
            Display::Flex => "flex",
            Display::Grid => "grid",
            Display::Inline => "inline",
            Display::InlineBlock => "inline-block",
        }
    }
}
*/

/* REMOVED: Position is now PropertyId::Position variant
impl Position {
    pub fn from_string(s: &str) -> Result<Self, EncodingError> {
        match s.to_lowercase().as_str() {
            "static" => Ok(Position::Static),
            "relative" => Ok(Position::Relative),
            "absolute" => Ok(Position::Absolute),
            "fixed" => Ok(Position::Fixed),
            "sticky" => Ok(Position::Sticky),
            _ => Err(EncodingError::InvalidPosition(s.to_string())),
        }
    }
    
    pub fn to_u8(self) -> u8 {
        self as u8
    }
    
    pub fn from_u8(value: u8) -> Result<Self, EncodingError> {
        match value {
            0 => Ok(Position::Static),
            1 => Ok(Position::Relative),
            2 => Ok(Position::Absolute),
            3 => Ok(Position::Fixed),
            4 => Ok(Position::Sticky),
            _ => Err(EncodingError::InvalidU8Value { 
                property: "Position", 
                value 
            }),
        }
    }
    
    pub fn to_string(self) -> &'static str {
        match self {
            Position::Static => "static",
            Position::Relative => "relative",
            Position::Absolute => "absolute",
            Position::Fixed => "fixed",
            Position::Sticky => "sticky",
        }
    }
}
*/

/// Utility function to encode any property value from string
/// FIXME: Update this to work with PropertyId variants instead of separate enums
pub fn encode_property_value(property_id: PropertyId, value: &str) -> Result<u8, EncodingError> {
    match property_id {
        PropertyId::TextAlignment => TextAlignment::from_string(value).map(|v| v.to_u8()),
        // FIXME: These need to be reimplemented as PropertyId methods
        // PropertyId::JustifyContent => JustifyContent::from_string(value).map(|v| v.to_u8()),
        // PropertyId::AlignItems => AlignItems::from_string(value).map(|v| v.to_u8()),
        // PropertyId::FlexDirection => FlexDirection::from_string(value).map(|v| v.to_u8()),
        // PropertyId::Display => Display::from_string(value).map(|v| v.to_u8()),
        // PropertyId::Position => Position::from_string(value).map(|v| v.to_u8()),
        _ => Err(EncodingError::UnsupportedProperty("FIXME")),
    }
}

/// Utility function to decode any property value to string
/// FIXME: Update this to work with PropertyId variants instead of separate enums
pub fn decode_property_value(property_id: PropertyId, value: u8) -> Result<String, EncodingError> {
    match property_id {
        PropertyId::TextAlignment => TextAlignment::from_u8(value).map(|v| v.to_string().to_string()),
        // FIXME: These need to be reimplemented as PropertyId methods
        // PropertyId::JustifyContent => JustifyContent::from_u8(value).map(|v| v.to_string().to_string()),
        // PropertyId::AlignItems => AlignItems::from_u8(value).map(|v| v.to_string().to_string()),
        // PropertyId::FlexDirection => FlexDirection::from_u8(value).map(|v| v.to_string().to_string()),
        // PropertyId::Display => Display::from_u8(value).map(|v| v.to_string().to_string()),
        // PropertyId::Position => Position::from_u8(value).map(|v| v.to_string().to_string()),
        _ => Err(EncodingError::UnsupportedProperty("FIXME")),
    }
}