//! Optimized Taffy-based layout engine for Kryon
//! 
//! This module provides a high-performance layout system with minimal debug overhead
//! and optimized caching for production use.

use kryon_core::{Element, ElementId, LayoutDimension};
use glam::Vec2;
use std::collections::HashMap;
use taffy::prelude::*;
use tracing::debug;

/// Performance-optimized layout configuration
#[derive(Debug, Clone)]
pub struct LayoutConfig {
    /// Enable debug logging (disabled by default for performance)
    pub debug_logging: bool,
    /// Enable layout caching across frames
    pub enable_caching: bool,
    /// Maximum cache size to prevent memory growth
    pub max_cache_size: usize,
    /// Enable incremental layout updates
    pub incremental_updates: bool,
}

impl Default for LayoutConfig {
    fn default() -> Self {
        Self {
            debug_logging: false,  // Disable debug logging for now
            enable_caching: true,
            max_cache_size: 1024,
            incremental_updates: true,
        }
    }
}

/// Layout invalidation regions for partial updates
#[derive(Debug, Clone)]
pub struct InvalidationRegion {
    /// Elements that need layout recomputation
    pub elements: Vec<ElementId>,
    /// Whether this affects the entire tree
    pub full_tree: bool,
    /// Affected viewport region
    pub bounds: Option<taffy::Rect<f32>>,
}

/// Optimized Taffy layout engine with performance enhancements
pub struct OptimizedTaffyLayoutEngine {
    /// Taffy layout tree
    taffy: TaffyTree<ElementId>,
    /// Map from element ID to Taffy node (persistent across frames)
    element_to_node: HashMap<ElementId, taffy::NodeId>,
    /// Map from Taffy node back to element ID (persistent across frames)
    node_to_element: HashMap<taffy::NodeId, ElementId>,
    /// Cached final layout results
    layout_cache: HashMap<ElementId, Layout>,
    /// Last computed elements hash for change detection
    last_elements_hash: u64,
    /// Layout configuration
    config: LayoutConfig,
    /// Invalidation regions for partial updates
    invalidation_regions: Vec<InvalidationRegion>,
}

impl OptimizedTaffyLayoutEngine {
    /// Create a new optimized layout engine
    pub fn new() -> Self {
        Self::with_config(LayoutConfig::default())
    }

    /// Create a new optimized layout engine with custom configuration
    pub fn with_config(config: LayoutConfig) -> Self {
        Self {
            taffy: TaffyTree::new(),
            element_to_node: HashMap::new(),
            node_to_element: HashMap::new(),
            layout_cache: HashMap::new(),
            last_elements_hash: 0,
            config,
            invalidation_regions: Vec::new(),
        }
    }

    /// Enable or disable debug logging at runtime
    pub fn set_debug_logging(&mut self, enabled: bool) {
        self.config.debug_logging = enabled;
    }

    /// Add an invalidation region for partial updates
    pub fn add_invalidation_region(&mut self, region: InvalidationRegion) {
        self.invalidation_regions.push(region);
    }

    /// Clear all invalidation regions
    pub fn clear_invalidation_regions(&mut self) {
        self.invalidation_regions.clear();
    }

    /// Check if layout needs recomputation based on element changes
    pub fn needs_layout_update(&self, elements: &HashMap<ElementId, Element>) -> bool {
        if !self.config.enable_caching {
            return true;
        }

        // Fast hash-based change detection
        let current_hash = self.compute_elements_hash(elements);
        current_hash != self.last_elements_hash
    }

