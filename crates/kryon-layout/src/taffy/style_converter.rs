//! Style conversion utilities for Taffy layout engine
//!
//! This module handles the conversion from Kryon's KRB element properties
//! to Taffy's style system.

use kryon_core::{Element, PropertyValue};
use taffy::prelude::*;

/// Converts KRB element properties to Taffy styles
pub struct StyleConverter;

impl StyleConverter {
    /// Convert a KRB element to a Taffy style
    pub fn krb_to_taffy_style(element: &Element) -> Style {
        let mut style = Style::DEFAULT;

        // Basic sizing from LayoutDimension
        Self::apply_layout_size(&mut style, element);

        // Apply layout flags
        Self::apply_layout_flags(&mut style, element);

        // Apply custom properties
        Self::apply_custom_properties(&mut style, element);

        // Apply element-specific styles
        Self::apply_element_type_styles(&mut style, element);

        style
    }

    /// Apply layout size from LayoutDimension
    fn apply_layout_size(style: &mut Style, element: &Element) {
        use kryon_core::LayoutDimension;
        
        // Convert LayoutDimension to Taffy Dimension
        match &element.layout_size.width {
            LayoutDimension::Pixels(px) => {
                if *px > 0.0 {
                    style.size.width = Dimension::Length(*px);
                }
            }
            LayoutDimension::Percentage(pct) => {
                style.size.width = Dimension::Percent(*pct);
            }
            LayoutDimension::Auto => {
                style.size.width = Dimension::Auto;
            }
            LayoutDimension::MinPixels(px) => {
                style.min_size.width = Dimension::Length(*px);
            }
            LayoutDimension::MaxPixels(px) => {
                style.max_size.width = Dimension::Length(*px);
            }
        }

        match &element.layout_size.height {
            LayoutDimension::Pixels(px) => {
                if *px > 0.0 {
                    style.size.height = Dimension::Length(*px);
                }
            }
            LayoutDimension::Percentage(pct) => {
                style.size.height = Dimension::Percent(*pct);
            }
            LayoutDimension::Auto => {
                style.size.height = Dimension::Auto;
            }
            LayoutDimension::MinPixels(px) => {
                style.min_size.height = Dimension::Length(*px);
            }
            LayoutDimension::MaxPixels(px) => {
                style.max_size.height = Dimension::Length(*px);
            }
        }
    }

    /// Apply layout position from LayoutDimension
    fn apply_layout_position(style: &mut Style, element: &Element) {
        use kryon_core::LayoutDimension;
        
        // Convert LayoutDimension to Taffy LengthPercentageAuto
        match &element.layout_position.x {
            LayoutDimension::Pixels(px) => {
                if *px != 0.0 {
                    style.inset.left = LengthPercentageAuto::Length(*px);
                }
            }
            LayoutDimension::Percentage(pct) => {
                style.inset.left = LengthPercentageAuto::Percent(*pct);
            }
            LayoutDimension::Auto => {
                style.inset.left = LengthPercentageAuto::Auto;
            }
            _ => {}
        }

        match &element.layout_position.y {
            LayoutDimension::Pixels(px) => {
                if *px != 0.0 {
                    style.inset.top = LengthPercentageAuto::Length(*px);
                }
            }
            LayoutDimension::Percentage(pct) => {
                style.inset.top = LengthPercentageAuto::Percent(*pct);
            }
            LayoutDimension::Auto => {
                style.inset.top = LengthPercentageAuto::Auto;
            }
            _ => {}
        }
    }

    /// Apply layout flags to Taffy style
    fn apply_layout_flags(style: &mut Style, element: &Element) {
        let flags = element.layout_flags;
        
        eprintln!("[STYLE_LAYOUT] Element {} flags: {:#04x}", element.id, flags);
        
        // Layout direction
        if flags & 0x01 != 0 {
            style.flex_direction = FlexDirection::Column;
            eprintln!("[STYLE_LAYOUT] Element {} set to column layout", element.id);
        } else {
            style.flex_direction = FlexDirection::Row;
            eprintln!("[STYLE_LAYOUT] Element {} set to row layout", element.id);
        }

        // Absolute positioning
        if flags & 0x02 != 0 {
            style.position = Position::Absolute;
            eprintln!("[STYLE_LAYOUT] Element {} set to absolute position", element.id);
            
            // Apply position from LayoutDimension
            Self::apply_layout_position(style, element);
        }

        // Centering
        if flags & 0x04 != 0 {
            style.justify_content = Some(JustifyContent::Center);
            style.align_items = Some(AlignItems::Center);
            eprintln!("[STYLE_LAYOUT] Element {} set to center alignment", element.id);
        }

        // Flex grow
        if flags & 0x20 != 0 {
            style.flex_grow = 1.0;
            eprintln!("[STYLE_LAYOUT] Element {} set to flex grow", element.id);
        }
    }

