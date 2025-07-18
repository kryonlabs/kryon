
// FILE: src/core/util.rs

use std::fmt;
use crate::error::{CompilerError, Result};


/// Check if a string is a valid identifier
pub fn is_valid_identifier(name: &str) -> bool {
    if name.is_empty() {
        return false;
    }
    
    let mut chars = name.chars();
    let first = chars.next().unwrap();
    
    if !first.is_ascii_alphabetic() && first != '_' {
        return false;
    }
    
    chars.all(|c| c.is_ascii_alphanumeric() || c == '_')
}

/// Check if a string starting with # is a color value rather than a comment
fn is_color_value(value: &str) -> bool {
    if !value.starts_with('#') {
        return false;
    }
    
    let hex_part = &value[1..];
    
    // Check for valid hex color lengths (3, 4, 6, 8 characters)
    match hex_part.len() {
        3 | 4 | 6 | 8 => {
            // Check if all characters are valid hex digits
            hex_part.chars().all(|c| c.is_ascii_hexdigit())
        }
        _ => false,
    }
}


// Color utilities
#[derive(Debug, Clone, Copy)]
pub struct Color {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

impl Color {
    pub fn new(r: u8, g: u8, b: u8, a: u8) -> Self {
        Self { r, g, b, a }
    }

    pub fn to_bytes(&self) -> [u8; 4] {
        [self.r, self.g, self.b, self.a]
    }
}

impl fmt::Display for Color {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "#{:02X}{:02X}{:02X}{:02X}", self.r, self.g, self.b, self.a)
    }
}


/// Parse a color string (#RGB, #RRGGBB, #RRGGBBAA) or CSS color keywords
pub fn parse_color(color_str: &str) -> Result<Color> {
    let trimmed = color_str.trim().to_lowercase();
    
    // Handle CSS color keywords
    match trimmed.as_str() {
        "transparent" => return Ok(Color::new(0, 0, 0, 0)),
        _ => {}
    }
    
    if !trimmed.starts_with('#') {
        return Err(CompilerError::InvalidFormat {
            message: format!("Color must start with # or be a valid color keyword: {}", color_str),
        });
    }
    
    let hex_str = &trimmed[1..];
    
    match hex_str.len() {
        3 => {
            // RGB -> RRGGBB
            let r = u8::from_str_radix(&hex_str[0..1].repeat(2), 16).map_err(|_| {
                CompilerError::InvalidFormat {
                    message: format!("Invalid hex color: {}", color_str),
                }
            })?;
            let g = u8::from_str_radix(&hex_str[1..2].repeat(2), 16).map_err(|_| {
                CompilerError::InvalidFormat {
                    message: format!("Invalid hex color: {}", color_str),
                }
            })?;
            let b = u8::from_str_radix(&hex_str[2..3].repeat(2), 16).map_err(|_| {
                CompilerError::InvalidFormat {
                    message: format!("Invalid hex color: {}", color_str),
                }
            })?;
            Ok(Color::new(r, g, b, 255))
        }
        4 => {
            // RGBA -> RRGGBBAA
            let r = u8::from_str_radix(&hex_str[0..1].repeat(2), 16).map_err(|_| {
                CompilerError::InvalidFormat {
                    message: format!("Invalid hex color: {}", color_str),
                }
            })?;
            let g = u8::from_str_radix(&hex_str[1..2].repeat(2), 16).map_err(|_| {
                CompilerError::InvalidFormat {
                    message: format!("Invalid hex color: {}", color_str),
                }
            })?;
            let b = u8::from_str_radix(&hex_str[2..3].repeat(2), 16).map_err(|_| {
                CompilerError::InvalidFormat {
                    message: format!("Invalid hex color: {}", color_str),
                }
            })?;
            let a = u8::from_str_radix(&hex_str[3..4].repeat(2), 16).map_err(|_| {
                CompilerError::InvalidFormat {
                    message: format!("Invalid hex color: {}", color_str),
                }
            })?;
            Ok(Color::new(r, g, b, a))
        }
        6 => {
            // RRGGBB
            let r = u8::from_str_radix(&hex_str[0..2], 16).map_err(|_| {
                CompilerError::InvalidFormat {
                    message: format!("Invalid hex color: {}", color_str),
                }
            })?;
            let g = u8::from_str_radix(&hex_str[2..4], 16).map_err(|_| {
                CompilerError::InvalidFormat {
                    message: format!("Invalid hex color: {}", color_str),
                }
            })?;
            let b = u8::from_str_radix(&hex_str[4..6], 16).map_err(|_| {
                CompilerError::InvalidFormat {
                    message: format!("Invalid hex color: {}", color_str),
                }
            })?;
            Ok(Color::new(r, g, b, 255))
        }
        8 => {
            // RRGGBBAA
            let r = u8::from_str_radix(&hex_str[0..2], 16).map_err(|_| {
                CompilerError::InvalidFormat {
                    message: format!("Invalid hex color: {}", color_str),
                }
            })?;
            let g = u8::from_str_radix(&hex_str[2..4], 16).map_err(|_| {
                CompilerError::InvalidFormat {
                    message: format!("Invalid hex color: {}", color_str),
                }
            })?;
            let b = u8::from_str_radix(&hex_str[4..6], 16).map_err(|_| {
                CompilerError::InvalidFormat {
                    message: format!("Invalid hex color: {}", color_str),
                }
            })?;
            let a = u8::from_str_radix(&hex_str[6..8], 16).map_err(|_| {
                CompilerError::InvalidFormat {
                    message: format!("Invalid hex color: {}", color_str),
                }
            })?;
            Ok(Color::new(r, g, b, a))
        }
        _ => Err(CompilerError::InvalidFormat {
            message: format!("Invalid color format: {} (expected 3, 4, 6, or 8 hex digits)", color_str),
        }),
    }
}


