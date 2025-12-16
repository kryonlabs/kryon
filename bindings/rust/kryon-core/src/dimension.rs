use serde::{Deserialize, Serialize};
use std::fmt;

/// Represents a dimension value (width, height, padding, etc.)
///
/// Dimensions can be specified in multiple units:
/// - Pixels: `Dimension::Px(100.0)` → "100.0px"
/// - Percent: `Dimension::Percent(50.0)` → "50.0%"
/// - Auto: `Dimension::Auto` → "auto"
/// - Flex: `Dimension::Flex(1.0)` → "1.0fr"
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum Dimension {
    /// Pixel value (e.g., "100.0px")
    Px(f32),
    /// Percentage value (e.g., "50.0%")
    Percent(f32),
    /// Auto-sizing
    Auto,
    /// Flex units
    Flex(f32),
}

impl Dimension {
    /// Create a pixel dimension
    pub fn px(value: f32) -> Self {
        Dimension::Px(value)
    }

    /// Create a percentage dimension
    pub fn percent(value: f32) -> Self {
        Dimension::Percent(value)
    }

    /// Create an auto dimension
    pub fn auto() -> Self {
        Dimension::Auto
    }

    /// Create a flex dimension
    pub fn flex(value: f32) -> Self {
        Dimension::Flex(value)
    }
}

impl fmt::Display for Dimension {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Dimension::Px(v) => write!(f, "{}px", v),
            Dimension::Percent(v) => write!(f, "{}%", v),
            Dimension::Auto => write!(f, "auto"),
            Dimension::Flex(v) => write!(f, "{}fr", v),
        }
    }
}

impl From<f32> for Dimension {
    fn from(value: f32) -> Self {
        Dimension::Px(value)
    }
}

impl From<&str> for Dimension {
    fn from(s: &str) -> Self {
        if s == "auto" {
            return Dimension::Auto;
        }

        if let Some(px) = s.strip_suffix("px") {
            if let Ok(value) = px.parse::<f32>() {
                return Dimension::Px(value);
            }
        }

        if let Some(pct) = s.strip_suffix('%') {
            if let Ok(value) = pct.parse::<f32>() {
                return Dimension::Percent(value);
            }
        }

        if let Some(fr) = s.strip_suffix("fr") {
            if let Ok(value) = fr.parse::<f32>() {
                return Dimension::Flex(value);
            }
        }

        // Default to auto if parsing fails
        Dimension::Auto
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_dimension_display() {
        assert_eq!(Dimension::px(100.0).to_string(), "100px");
        assert_eq!(Dimension::percent(50.0).to_string(), "50%");
        assert_eq!(Dimension::auto().to_string(), "auto");
        assert_eq!(Dimension::flex(1.0).to_string(), "1fr");
    }

    #[test]
    fn test_dimension_from_str() {
        assert_eq!(Dimension::from("100.0px"), Dimension::Px(100.0));
        assert_eq!(Dimension::from("50.0%"), Dimension::Percent(50.0));
        assert_eq!(Dimension::from("auto"), Dimension::Auto);
        assert_eq!(Dimension::from("1.0fr"), Dimension::Flex(1.0));
    }
}