    /// Apply custom properties to Taffy style
    fn apply_custom_properties(style: &mut Style, element: &Element) {
        // Display property
        if let Some(PropertyValue::String(value)) = element.custom_properties.get("display") {
            style.display = match value.as_str() {
                "grid" => Display::Grid,
                "flex" => Display::Flex,
                "block" => Display::Block,
                "none" => Display::None,
                _ => style.display,
            };
        }

        // Flexbox properties
        if let Some(PropertyValue::String(value)) = element.custom_properties.get("flex_direction") {
            style.flex_direction = match value.as_str() {
                "row" => FlexDirection::Row,
                "column" => FlexDirection::Column,
                "row-reverse" => FlexDirection::RowReverse,
                "column-reverse" => FlexDirection::ColumnReverse,
                _ => style.flex_direction,
            };
        }

        if let Some(PropertyValue::String(value)) = element.custom_properties.get("justify_content") {
            style.justify_content = match value.as_str() {
                "flex-start" | "start" => Some(JustifyContent::FlexStart),
                "flex-end" | "end" => Some(JustifyContent::FlexEnd),
                "center" => Some(JustifyContent::Center),
                "space-between" => Some(JustifyContent::SpaceBetween),
                "space-around" => Some(JustifyContent::SpaceAround),
                "space-evenly" => Some(JustifyContent::SpaceEvenly),
                _ => style.justify_content,
            };
        }

        if let Some(PropertyValue::String(value)) = element.custom_properties.get("align_items") {
            style.align_items = match value.as_str() {
                "flex-start" | "start" => Some(AlignItems::FlexStart),
                "flex-end" | "end" => Some(AlignItems::FlexEnd),
                "center" => Some(AlignItems::Center),
                "baseline" => Some(AlignItems::Baseline),
                "stretch" => Some(AlignItems::Stretch),
                _ => style.align_items,
            };
        }

        // Numeric properties
        if let Some(PropertyValue::Float(value)) = element.custom_properties.get("flex_grow") {
            style.flex_grow = *value;
        }

        if let Some(PropertyValue::Float(value)) = element.custom_properties.get("flex_shrink") {
            style.flex_shrink = *value;
        }

        // Box model properties
        if let Some(PropertyValue::Float(value)) = element.custom_properties.get("margin") {
            let margin = LengthPercentageAuto::Length(*value);
            style.margin = taffy::Rect {
                left: margin,
                right: margin,
                top: margin,
                bottom: margin,
            };
        }

        if let Some(PropertyValue::Float(value)) = element.custom_properties.get("padding") {
            let padding = LengthPercentage::Length(*value);
            style.padding = taffy::Rect {
                left: padding,
                right: padding,
                top: padding,
                bottom: padding,
            };
        }

        // Width and height properties
        if let Some(PropertyValue::Float(value)) = element.custom_properties.get("width") {
            style.size.width = Dimension::Length(*value);
        }

        if let Some(PropertyValue::Float(value)) = element.custom_properties.get("height") {
            style.size.height = Dimension::Length(*value);
        }
    }

