//! Optimized property access system for high-performance layout computation
//!
//! This module provides O(1) property access with minimal memory overhead
//! and cache-friendly data structures for production use.

use crate::{Element, ElementId, PropertyValue};
use std::collections::HashMap;

/// Fast property access cache with optimized layout
#[derive(Debug, Clone)]
pub struct OptimizedPropertyCache {
    /// Pre-computed layout-critical properties for fast access
    layout_properties: Vec<LayoutProperties>,
    /// Element ID to index mapping
    element_to_index: HashMap<ElementId, usize>,
    /// Generation counter for cache invalidation
    generation: u64,
}

/// Layout-critical properties stored in cache-friendly format
#[derive(Debug, Clone, Default)]
pub struct LayoutProperties {
    /// Element ID
    pub element_id: ElementId,
    /// Element size (width, height)
    pub size: [f32; 2],
    /// Element position (x, y)
    pub position: [f32; 2],
    /// Layout flags (packed into single byte)
    pub layout_flags: u8,
    /// Flexbox properties (packed)
    pub flex_properties: FlexProperties,
    /// Box model properties (packed)
    pub box_model: BoxModel,
    /// Display mode
    pub display_mode: DisplayMode,
    /// Visibility state
    pub visible: bool,
    /// Has custom properties flag
    pub has_custom_properties: bool,
}

/// Packed flexbox properties for efficient storage
#[derive(Debug, Clone, Copy, Default)]
pub struct FlexProperties {
    /// Flex grow (0-255 mapped to 0.0-1.0)
    pub grow: u8,
    /// Flex shrink (0-255 mapped to 0.0-1.0)
    pub shrink: u8,
    /// Flex direction (2 bits) + justify content (3 bits) + align items (3 bits)
    pub direction_and_alignment: u8,
}

/// Packed box model properties
#[derive(Debug, Clone, Copy, Default)]
pub struct BoxModel {
    /// Margin (top, right, bottom, left) in pixels
    pub margin: [f32; 4],
    /// Padding (top, right, bottom, left) in pixels
    pub padding: [f32; 4],
    /// Border width
    pub border_width: f32,
}

/// Display mode enumeration
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum DisplayMode {
    #[default]
    Block,
    Flex,
    Grid,
    None,
}

impl OptimizedPropertyCache {
    /// Create new property cache
    pub fn new() -> Self {
        Self {
            layout_properties: Vec::new(),
            element_to_index: HashMap::new(),
            generation: 0,
        }
    }