    /// Compute layout with optimized caching and partial updates
    pub fn compute_layout_optimized(
        &mut self,
        elements: &HashMap<ElementId, Element>,
        root_element_id: ElementId,
        available_space: Size<f32>,
    ) -> Result<(), taffy::TaffyError> {
        // Check if we can use cached results
        if self.config.enable_caching && !self.needs_layout_update(elements) {
            if self.config.debug_logging {
                debug!("Using cached layout results");
            }
            return Ok(());
        }

        // Update elements hash
        self.last_elements_hash = self.compute_elements_hash(elements);

        // Determine update strategy
        let use_incremental = self.config.incremental_updates && 
                             !self.invalidation_regions.is_empty() &&
                             !self.invalidation_regions.iter().any(|r| r.full_tree);

        if use_incremental {
            self.compute_incremental_layout(elements, root_element_id, available_space)?;
        } else {
            self.compute_full_layout(elements, root_element_id, available_space)?;
        }

        // Clear invalidation regions after processing
        self.clear_invalidation_regions();

        if self.config.debug_logging {
            debug!("Layout computation completed for {} elements", elements.len());
        }

        Ok(())
    }

    /// Compute full layout (fallback for major changes)
    fn compute_full_layout(
        &mut self,
        elements: &HashMap<ElementId, Element>,
        root_element_id: ElementId,
        available_space: Size<f32>,
    ) -> Result<(), taffy::TaffyError> {
        // Clear previous state but preserve node mappings if possible
        if self.config.enable_caching {
            self.layout_cache.clear();
        } else {
            self.clear_all_state();
        }

        // Build or update Taffy tree
        let root_node = self.build_taffy_tree_cached(elements, root_element_id)?;
        
        // Compute layout with Taffy
        let available_space = Size {
            width: AvailableSpace::Definite(available_space.width),
            height: AvailableSpace::Definite(available_space.height),
        };
        
        self.taffy.compute_layout(root_node, available_space)?;

        // Cache layout results
        self.cache_layouts(elements)?;

        Ok(())
    }

    /// Compute incremental layout (optimization for partial updates)
    fn compute_incremental_layout(
        &mut self,
        elements: &HashMap<ElementId, Element>,
        root_element_id: ElementId,
        available_space: Size<f32>,
    ) -> Result<(), taffy::TaffyError> {
        if self.config.debug_logging {
            debug!("Computing incremental layout for {} invalidation regions", self.invalidation_regions.len());
        }

        // Collect all elements that need updates
        let mut elements_to_update = std::collections::HashSet::new();
        
        for region in &self.invalidation_regions {
            if region.full_tree {
                // Full tree invalidation - fall back to full layout
                return self.compute_full_layout(elements, root_element_id, available_space);
            }
            
            // Add all elements in the region
            for &element_id in &region.elements {
                elements_to_update.insert(element_id);
                
                // Add children recursively if they exist
                if let Some(_element) = elements.get(&element_id) {
                    self.add_children_to_update_set(elements, element_id, &mut elements_to_update);
                }
            }
        }

        if elements_to_update.is_empty() {
            return Ok(());
        }

        // Update styles for affected elements
        for &element_id in &elements_to_update {
            if let Some(element) = elements.get(&element_id) {
                if let Some(&node_id) = self.element_to_node.get(&element_id) {
                    let new_style = self.krb_to_taffy_style(element);
                    
                    // Only update if style actually changed
                    if let Ok(current_style) = self.taffy.style(node_id) {
                        if !self.styles_equal(&current_style, &new_style) {
                            self.taffy.set_style(node_id, new_style)?;
                            
                            if self.config.debug_logging {
                                debug!("Updated style for element {}", element_id);
                            }
                        }
                    }
                }
            }
        }

        // Recompute layout for the root (Taffy will optimize internally)
        let root_node = self.element_to_node.get(&root_element_id).copied()
            .ok_or(taffy::TaffyError::InvalidParentNode(taffy::NodeId::from(0u64)))?;
        
        let available_space = Size {
            width: AvailableSpace::Definite(available_space.width),
            height: AvailableSpace::Definite(available_space.height),
        };
        
        self.taffy.compute_layout(root_node, available_space)?;

        // Update cache for affected elements only
        self.update_cache_for_elements(elements, &elements_to_update)?;

        if self.config.debug_logging {
            debug!("Incremental layout completed for {} elements", elements_to_update.len());
        }

        Ok(())
    }

