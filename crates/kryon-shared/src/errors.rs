//! Error types for property encoding and decoding

use std::fmt;

#[derive(Debug, Clone)]
pub enum EncodingError {
    InvalidTextAlignment(String),
    InvalidJustifyContent(String),
    InvalidAlignItems(String),
    InvalidFlexDirection(String),
    InvalidDisplay(String),
    InvalidPosition(String),
    InvalidU8Value { property: &'static str, value: u8 },
    UnsupportedProperty(&'static str),
}

impl fmt::Display for EncodingError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            EncodingError::InvalidTextAlignment(val) => {
                write!(f, "Invalid text alignment value: '{}'. Expected: start, center, end, justify", val)
            }
            EncodingError::InvalidJustifyContent(val) => {
                write!(f, "Invalid justify-content value: '{}'. Expected: flex-start, flex-end, center, space-between, space-around, space-evenly", val)
            }
            EncodingError::InvalidAlignItems(val) => {
                write!(f, "Invalid align-items value: '{}'. Expected: flex-start, flex-end, center, stretch, baseline", val)
            }
            EncodingError::InvalidFlexDirection(val) => {
                write!(f, "Invalid flex-direction value: '{}'. Expected: row, column, row-reverse, column-reverse", val)
            }
            EncodingError::InvalidDisplay(val) => {
                write!(f, "Invalid display value: '{}'. Expected: none, block, flex, grid, inline, inline-block", val)
            }
            EncodingError::InvalidPosition(val) => {
                write!(f, "Invalid position value: '{}'. Expected: static, relative, absolute, fixed, sticky", val)
            }
            EncodingError::InvalidU8Value { property, value } => {
                write!(f, "Invalid {} value: {}. Not a valid enum value", property, value)
            }
            EncodingError::UnsupportedProperty(prop) => {
                write!(f, "Property {} does not support enum encoding/decoding", prop)
            }
        }
    }
}

impl std::error::Error for EncodingError {}