    /// Apply element-type specific styles
    fn apply_element_type_styles(style: &mut Style, element: &Element) {
        use kryon_core::LayoutDimension;
        
        match element.element_type {
            kryon_core::ElementType::Text | kryon_core::ElementType::Link => {
                style.display = Display::Block;
                // Check if height is auto and set default text height
                if matches!(element.layout_size.height, LayoutDimension::Auto) {
                    let text_height = element.font_size.max(16.0);
                    style.size.height = Dimension::Length(text_height);
                }
            }
            kryon_core::ElementType::Button => {
                style.display = Display::Block;
                style.flex_grow = 0.0;
                style.flex_shrink = 0.0;
                
                // Preserve button dimensions based on LayoutDimension
                match &element.layout_size.width {
                    LayoutDimension::Pixels(px) if *px > 0.0 => {
                        style.min_size.width = Dimension::Length(*px);
                        style.max_size.width = Dimension::Length(*px);
                    }
                    _ => {}
                }
                
                match &element.layout_size.height {
                    LayoutDimension::Pixels(px) if *px > 0.0 => {
                        style.min_size.height = Dimension::Length(*px);
                        style.max_size.height = Dimension::Length(*px);
                    }
                    _ => {}
                }
            }
            kryon_core::ElementType::App => {
                if !element.custom_properties.contains_key("display") {
                    style.display = Display::Flex;
                }
                if !element.custom_properties.contains_key("flex_direction") {
                    style.flex_direction = FlexDirection::Column;
                }
                if !element.custom_properties.contains_key("align_items") {
                    style.align_items = Some(AlignItems::Center);
                }
                if !element.custom_properties.contains_key("justify_content") {
                    style.justify_content = Some(JustifyContent::Center);
                }
            }
            kryon_core::ElementType::Container => {
                // Check if container should be flex
                let should_be_flex = element.custom_properties.contains_key("flex_direction") ||
                                   element.custom_properties.contains_key("justify_content") ||
                                   element.custom_properties.contains_key("align_items") ||
                                   element.layout_flags != 0;
                
                if should_be_flex {
                    style.display = Display::Flex;
                }
            }
            kryon_core::ElementType::Input => {
                style.display = Display::Block;
                
                // Get input type to determine default sizes
                let input_type = element.custom_properties.get("input_type")
                    .and_then(|v| match v {
                        PropertyValue::String(s) => Some(s.as_str()),
                        _ => None,
                    })
                    .unwrap_or("text");
                
                // Apply default sizes if not explicitly set
                match input_type {
                    "checkbox" => {
                        // Default checkbox size: 20x20px
                        if matches!(element.layout_size.width, LayoutDimension::Auto) {
                            style.size.width = Dimension::Length(20.0);
                        }
                        if matches!(element.layout_size.height, LayoutDimension::Auto) {
                            style.size.height = Dimension::Length(20.0);
                        }
                    }
                    "radio" => {
                        // Default radio button size: 20x20px
                        if matches!(element.layout_size.width, LayoutDimension::Auto) {
                            style.size.width = Dimension::Length(20.0);
                        }
                        if matches!(element.layout_size.height, LayoutDimension::Auto) {
                            style.size.height = Dimension::Length(20.0);
                        }
                    }
                    _ => {
                        // Default text input size: auto width, height based on font size
                        if matches!(element.layout_size.height, LayoutDimension::Auto) {
                            let input_height = (element.font_size + 8.0).max(24.0); // Add padding
                            style.size.height = Dimension::Length(input_height);
                        }
                        if matches!(element.layout_size.width, LayoutDimension::Auto) {
                            // Default text input width: 200px
                            style.size.width = Dimension::Length(200.0);
                        }
                    }
                }
            }
            _ => {}
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kryon_core::ElementType;

    #[test]
    fn test_basic_style_conversion() {
        let mut element = Element::new(ElementType::Container);
        element.layout_size = LayoutSize::pixels(100.0, 200.0);
        element.layout_flags = 0x01; // Column layout
        
        let style = StyleConverter::krb_to_taffy_style(&element);
        
        assert_eq!(style.size.width, Dimension::Length(100.0));
        assert_eq!(style.size.height, Dimension::Length(200.0));
        assert_eq!(style.flex_direction, FlexDirection::Column);
    }

    #[test]
    fn test_custom_properties() {
        let mut element = Element::new(ElementType::Container);
        element.custom_properties.insert("display".to_string(), PropertyValue::String("flex".to_string()));
        element.custom_properties.insert("flex_grow".to_string(), PropertyValue::Float(1.0));
        
        let style = StyleConverter::krb_to_taffy_style(&element);
        
        assert_eq!(style.display, Display::Flex);
        assert_eq!(style.flex_grow, 1.0);
    }

    #[test]
    fn test_element_type_styles() {
        let mut text_element = Element::new(ElementType::Text);
        text_element.font_size = 20.0;
        
        let style = StyleConverter::krb_to_taffy_style(&text_element);
        
        assert_eq!(style.display, Display::Block);
        assert_eq!(style.size.height, Dimension::Length(20.0));
    }
}