    /// Add children recursively to update set
    fn add_children_to_update_set(
        &self,
        elements: &HashMap<ElementId, Element>,
        element_id: ElementId,
        update_set: &mut std::collections::HashSet<ElementId>,
    ) {
        if let Some(element) = elements.get(&element_id) {
            for &child_id in &element.children {
                if update_set.insert(child_id) {
                    // Only recurse if we haven't seen this child before
                    self.add_children_to_update_set(elements, child_id, update_set);
                }
            }
        }
    }

    /// Update cache for specific elements
    fn update_cache_for_elements(
        &mut self,
        _elements: &HashMap<ElementId, Element>,
        element_ids: &std::collections::HashSet<ElementId>,
    ) -> Result<(), taffy::TaffyError> {
        for &element_id in element_ids {
            if let Some(&node_id) = self.element_to_node.get(&element_id) {
                let layout = *self.taffy.layout(node_id)?;
                self.layout_cache.insert(element_id, layout);
            }
        }
        Ok(())
    }

    /// Check if two styles are equal (simplified comparison)
    fn styles_equal(&self, style1: &Style, style2: &Style) -> bool {
        // Compare key style properties that affect layout
        style1.display == style2.display &&
        style1.position == style2.position &&
        style1.flex_direction == style2.flex_direction &&
        style1.flex_grow == style2.flex_grow &&
        style1.flex_shrink == style2.flex_shrink &&
        style1.size == style2.size &&
        style1.min_size == style2.min_size &&
        style1.max_size == style2.max_size &&
        style1.margin == style2.margin &&
        style1.padding == style2.padding &&
        style1.justify_content == style2.justify_content &&
        style1.align_items == style2.align_items
    }

    /// Build Taffy tree with node reuse optimization
    fn build_taffy_tree_cached(
        &mut self,
        elements: &HashMap<ElementId, Element>,
        root_element_id: ElementId,
    ) -> Result<taffy::NodeId, taffy::TaffyError> {
        // Check if we can reuse existing nodes
        if self.config.enable_caching && !self.element_to_node.is_empty() {
            return self.update_existing_tree(elements, root_element_id);
        }

        // Build new tree
        self.build_taffy_tree_deterministic(elements, root_element_id)
    }

    /// Update existing Taffy tree (optimization)
    fn update_existing_tree(
        &mut self,
        elements: &HashMap<ElementId, Element>,
        root_element_id: ElementId,
    ) -> Result<taffy::NodeId, taffy::TaffyError> {
        // Update styles for existing nodes
        for (&element_id, element) in elements {
            if let Some(&node_id) = self.element_to_node.get(&element_id) {
                let style = self.krb_to_taffy_style(element);
                self.taffy.set_style(node_id, style)?;
            }
        }

        // Return root node
        self.element_to_node.get(&root_element_id).copied()
            .ok_or(taffy::TaffyError::InvalidParentNode(taffy::NodeId::from(0u64)))
    }

    /// Build Taffy tree in deterministic order (same as original but optimized)
    fn build_taffy_tree_deterministic(
        &mut self,
        elements: &HashMap<ElementId, Element>,
        root_element_id: ElementId,
    ) -> Result<taffy::NodeId, taffy::TaffyError> {
        // Create all nodes in sorted order for deterministic behavior
        let mut sorted_elements: Vec<_> = elements.iter().collect();
        sorted_elements.sort_by_key(|(id, _)| *id);
        
        for (&element_id, element) in sorted_elements {
            let style = self.krb_to_taffy_style(element);
            let node = self.taffy.new_leaf(style)?;
            
            self.element_to_node.insert(element_id, node);
            self.node_to_element.insert(node, element_id);

            if self.config.debug_logging {
                debug!("Created Taffy node for element {}", element_id);
            }
        }

        // Build parent-child relationships
        for (&element_id, element) in elements {
            if let Some(&parent_node) = self.element_to_node.get(&element_id) {
                let child_nodes: Vec<_> = element.children.iter()
                    .filter_map(|&child_id| self.element_to_node.get(&child_id).copied())
                    .collect();
                
                if !child_nodes.is_empty() {
                    self.taffy.set_children(parent_node, &child_nodes)?;
                    
                    if self.config.debug_logging {
                        debug!("Set {} children for element {}", child_nodes.len(), element_id);
                    }
                }
            }
        }

        self.element_to_node.get(&root_element_id).copied()
            .ok_or(taffy::TaffyError::InvalidParentNode(taffy::NodeId::from(0u64)))
    }