    /// Create property cache with initial capacity
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            layout_properties: Vec::with_capacity(capacity),
            element_to_index: HashMap::with_capacity(capacity),
            generation: 0,
        }
    }

    /// Update cache from elements (optimized bulk operation)
    pub fn update_from_elements(&mut self, elements: &HashMap<ElementId, Element>) {
        // Clear existing data
        self.layout_properties.clear();
        self.element_to_index.clear();
        self.generation += 1;

        // Reserve capacity for better performance
        self.layout_properties.reserve(elements.len());
        self.element_to_index.reserve(elements.len());

        // Process elements in sorted order for cache efficiency
        let mut sorted_elements: Vec<_> = elements.iter().collect();
        sorted_elements.sort_by_key(|(id, _)| *id);

        for (index, (&element_id, element)) in sorted_elements.iter().enumerate() {
            let layout_props = self.extract_layout_properties(element_id, element);
            self.layout_properties.push(layout_props);
            self.element_to_index.insert(element_id, index);
        }
    }

    /// Extract layout properties from element (optimized)
    fn extract_layout_properties(&self, element_id: ElementId, element: &Element) -> LayoutProperties {
        let mut props = LayoutProperties {
            element_id,
            size: [element.layout_size.width.to_pixels(1.0), element.layout_size.height.to_pixels(1.0)],
            position: [element.layout_position.x.to_pixels(1.0), element.layout_position.y.to_pixels(1.0)],
            layout_flags: element.layout_flags,
            visible: element.visible,
            has_custom_properties: !element.custom_properties.is_empty(),
            ..Default::default()
        };

        // Extract flex properties
        props.flex_properties = self.extract_flex_properties(element);

        // Extract box model
        props.box_model = self.extract_box_model(element);

        // Extract display mode
        props.display_mode = self.extract_display_mode(element);

        props
    }

    /// Extract flex properties efficiently
    fn extract_flex_properties(&self, element: &Element) -> FlexProperties {
        let mut flex_props = FlexProperties::default();

        // Extract flex grow/shrink
        if let Some(PropertyValue::Float(grow)) = element.custom_properties.get("flex_grow") {
            flex_props.grow = (grow.clamp(0.0, 1.0) * 255.0) as u8;
        }

        if let Some(PropertyValue::Float(shrink)) = element.custom_properties.get("flex_shrink") {
            flex_props.shrink = (shrink.clamp(0.0, 1.0) * 255.0) as u8;
        }

        // Pack direction and alignment into single byte
        let mut packed = 0u8;

        // Flex direction (2 bits)
        if let Some(PropertyValue::String(direction)) = element.custom_properties.get("flex_direction") {
            packed |= match direction.as_str() {
                "row" => 0,
                "column" => 1,
                "row-reverse" => 2,
                "column-reverse" => 3,
                _ => 0,
            };
        }

        // Justify content (3 bits, shifted left 2)
        if let Some(PropertyValue::String(justify)) = element.custom_properties.get("justify_content") {
            let justify_value = match justify.as_str() {
                "flex-start" | "start" => 0,
                "flex-end" | "end" => 1,
                "center" => 2,
                "space-between" => 3,
                "space-around" => 4,
                "space-evenly" => 5,
                _ => 0,
            };
            packed |= justify_value << 2;
        }

        // Align items (3 bits, shifted left 5)
        if let Some(PropertyValue::String(align)) = element.custom_properties.get("align_items") {
            let align_value = match align.as_str() {
                "flex-start" | "start" => 0,
                "flex-end" | "end" => 1,
                "center" => 2,
                "baseline" => 3,
                "stretch" => 4,
                _ => 0,
            };
            packed |= align_value << 5;
        }

        flex_props.direction_and_alignment = packed;
        flex_props
    }

    /// Extract box model efficiently
    fn extract_box_model(&self, element: &Element) -> BoxModel {
        let mut box_model = BoxModel::default();

        // Extract margin
        if let Some(PropertyValue::Float(margin)) = element.custom_properties.get("margin") {
            box_model.margin = [*margin; 4];
        } else {
            // Individual margin properties
            if let Some(PropertyValue::Float(top)) = element.custom_properties.get("margin_top") {
                box_model.margin[0] = *top;
            }
            if let Some(PropertyValue::Float(right)) = element.custom_properties.get("margin_right") {
                box_model.margin[1] = *right;
            }
            if let Some(PropertyValue::Float(bottom)) = element.custom_properties.get("margin_bottom") {
                box_model.margin[2] = *bottom;
            }
            if let Some(PropertyValue::Float(left)) = element.custom_properties.get("margin_left") {
                box_model.margin[3] = *left;
            }
        }

        // Extract padding
        if let Some(PropertyValue::Float(padding)) = element.custom_properties.get("padding") {
            box_model.padding = [*padding; 4];
        } else {
            // Individual padding properties
            if let Some(PropertyValue::Float(top)) = element.custom_properties.get("padding_top") {
                box_model.padding[0] = *top;
            }
            if let Some(PropertyValue::Float(right)) = element.custom_properties.get("padding_right") {
                box_model.padding[1] = *right;
            }
            if let Some(PropertyValue::Float(bottom)) = element.custom_properties.get("padding_bottom") {
                box_model.padding[2] = *bottom;
            }
            if let Some(PropertyValue::Float(left)) = element.custom_properties.get("padding_left") {
                box_model.padding[3] = *left;
            }
        }

        // Extract border width
        if let Some(PropertyValue::Float(border)) = element.custom_properties.get("border_width") {
            box_model.border_width = *border;
        }

        box_model
    }

    /// Extract display mode efficiently
    fn extract_display_mode(&self, element: &Element) -> DisplayMode {
        if let Some(PropertyValue::String(display)) = element.custom_properties.get("display") {
            match display.as_str() {
                "flex" => DisplayMode::Flex,
                "grid" => DisplayMode::Grid,
                "none" => DisplayMode::None,
                _ => DisplayMode::Block,
            }
        } else {
            // Default based on element type
            match element.element_type {
                crate::ElementType::Container | crate::ElementType::App => DisplayMode::Flex,
                _ => DisplayMode::Block,
            }
        }
    }

    /// Get layout properties for element (O(1) access)
    pub fn get_layout_properties(&self, element_id: ElementId) -> Option<&LayoutProperties> {
        self.element_to_index.get(&element_id)
            .and_then(|&index| self.layout_properties.get(index))
    }

    /// Get element size efficiently
    pub fn get_size(&self, element_id: ElementId) -> Option<[f32; 2]> {
        self.get_layout_properties(element_id).map(|props| props.size)
    }

    /// Get element position efficiently
    pub fn get_position(&self, element_id: ElementId) -> Option<[f32; 2]> {
        self.get_layout_properties(element_id).map(|props| props.position)
    }

    /// Get layout flags efficiently
    pub fn get_layout_flags(&self, element_id: ElementId) -> Option<u8> {
        self.get_layout_properties(element_id).map(|props| props.layout_flags)
    }

    /// Get flex properties efficiently
    pub fn get_flex_properties(&self, element_id: ElementId) -> Option<FlexProperties> {
        self.get_layout_properties(element_id).map(|props| props.flex_properties)
    }

    /// Get box model efficiently
    pub fn get_box_model(&self, element_id: ElementId) -> Option<BoxModel> {
        self.get_layout_properties(element_id).map(|props| props.box_model)
    }

    /// Get display mode efficiently
    pub fn get_display_mode(&self, element_id: ElementId) -> Option<DisplayMode> {
        self.get_layout_properties(element_id).map(|props| props.display_mode)
    }

    /// Check if element is visible
    pub fn is_visible(&self, element_id: ElementId) -> bool {
        self.get_layout_properties(element_id)
            .map(|props| props.visible)
            .unwrap_or(true)
    }

    /// Get cache statistics
    pub fn get_stats(&self) -> CacheStats {
        CacheStats {
            element_count: self.layout_properties.len(),
            memory_usage: self.layout_properties.capacity() * std::mem::size_of::<LayoutProperties>(),
            generation: self.generation,
        }
    }

    /// Clear cache
    pub fn clear(&mut self) {
        self.layout_properties.clear();
        self.element_to_index.clear();
        self.generation += 1;
    }

    /// Batch update for multiple elements
    pub fn update_elements(&mut self, updates: &[(ElementId, &Element)]) {
        for &(element_id, element) in updates {
            if let Some(&index) = self.element_to_index.get(&element_id) {
                let new_props = self.extract_layout_properties(element_id, element);
                if let Some(props) = self.layout_properties.get_mut(index) {
                    *props = new_props;
                }
            }
        }
        self.generation += 1;
    }
}