/// Clean and parse a quoted or unquoted value string
pub fn clean_and_quote_value(value: &str) -> (String, bool) {
    let trimmed = value.trim();
    
    // Check for full-line comment (but not if it's quoted or a color value)
    if trimmed.starts_with('#') && !trimmed.starts_with("\"#") && !is_color_value(trimmed) {
        return (String::new(), false);
    }
    
    // Find comment (not inside quotes)
    let mut in_quotes = false;
    let mut comment_index = None;
    
    for (i, ch) in trimmed.char_indices() {
        if ch == '"' {
            in_quotes = !in_quotes;
        }
        if ch == '#' && !in_quotes && i > 0 {
            comment_index = Some(i);
            break;
        }
    }
    
    let value_part = if let Some(idx) = comment_index {
        trimmed[..idx].trim_end()
    } else {
        trimmed
    };
    
    // Check if quoted
    let was_quoted = value_part.len() >= 2 
        && value_part.starts_with('"') 
        && value_part.ends_with('"');
    
    let cleaned = if was_quoted {
        value_part[1..value_part.len()-1].to_string()
    } else {
        value_part.to_string()
    };
    
    (cleaned, was_quoted)
}



/// Split properties string by semicolon, respecting quotes
pub fn split_properties_by_semicolon(props_str: &str) -> Vec<String> {
    let mut properties = Vec::new();
    let mut current = String::new();
    let mut in_quotes = false;
    let mut escape_next = false;
    
    for ch in props_str.chars() {
        if escape_next {
            current.push(ch);
            escape_next = false;
            continue;
        }
        
        if ch == '\\' {
            current.push(ch);
            escape_next = true;
            continue;
        }
        
        if ch == '"' {
            in_quotes = !in_quotes;
            current.push(ch);
            continue;
        }
        
        if ch == ';' && !in_quotes {
            let trimmed = current.trim();
            if !trimmed.is_empty() {
                properties.push(trimmed.to_string());
            }
            current.clear();
            continue;
        }
        
        current.push(ch);
    }
    
    let trimmed = current.trim();
    if !trimmed.is_empty() {
        properties.push(trimmed.to_string());
    }
    
    properties
}