    /// Convert element to Taffy style (optimized version without debug prints)
    fn krb_to_taffy_style(&self, element: &Element) -> Style {
        let mut style = Style::DEFAULT;

        // Basic sizing from LayoutDimension
        match &element.layout_size.width {
            LayoutDimension::Pixels(px) if *px > 0.0 => {
                style.size.width = Dimension::Length(*px);
            }
            LayoutDimension::Percentage(pct) => {
                style.size.width = Dimension::Percent(*pct);
            }
            _ => {}
        }
        match &element.layout_size.height {
            LayoutDimension::Pixels(px) if *px > 0.0 => {
                style.size.height = Dimension::Length(*px);
            }
            LayoutDimension::Percentage(pct) => {
                style.size.height = Dimension::Percent(*pct);
            }
            _ => {}
        }

        // Apply layout flags efficiently
        self.apply_layout_flags(&mut style, element);

        // Element-specific optimizations (before custom properties so they can be overridden)
        self.apply_element_type_optimizations(&mut style, element);

        // Apply custom properties (these should override defaults)
        self.apply_custom_properties(&mut style, element);

        style
    }

    /// Apply layout flags without debug output
    fn apply_layout_flags(&self, style: &mut Style, element: &Element) {
        let flags = element.layout_flags;
        
        // Layout direction
        if flags & 0x01 != 0 {
            style.flex_direction = FlexDirection::Column;
        }

        // Absolute positioning
        if flags & 0x02 != 0 {
            style.position = Position::Absolute;
            match &element.layout_position.x {
                LayoutDimension::Pixels(px) if *px != 0.0 => {
                    style.inset.left = LengthPercentageAuto::Length(*px);
                }
                LayoutDimension::Percentage(pct) => {
                    style.inset.left = LengthPercentageAuto::Percent(*pct);
                }
                _ => {}
            }
            match &element.layout_position.y {
                LayoutDimension::Pixels(px) if *px != 0.0 => {
                    style.inset.top = LengthPercentageAuto::Length(*px);
                }
                LayoutDimension::Percentage(pct) => {
                    style.inset.top = LengthPercentageAuto::Percent(*pct);
                }
                _ => {}
            }
        }

        // Centering
        if flags & 0x04 != 0 {
            style.justify_content = Some(JustifyContent::Center);
            style.align_items = Some(AlignItems::Center);
        }

        // Flex grow
        if flags & 0x20 != 0 {
            style.flex_grow = 1.0;
        }
    }