/// Cache performance statistics
#[derive(Debug, Clone)]
pub struct CacheStats {
    pub element_count: usize,
    pub memory_usage: usize,
    pub generation: u64,
}

/// Flex properties helper methods
impl FlexProperties {
    /// Get flex grow as f32
    pub fn flex_grow(&self) -> f32 {
        self.grow as f32 / 255.0
    }

    /// Get flex shrink as f32
    pub fn flex_shrink(&self) -> f32 {
        self.shrink as f32 / 255.0
    }

    /// Get flex direction
    pub fn flex_direction(&self) -> u8 {
        self.direction_and_alignment & 0b11
    }

    /// Get justify content
    pub fn justify_content(&self) -> u8 {
        (self.direction_and_alignment >> 2) & 0b111
    }

    /// Get align items
    pub fn align_items(&self) -> u8 {
        (self.direction_and_alignment >> 5) & 0b111
    }
}

/// Box model helper methods
impl BoxModel {
    /// Get total horizontal margin
    pub fn horizontal_margin(&self) -> f32 {
        self.margin[1] + self.margin[3] // right + left
    }

    /// Get total vertical margin
    pub fn vertical_margin(&self) -> f32 {
        self.margin[0] + self.margin[2] // top + bottom
    }

    /// Get total horizontal padding
    pub fn horizontal_padding(&self) -> f32 {
        self.padding[1] + self.padding[3] // right + left
    }

    /// Get total vertical padding
    pub fn vertical_padding(&self) -> f32 {
        self.padding[0] + self.padding[2] // top + bottom
    }

    /// Get total horizontal border
    pub fn horizontal_border(&self) -> f32 {
        self.border_width * 2.0
    }

    /// Get total vertical border
    pub fn vertical_border(&self) -> f32 {
        self.border_width * 2.0
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ElementType;

    #[test]
    fn test_property_cache_basic() {
        let mut cache = OptimizedPropertyCache::new();
        
        let mut elements = HashMap::new();
        let mut element = Element::new(ElementType::Container);
        element.layout_size = LayoutSize::pixels(100.0, 200.0);
        element.layout_position = LayoutPosition::pixels(10.0, 20.0);
        elements.insert(0, element);

        cache.update_from_elements(&elements);

        let props = cache.get_layout_properties(0).unwrap();
        assert_eq!(props.size, [100.0, 200.0]);
        assert_eq!(props.position, [10.0, 20.0]);
    }

    #[test]
    fn test_flex_properties_packing() {
        let mut element = Element::new(ElementType::Container);
        element.custom_properties.insert("flex_grow".to_string(), PropertyValue::Float(0.5));
        element.custom_properties.insert("flex_direction".to_string(), PropertyValue::String("column".to_string()));
        element.custom_properties.insert("justify_content".to_string(), PropertyValue::String("center".to_string()));

        let cache = OptimizedPropertyCache::new();
        let flex_props = cache.extract_flex_properties(&element);

        assert_eq!(flex_props.flex_grow(), 0.5);
        assert_eq!(flex_props.flex_direction(), 1); // column
        assert_eq!(flex_props.justify_content(), 2); // center
    }

    #[test]
    fn test_cache_performance() {
        let mut cache = OptimizedPropertyCache::with_capacity(1000);
        
        let mut elements = HashMap::new();
        for i in 0..1000 {
            let mut element = Element::new(ElementType::Container);
            element.layout_size = LayoutSize::pixels(i as f32, i as f32);
            elements.insert(i, element);
        }

        let start = std::time::Instant::now();
        cache.update_from_elements(&elements);
        let update_time = start.elapsed();

        let start = std::time::Instant::now();
        for i in 0..1000 {
            let _props = cache.get_layout_properties(i);
        }
        let access_time = start.elapsed();

        println!("Update time: {:?}, Access time: {:?}", update_time, access_time);
        
        // Basic performance assertions
        assert!(update_time.as_millis() < 100);
        assert!(access_time.as_millis() < 10);
    }
}