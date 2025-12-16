use serde::{Deserialize, Serialize};
use std::fmt;

/// Represents an RGB color
///
/// Colors are serialized as hex strings in the .kir format (e.g., "#1a1a2e")
/// but can be constructed from RGB components or hex strings.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(transparent)]
pub struct Color {
    /// Internal representation as hex string
    #[serde(with = "color_format")]
    pub value: u32,
}

impl Color {
    /// Create a color from RGB components (0-255)
    pub fn rgb(r: u8, g: u8, b: u8) -> Self {
        let value = ((r as u32) << 16) | ((g as u32) << 8) | (b as u32);
        Color { value }
    }

    /// Create a color from a hex value (0xRRGGBB)
    pub fn from_hex(hex: u32) -> Self {
        Color { value: hex & 0xFFFFFF }
    }

    /// Create a color from a hex string ("#RRGGBB" or "RRGGBB")
    pub fn from_hex_str(s: &str) -> Option<Self> {
        let s = s.strip_prefix('#').unwrap_or(s);
        u32::from_str_radix(s, 16)
            .ok()
            .map(|value| Color { value: value & 0xFFFFFF })
    }

    /// Get red component (0-255)
    pub fn r(&self) -> u8 {
        ((self.value >> 16) & 0xFF) as u8
    }

    /// Get green component (0-255)
    pub fn g(&self) -> u8 {
        ((self.value >> 8) & 0xFF) as u8
    }

    /// Get blue component (0-255)
    pub fn b(&self) -> u8 {
        (self.value & 0xFF) as u8
    }

    /// Get hex value (0xRRGGBB)
    pub fn as_hex(&self) -> u32 {
        self.value
    }

    // Common color constants
    pub const BLACK: Color = Color { value: 0x000000 };
    pub const WHITE: Color = Color { value: 0xFFFFFF };
    pub const RED: Color = Color { value: 0xFF0000 };
    pub const GREEN: Color = Color { value: 0x00FF00 };
    pub const BLUE: Color = Color { value: 0x0000FF };
    pub const TRANSPARENT: Color = Color { value: 0x000000 }; // Alpha handled separately
}

impl fmt::Display for Color {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "#{:06x}", self.value)
    }
}

impl From<u32> for Color {
    fn from(value: u32) -> Self {
        Color::from_hex(value)
    }
}

impl From<(u8, u8, u8)> for Color {
    fn from((r, g, b): (u8, u8, u8)) -> Self {
        Color::rgb(r, g, b)
    }
}

// Custom serde module for hex string serialization
mod color_format {
    use serde::{Deserialize, Deserializer, Serializer};

    pub fn serialize<S>(value: &u32, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(&format!("#{:06x}", value))
    }

    pub fn deserialize<'de, D>(deserializer: D) -> Result<u32, D::Error>
    where
        D: Deserializer<'de>,
    {
        let s = String::deserialize(deserializer)?;
        let s = s.strip_prefix('#').unwrap_or(&s);
        u32::from_str_radix(s, 16)
            .map(|v| v & 0xFFFFFF)
            .map_err(serde::de::Error::custom)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_color_from_rgb() {
        let color = Color::rgb(26, 26, 46);
        assert_eq!(color.r(), 26);
        assert_eq!(color.g(), 26);
        assert_eq!(color.b(), 46);
        assert_eq!(color.to_string(), "#1a1a2e");
    }

    #[test]
    fn test_color_from_hex() {
        let color = Color::from_hex(0x1a1a2e);
        assert_eq!(color.to_string(), "#1a1a2e");
    }

    #[test]
    fn test_color_from_hex_str() {
        let color1 = Color::from_hex_str("#1a1a2e").unwrap();
        let color2 = Color::from_hex_str("1a1a2e").unwrap();
        assert_eq!(color1, color2);
        assert_eq!(color1.to_string(), "#1a1a2e");
    }

    #[test]
    fn test_color_constants() {
        assert_eq!(Color::BLACK.to_string(), "#000000");
        assert_eq!(Color::WHITE.to_string(), "#ffffff");
        assert_eq!(Color::RED.to_string(), "#ff0000");
    }
}