    /// Apply custom properties efficiently
    fn apply_custom_properties(&self, style: &mut Style, element: &Element) {
        use kryon_core::PropertyValue;

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

        // Position property
        if let Some(PropertyValue::String(value)) = element.custom_properties.get("position") {
            style.position = match value.as_str() {
                "absolute" => Position::Absolute,
                "relative" => Position::Relative,
                _ => style.position,
            };
            
            // For absolute positioning, set inset values from layout_position
            if style.position == Position::Absolute {
                match &element.layout_position.x {
                    LayoutDimension::Pixels(px) => {
                        style.inset.left = LengthPercentageAuto::Length(*px);
                    }
                    LayoutDimension::Percentage(pct) => {
                        style.inset.left = LengthPercentageAuto::Percent(*pct);
                    }
                    _ => {}
                }
                match &element.layout_position.y {
                    LayoutDimension::Pixels(px) => {
                        style.inset.top = LengthPercentageAuto::Length(*px);
                    }
                    LayoutDimension::Percentage(pct) => {
                        style.inset.top = LengthPercentageAuto::Percent(*pct);
                    }
                    _ => {}
                }
            }
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

        // Gap property for flexbox spacing
        if let Some(PropertyValue::Float(value)) = element.custom_properties.get("gap") {
            let gap = LengthPercentage::Length(*value);
            style.gap = Size {
                width: gap,
                height: gap,
            };
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

        // Individual padding properties
        if let Some(PropertyValue::Float(value)) = element.custom_properties.get("padding_top") {
            style.padding.top = LengthPercentage::Length(*value);
        }
        if let Some(PropertyValue::Float(value)) = element.custom_properties.get("padding_right") {
            style.padding.right = LengthPercentage::Length(*value);
        }
        if let Some(PropertyValue::Float(value)) = element.custom_properties.get("padding_bottom") {
            style.padding.bottom = LengthPercentage::Length(*value);
        }
        if let Some(PropertyValue::Float(value)) = element.custom_properties.get("padding_left") {
            style.padding.left = LengthPercentage::Length(*value);
        }
    }

    /// Apply element type specific optimizations
    fn apply_element_type_optimizations(&self, style: &mut Style, element: &Element) {
        match element.element_type {
            kryon_core::ElementType::Text | kryon_core::ElementType::Link => {
                // Text elements should participate in their parent's layout context
                // Don't force display: block - let them inherit flex behavior from parent
                
                // Always provide intrinsic sizing for text elements to prevent zero size
                let text_height = element.font_size.max(16.0);
                let text_length = element.text.len() as f32;
                let char_width = element.font_size * 0.6; // Rough estimate
                let text_width = (text_length * char_width).max(50.0); // Minimum 50px
                
                // Apply width if not explicitly set or if zero
                match element.layout_size.width {
                    LayoutDimension::Auto => {
                        style.size.width = Dimension::Length(text_width);
                    }
                    LayoutDimension::Pixels(px) if px <= 0.0 => {
                        style.size.width = Dimension::Length(text_width);
                    }
                    LayoutDimension::Pixels(px) => {
                        style.size.width = Dimension::Length(px);
                    }
                    LayoutDimension::Percentage(pct) => {
                        style.size.width = Dimension::Percent(pct);
                    }
                    LayoutDimension::MinPixels(px) | LayoutDimension::MaxPixels(px) => {
                        style.size.width = Dimension::Length(px);
                    }
                }
                
                // Apply height if not explicitly set or if zero
                match element.layout_size.height {
                    LayoutDimension::Auto => {
                        style.size.height = Dimension::Length(text_height);
                    }
                    LayoutDimension::Pixels(px) if px <= 0.0 => {
                        style.size.height = Dimension::Length(text_height);
                    }
                    LayoutDimension::Pixels(px) => {
                        style.size.height = Dimension::Length(px);
                    }
                    LayoutDimension::Percentage(pct) => {
                        style.size.height = Dimension::Percent(pct);
                    }
                    LayoutDimension::MinPixels(px) | LayoutDimension::MaxPixels(px) => {
                        style.size.height = Dimension::Length(px);
                    }
                }
            }
            kryon_core::ElementType::Button => {
                // Only set block display if no explicit display property is set
                if !element.custom_properties.contains_key("display") {
                    style.display = Display::Block;
                }
                style.flex_grow = 0.0;
                style.flex_shrink = 0.0;
                
                // Preserve button dimensions
                if let LayoutDimension::Pixels(px) = element.layout_size.width {
                    if px > 0.0 {
                        style.min_size.width = Dimension::Length(px);
                        style.max_size.width = Dimension::Length(px);
                    }
                }
                if let LayoutDimension::Pixels(px) = element.layout_size.height {
                    if px > 0.0 {
                        style.min_size.height = Dimension::Length(px);
                        style.max_size.height = Dimension::Length(px);
                    }
                }
            }
            kryon_core::ElementType::App => {
                // App elements are flex containers but don't override alignment
                if !element.custom_properties.contains_key("display") {
                    style.display = Display::Flex;
                }
                if !element.custom_properties.contains_key("flex_direction") {
                    style.flex_direction = FlexDirection::Column;
                }
                // Don't set default alignment - let child elements control their own layout
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
            _ => {}
        }
    }

    /// Cache layout results efficiently
    fn cache_layouts(&mut self, elements: &HashMap<ElementId, Element>) -> Result<(), taffy::TaffyError> {
        self.layout_cache.clear();
        
        // Reserve capacity for better performance
        self.layout_cache.reserve(elements.len());
        
        for (&element_id, _) in elements {
            if let Some(&node_id) = self.element_to_node.get(&element_id) {
                let layout = *self.taffy.layout(node_id)?;
                self.layout_cache.insert(element_id, layout);
            }
        }

        // Enforce cache size limits
        if self.layout_cache.len() > self.config.max_cache_size {
            self.layout_cache.clear();
            if self.config.debug_logging {
                debug!("Layout cache cleared due to size limit");
            }
        }

        Ok(())
    }

    /// Compute simple hash of elements for change detection
    fn compute_elements_hash(&self, elements: &HashMap<ElementId, Element>) -> u64 {
        use std::collections::hash_map::DefaultHasher;
        use std::hash::{Hash, Hasher};

        let mut hasher = DefaultHasher::new();
        
        // Hash key properties that affect layout
        for (&id, element) in elements {
            id.hash(&mut hasher);
            element.layout_size.width.to_pixels(1.0).to_bits().hash(&mut hasher);
            element.layout_size.height.to_pixels(1.0).to_bits().hash(&mut hasher);
            element.layout_position.x.to_pixels(1.0).to_bits().hash(&mut hasher);
            element.layout_position.y.to_pixels(1.0).to_bits().hash(&mut hasher);
            element.layout_flags.hash(&mut hasher);
            std::mem::discriminant(&element.element_type).hash(&mut hasher);
            element.children.len().hash(&mut hasher);
        }

        hasher.finish()
    }

    /// Clear all state (for major changes)
    fn clear_all_state(&mut self) {
        self.taffy = TaffyTree::new();
        self.element_to_node.clear();
        self.node_to_element.clear();
        self.layout_cache.clear();
    }

    /// Get cached layout result
    pub fn get_layout(&self, element_id: ElementId) -> Option<&Layout> {
        self.layout_cache.get(&element_id)
    }

    /// Get layout cache statistics
    pub fn get_cache_stats(&self) -> (usize, usize) {
        (self.layout_cache.len(), self.config.max_cache_size)
    }
}

// Implement required trait for compatibility
impl crate::LayoutEngine for OptimizedTaffyLayoutEngine {
    fn compute_layout(
        &mut self,
        elements: &HashMap<ElementId, Element>,
        root_id: ElementId,
        viewport_size: Vec2,
    ) -> crate::LayoutResult {
        let size = Size {
            width: viewport_size.x,
            height: viewport_size.y,
        };
        
        if let Err(e) = self.compute_layout_optimized(elements, root_id, size) {
            tracing::error!("Optimized layout computation failed: {:?}", e);
            return crate::LayoutResult {
                computed_positions: HashMap::new(),
                computed_sizes: HashMap::new(),
            };
        }

        // Convert to LayoutResult
        let mut computed_positions = HashMap::new();
        let mut computed_sizes = HashMap::new();

        self.compute_absolute_positions(elements, root_id, Vec2::ZERO, &mut computed_positions, &mut computed_sizes);

        crate::LayoutResult {
            computed_positions,
            computed_sizes,
        }
    }
}

impl OptimizedTaffyLayoutEngine {
    /// Compute absolute positions recursively (optimized version)
    fn compute_absolute_positions(
        &self,
        elements: &HashMap<ElementId, Element>,
        element_id: ElementId,
        parent_offset: Vec2,
        computed_positions: &mut HashMap<ElementId, Vec2>,
        computed_sizes: &mut HashMap<ElementId, Vec2>,
    ) {
        if let Some(layout) = self.get_layout(element_id) {
            let taffy_position = Vec2::new(layout.location.x, layout.location.y);
            let taffy_size = Vec2::new(layout.size.width, layout.size.height);
            
            if self.config.debug_logging {
                tracing::debug!("Element {}: Taffy position={:?}, size={:?}", element_id, taffy_position, taffy_size);
            }
            
            // Determine final position based on element properties
            let final_position = if let Some(element) = elements.get(&element_id) {
                // Check if element has absolute positioning
                let has_absolute_position = element.custom_properties.get("position")
                    .map(|p| matches!(p, kryon_core::PropertyValue::String(s) if s == "absolute"))
                    .unwrap_or(false) || (element.layout_flags & 0x02 != 0);
                
                if has_absolute_position {
                    // Use element's actual position from KRB file for absolute positioning
                    match (&element.layout_position.x, &element.layout_position.y) {
                        (LayoutDimension::Pixels(x), LayoutDimension::Pixels(y)) => {
                            Vec2::new(*x, *y)
                        }
                        _ => {
                            // Fallback: use Taffy computed position if layout_position not set
                            parent_offset + taffy_position
                        }
                    }
                } else {
                    // Use Taffy computed position for flex layout
                    parent_offset + taffy_position
                }
            } else {
                // Fallback to Taffy position if element not found
                parent_offset + taffy_position
            };
            
            if self.config.debug_logging {
                tracing::debug!("Element {}: Final position={:?}, parent_offset={:?}", element_id, final_position, parent_offset);
            }
            
            computed_positions.insert(element_id, final_position);
            computed_sizes.insert(element_id, taffy_size);

            // Process children
            if let Some(element) = elements.get(&element_id) {
                for &child_id in &element.children {
                    self.compute_absolute_positions(
                        elements,
                        child_id,
                        final_position,  // Use the computed final position as parent offset
                        computed_positions,
                        computed_sizes,
                    );
                }
            }
        } else {
            if self.config.debug_logging {
                tracing::debug!("Element {}: No layout found in cache!", element_id);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kryon_core::ElementType;

    #[test]
    fn test_optimized_layout_engine() {
        let mut engine = OptimizedTaffyLayoutEngine::new();
        
        // Create test elements
        let mut elements = HashMap::new();
        let mut root_element = Element::new(ElementType::App);
        root_element.layout_size = LayoutSize::pixels(800.0, 600.0);
        elements.insert(0, root_element);

        // Test layout computation
        let result = engine.compute_layout(&elements, 0, Vec2::new(800.0, 600.0));
        
        assert_eq!(result.computed_positions.len(), 1);
        assert_eq!(result.computed_sizes.len(), 1);
    }

    #[test]
    fn test_caching() {
        let mut engine = OptimizedTaffyLayoutEngine::with_config(LayoutConfig {
            enable_caching: true,
            ..Default::default()
        });

        let mut elements = HashMap::new();
        let mut root_element = Element::new(ElementType::App);
        root_element.layout_size = LayoutSize::pixels(800.0, 600.0);
        elements.insert(0, root_element);

        // First computation
        let _result1 = engine.compute_layout(&elements, 0, Vec2::new(800.0, 600.0));
        
        // Second computation with same elements should use cache
        let needs_update = engine.needs_layout_update(&elements);
        assert!(!needs_update);
    }
}