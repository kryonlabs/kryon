//! Advanced assertion utilities for Kryon testing

use crate::prelude::*;
use std::collections::HashSet;

/// Property assertion utilities
pub mod properties {
    use super::*;

    /// Assert that an element has a specific property value
    pub fn assert_element_has_property(
        krb_file: &KrbFile,
        element_index: usize,
        property_name: &str,
        expected_value: &PropertyValue,
    ) -> Result<()> {
        let element = krb_file.elements.get(element_index)
            .ok_or_else(|| anyhow::anyhow!("Element {} not found", element_index))?;

        let actual_value = element.custom_properties.get(property_name)
            .ok_or_else(|| anyhow::anyhow!("Property '{}' not found on element {}", property_name, element_index))?;

        if actual_value != expected_value {
            bail!(
                "Property '{}' on element {} has value {:?}, expected {:?}",
                property_name, element_index, actual_value, expected_value
            );
        }

        Ok(())
    }

    /// Assert that any element has a specific property value
    pub fn assert_any_element_has_property(
        krb_file: &KrbFile,
        property_name: &str,
        expected_value: &PropertyValue,
    ) -> Result<()> {
        for (i, element) in krb_file.elements.iter().enumerate() {
            if let Some(value) = element.custom_properties.get(property_name) {
                if value == expected_value {
                    return Ok(());
                }
            }
        }

        bail!(
            "No element found with property '{}' = {:?}",
            property_name, expected_value
        );
    }

    /// Assert that elements have expected property count
    pub fn assert_property_count(
        krb_file: &KrbFile,
        element_index: usize,
        expected_count: usize,
    ) -> Result<()> {
        let element = krb_file.elements.get(element_index)
            .ok_or_else(|| anyhow::anyhow!("Element {} not found", element_index))?;

        let actual_count = element.custom_properties.len();
        if actual_count != expected_count {
            bail!(
                "Element {} has {} properties, expected {}",
                element_index, actual_count, expected_count
            );
        }

        Ok(())
    }
}

/// Element structure assertions
pub mod structure {
    use super::*;

    /// Assert element count in KRB file
    pub fn assert_element_count(krb_file: &KrbFile, expected_count: usize) -> Result<()> {
        let actual_count = krb_file.elements.len();
        if actual_count != expected_count {
            bail!("Expected {} elements, found {}", expected_count, actual_count);
        }
        Ok(())
    }

    /// Assert specific element type exists
    pub fn assert_element_type_exists(krb_file: &KrbFile, element_type: &str) -> Result<()> {
        let found = krb_file.elements.iter().any(|element| element.element_type == element_type);
        if !found {
            bail!("Element type '{}' not found", element_type);
        }
        Ok(())
    }

    /// Assert element type count
    pub fn assert_element_type_count(
        krb_file: &KrbFile,
        element_type: &str,
        expected_count: usize,
    ) -> Result<()> {
        let actual_count = krb_file.elements.iter()
            .filter(|element| element.element_type == element_type)
            .count();

        if actual_count != expected_count {
            bail!(
                "Expected {} elements of type '{}', found {}",
                expected_count, element_type, actual_count
            );
        }
        Ok(())
    }

    /// Assert element hierarchy structure
    pub fn assert_parent_child_relationship(
        krb_file: &KrbFile,
        parent_index: usize,
        child_index: usize,
    ) -> Result<()> {
        let parent = krb_file.elements.get(parent_index)
            .ok_or_else(|| anyhow::anyhow!("Parent element {} not found", parent_index))?;

        let child = krb_file.elements.get(child_index)
            .ok_or_else(|| anyhow::anyhow!("Child element {} not found", child_index))?;

        if !parent.children.contains(&(child_index as u32)) {
            bail!(
                "Element {} is not a child of element {}",
                child_index, parent_index
            );
        }

        Ok(())
    }
}

/// Text content assertions
pub mod text {
    use super::*;

    /// Assert text content in any element
    pub fn assert_text_content_exists(krb_file: &KrbFile, expected_text: &str) -> Result<()> {
        let found = krb_file.elements.iter().any(|element| {
            element.custom_properties.get("text") == Some(&PropertyValue::String(expected_text.to_string()))
        });

        if !found {
            bail!("Text content '{}' not found in any element", expected_text);
        }
        Ok(())
    }

    /// Assert text content in specific element
    pub fn assert_element_text_content(
        krb_file: &KrbFile,
        element_index: usize,
        expected_text: &str,
    ) -> Result<()> {
        let element = krb_file.elements.get(element_index)
            .ok_or_else(|| anyhow::anyhow!("Element {} not found", element_index))?;

        let actual_text = element.custom_properties.get("text")
            .and_then(|v| match v {
                PropertyValue::String(s) => Some(s.as_str()),
                _ => None,
            })
            .ok_or_else(|| anyhow::anyhow!("Element {} has no text property", element_index))?;

        if actual_text != expected_text {
            bail!(
                "Element {} has text '{}', expected '{}'",
                element_index, actual_text, expected_text
            );
        }

        Ok(())
    }

    /// Assert text content contains substring
    pub fn assert_text_contains(krb_file: &KrbFile, substring: &str) -> Result<()> {
        let found = krb_file.elements.iter().any(|element| {
            if let Some(PropertyValue::String(text)) = element.custom_properties.get("text") {
                text.contains(substring)
            } else {
                false
            }
        });

        if !found {
            bail!("No text content contains substring '{}'", substring);
        }
        Ok(())
    }
}

/// Layout and styling assertions
pub mod layout {
    use super::*;

    /// Assert layout property values
    pub fn assert_layout_property(
        krb_file: &KrbFile,
        element_index: usize,
        property: &str,
        expected_value: &str,
    ) -> Result<()> {
        properties::assert_element_has_property(
            krb_file,
            element_index,
            property,
            &PropertyValue::String(expected_value.to_string()),
        )
    }

