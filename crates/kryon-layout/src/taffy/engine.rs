//! Main Taffy layout engine implementation
//!
//! This module provides the main TaffyLayoutEngine that coordinates
//! all the layout computation components.

use kryon_core::{Element, ElementId};
use crate::{LayoutEngine, LayoutResult};
use std::collections::HashMap;
use taffy::prelude::*;
use glam::Vec2;

use super::{TreeBuilder, LayoutComputer};

/// Taffy-based layout engine that replaces the legacy flex layout system
pub struct TaffyLayoutEngine {
    /// Taffy layout tree
    taffy: TaffyTree<ElementId>,
    /// Tree builder for creating Taffy trees
    tree_builder: TreeBuilder,
    /// Cached final layout results
    layout_cache: HashMap<ElementId, Layout>,
    /// Last computed elements for change detection
    last_computed_elements: HashMap<ElementId, Element>,
}

impl TaffyLayoutEngine {
    /// Create a new Taffy layout engine
    pub fn new() -> Self {
        Self {
            taffy: TaffyTree::new(),
            tree_builder: TreeBuilder::new(),
            layout_cache: HashMap::new(),
            last_computed_elements: HashMap::new(),
        }
    }

    /// Convert KRB elements to Taffy layout tree and compute layout
    pub fn compute_taffy_layout(
        &mut self,
        elements: &HashMap<ElementId, Element>,
        root_element_id: ElementId,
        available_space: Size<f32>,
    ) -> Result<(), taffy::TaffyError> {
        // Clear previous state
        self.clear();

        // Build Taffy tree from KRB elements in deterministic order
        let root_node = self.tree_builder.build_taffy_tree_deterministic(
            &mut self.taffy,
            elements,
            root_element_id,
        )?;
        
        // Compute layout with Taffy
        let available_space = Size {
            width: AvailableSpace::Definite(available_space.width),
            height: AvailableSpace::Definite(available_space.height),
        };
        
        self.taffy.compute_layout(root_node, available_space)?;

        // Cache layout results
        self.cache_layouts(elements)?;

        // Update last computed elements
        self.last_computed_elements = elements.clone();

        Ok(())
    }

    /// Cache layout results for quick access
    fn cache_layouts(&mut self, elements: &HashMap<ElementId, Element>) -> Result<(), taffy::TaffyError> {
        self.layout_cache.clear();
        
        for (&element_id, _) in elements {
            if let Some(node) = self.tree_builder.get_node(element_id) {
                let layout = *self.taffy.layout(node)?;
                self.layout_cache.insert(element_id, layout);
            }
        }

        Ok(())
    }

    /// Get cached layout result for an element
    pub fn get_layout(&self, element_id: ElementId) -> Option<&Layout> {
        self.layout_cache.get(&element_id)
    }

    /// Check if layout computation is needed
    pub fn needs_layout_computation(&self, elements: &HashMap<ElementId, Element>) -> bool {
        LayoutComputer::needs_layout_computation(elements, &self.last_computed_elements)
    }

    /// Clear all cached data
    pub fn clear(&mut self) {
        self.taffy = TaffyTree::new();
        self.tree_builder.clear();
        self.layout_cache.clear();
    }

    /// Get the number of cached layouts
    pub fn layout_cache_size(&self) -> usize {
        self.layout_cache.len()
    }

    /// Get cache statistics
    pub fn get_cache_stats(&self) -> (usize, usize, usize) {
        (
            self.layout_cache.len(),
            self.tree_builder.node_count(),
            self.last_computed_elements.len(),
        )
    }

    /// Force layout recomputation on next call
    pub fn invalidate_layout(&mut self) {
        self.last_computed_elements.clear();
        self.layout_cache.clear();
    }

    /// Get layout for a specific element
    pub fn get_element_layout(&self, element_id: ElementId) -> Option<Layout> {
        LayoutComputer::get_element_layout(
            &self.taffy,
            self.tree_builder.get_element_to_node_map(),
            element_id,
        )
    }

    /// Check if an element has valid layout
    pub fn has_valid_layout(&self, element_id: ElementId) -> bool {
        LayoutComputer::has_valid_layout(
            &self.taffy,
            self.tree_builder.get_element_to_node_map(),
            element_id,
        )
    }
}

impl Default for TaffyLayoutEngine {
    fn default() -> Self {
        Self::new()
    }
}

impl LayoutEngine for TaffyLayoutEngine {
    fn compute_layout(
        &mut self,
        elements: &HashMap<ElementId, Element>,
        root_id: ElementId,
        viewport_size: Vec2,
    ) -> LayoutResult {
        let size = Size {
            width: viewport_size.x,
            height: viewport_size.y,
        };
        
        if let Err(e) = self.compute_taffy_layout(elements, root_id, size) {
            tracing::error!("Taffy layout computation failed: {:?}", e);
            return LayoutResult {
                computed_positions: HashMap::new(),
                computed_sizes: HashMap::new(),
            };
        }

        LayoutComputer::create_layout_result(
            &self.taffy,
            elements,
            self.tree_builder.get_element_to_node_map(),
            root_id,
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kryon_core::ElementType;

    #[test]
    fn test_taffy_layout_engine() {
        let mut engine = TaffyLayoutEngine::new();
        
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
    fn test_layout_caching() {
        let mut engine = TaffyLayoutEngine::new();
        
        let mut elements = HashMap::new();
        let mut root_element = Element::new(ElementType::App);
        root_element.layout_size = LayoutSize::pixels(800.0, 600.0);
        elements.insert(0, root_element);

        // First computation
        let _result1 = engine.compute_layout(&elements, 0, Vec2::new(800.0, 600.0));
        assert_eq!(engine.layout_cache_size(), 1);
        
        // Second computation with same elements should use cache
        let needs_computation = engine.needs_layout_computation(&elements);
        assert!(!needs_computation);
    }

    #[test]
    fn test_layout_invalidation() {
        let mut engine = TaffyLayoutEngine::new();
        
        let mut elements = HashMap::new();
        let root_element = Element::new(ElementType::App);
        elements.insert(0, root_element);

        engine.compute_layout(&elements, 0, Vec2::new(800.0, 600.0));
        assert_eq!(engine.layout_cache_size(), 1);
        
        engine.invalidate_layout();
        assert_eq!(engine.layout_cache_size(), 0);
    }

    #[test]
    fn test_layout_statistics() {
        let mut engine = TaffyLayoutEngine::new();
        
        let mut elements = HashMap::new();
        let root_element = Element::new(ElementType::App);
        elements.insert(0, root_element);

        engine.compute_layout(&elements, 0, Vec2::new(800.0, 600.0));
        
        let (cache_size, node_count, element_count) = engine.get_cache_stats();
        assert_eq!(cache_size, 1);
        assert_eq!(node_count, 1);
        assert_eq!(element_count, 1);
    }
}