    /// Assert flexbox layout configuration
    pub fn assert_flex_layout(
        krb_file: &KrbFile,
        element_index: usize,
        direction: &str,
        justify_content: Option<&str>,
        align_items: Option<&str>,
    ) -> Result<()> {
        // Check layout direction
        assert_layout_property(krb_file, element_index, "layout", direction)?;

        // Check justify_content if specified
        if let Some(justify) = justify_content {
            assert_layout_property(krb_file, element_index, "justify_content", justify)?;
        }

        // Check align_items if specified
        if let Some(align) = align_items {
            assert_layout_property(krb_file, element_index, "align_items", align)?;
        }

        Ok(())
    }

    /// Assert absolute positioning
    pub fn assert_absolute_position(
        krb_file: &KrbFile,
        element_index: usize,
        x: Option<i64>,
        y: Option<i64>,
    ) -> Result<()> {
        assert_layout_property(krb_file, element_index, "layout", "absolute")?;

        if let Some(x_pos) = x {
            properties::assert_element_has_property(
                krb_file,
                element_index,
                "x",
                &PropertyValue::Integer(x_pos),
            )?;
        }

        if let Some(y_pos) = y {
            properties::assert_element_has_property(
                krb_file,
                element_index,
                "y",
                &PropertyValue::Integer(y_pos),
            )?;
        }

        Ok(())
    }
}

/// Theme and styling assertions
pub mod theming {
    use super::*;

    /// Assert theme variable interpolation
    pub fn assert_theme_interpolation(
        krb_file: &KrbFile,
        property_name: &str,
        expected_interpolated_value: &str,
    ) -> Result<()> {
        properties::assert_any_element_has_property(
            krb_file,
            property_name,
            &PropertyValue::String(expected_interpolated_value.to_string()),
        )
    }

    /// Assert color property format
    pub fn assert_color_property(
        krb_file: &KrbFile,
        element_index: usize,
        property_name: &str,
        expected_color: &str,
    ) -> Result<()> {
        let element = krb_file.elements.get(element_index)
            .ok_or_else(|| anyhow::anyhow!("Element {} not found", element_index))?;

        let color_value = element.custom_properties.get(property_name)
            .ok_or_else(|| anyhow::anyhow!("Color property '{}' not found", property_name))?;

        if let PropertyValue::String(color) = color_value {
            if !color.starts_with('#') || color.len() < 7 {
                bail!("Color property '{}' has invalid format: '{}'", property_name, color);
            }
            if color != expected_color {
                bail!(
                    "Color property '{}' is '{}', expected '{}'",
                    property_name, color, expected_color
                );
            }
        } else {
            bail!("Property '{}' is not a color string", property_name);
        }

        Ok(())
    }
}

/// Performance and validation assertions
pub mod validation {
    use super::*;

    /// Assert compilation performance
    pub fn assert_compilation_time(actual_duration: Duration, max_duration: Duration) -> Result<()> {
        if actual_duration > max_duration {
            bail!(
                "Compilation took {:.2}ms, expected max {:.2}ms",
                actual_duration.as_secs_f64() * 1000.0,
                max_duration.as_secs_f64() * 1000.0
            );
        }
        Ok(())
    }

    /// Assert no duplicate element IDs
    pub fn assert_unique_element_ids(krb_file: &KrbFile) -> Result<()> {
        let mut seen_ids = HashSet::new();
        let mut duplicates = Vec::new();

        for element in &krb_file.elements {
            if let Some(PropertyValue::String(id)) = element.custom_properties.get("id") {
                if !seen_ids.insert(id.clone()) {
                    duplicates.push(id.clone());
                }
            }
        }

        if !duplicates.is_empty() {
            bail!("Duplicate element IDs found: {:?}", duplicates);
        }

        Ok(())
    }

    /// Assert KRB file structure integrity
    pub fn assert_krb_integrity(krb_file: &KrbFile) -> Result<()> {
        // Check for orphaned child references
        for (parent_idx, parent) in krb_file.elements.iter().enumerate() {
            for &child_id in &parent.children {
                let child_idx = child_id as usize;
                if child_idx >= krb_file.elements.len() {
                    bail!(
                        "Element {} references non-existent child {}",
                        parent_idx, child_idx
                    );
                }
            }
        }

        // Check for circular references (basic check)
        for (idx, element) in krb_file.elements.iter().enumerate() {
            if element.children.contains(&(idx as u32)) {
                bail!("Element {} contains itself as child (circular reference)", idx);
            }
        }

        Ok(())
    }
}

/// Utility macros for common assertions
#[macro_export]
macro_rules! assert_krb_element_count {
    ($krb_file:expr, $count:expr) => {
        crate::assertions::structure::assert_element_count($krb_file, $count)
            .with_context(|| format!("Element count assertion failed at {}:{}", file!(), line!()))
    };
}

#[macro_export]
macro_rules! assert_krb_text_content {
    ($krb_file:expr, $text:expr) => {
        crate::assertions::text::assert_text_content_exists($krb_file, $text)
            .with_context(|| format!("Text content assertion failed at {}:{}", file!(), line!()))
    };
}

#[macro_export]
macro_rules! assert_krb_property {
    ($krb_file:expr, $element_idx:expr, $prop:expr, $value:expr) => {
        crate::assertions::properties::assert_element_has_property($krb_file, $element_idx, $prop, $value)
            .with_context(|| format!("Property assertion failed at {}:{}", file!(), line!()))
    };
}

// Re-export macros
pub use {
    assert_krb_element_count,
    assert_krb_text_content,
    assert_krb_property,
};
