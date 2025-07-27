//! Layout computation utilities for Taffy layout engine
//!
//! This module handles the computation of final layout positions and sizes.

use kryon_core::{Element, ElementId};
use crate::LayoutResult;
use std::collections::HashMap;
use taffy::prelude::*;
use glam::Vec2;

/// Computes final layout positions and sizes from Taffy results
pub struct LayoutComputer;

impl LayoutComputer {
    /// Compute absolute positions recursively from Taffy layout results
    pub fn compute_absolute_positions(
        taffy: &TaffyTree<ElementId>,
        elements: &HashMap<ElementId, Element>,
        element_to_node: &HashMap<ElementId, taffy::NodeId>,
        element_id: ElementId,
        parent_offset: Vec2,
        computed_positions: &mut HashMap<ElementId, Vec2>,
        computed_sizes: &mut HashMap<ElementId, Vec2>,
    ) -> Result<(), taffy::TaffyError> {
        eprintln!("🎯 [LAYOUT_COMPUTER] Processing element {} at parent_offset ({:.1}, {:.1})", element_id, parent_offset.x, parent_offset.y);
        if let Some(&node) = element_to_node.get(&element_id) {
            let layout = *taffy.layout(node)?;
            
            let taffy_position = Vec2::new(layout.location.x, layout.location.y);
            let taffy_size = Vec2::new(layout.size.width, layout.size.height);
            
            // Calculate absolute position with flexbox padding fix
            let mut absolute_position = parent_offset + taffy_position;
            
            // ✨ Fix for flexbox justify-content:end padding inconsistency
            // This ensures flex-end containers have symmetric visual padding like flex-start containers
            if let Some(current_element) = elements.get(&element_id) {
                eprintln!("🎯 [DEBUG] Checking element {} for flex-end fix", element_id);
                
                // Check if this element is a flex container with justify-content: end
                if let (Some(justify_content), Some(display)) = (
                    current_element.custom_properties.get("justify_content"),
                    current_element.custom_properties.get("display")
                ) {
                    eprintln!("🎯 [DEBUG] Element {} has display: {:?}, justify: {:?}", 
                             element_id, display.as_string(), justify_content.as_string());
                    
                    if display.as_string().map_or(false, |s| s == "flex") {
                        if let Some(justify_val) = justify_content.as_string() {
                            eprintln!("🎯 [DEBUG] Element {} justify_content = '{}'", element_id, justify_val);
                            
                            if justify_val == "end" || justify_val == "flex-end" {
                                eprintln!("🎯 [DEBUG] Element {} has flex-end, looking for parent...", element_id);
                                
                                // Find the parent container to get its padding
                                for (_parent_id, parent_element) in elements {
                                    if parent_element.children.contains(&element_id) {
                                        let parent_padding = parent_element.custom_properties.get("padding")
                                            .and_then(|p| p.as_float()).unwrap_or(0.0);
                                        
                                        eprintln!("🎯 [DEBUG] Found parent with padding: {:.1}px", parent_padding);
                                        
                                        if parent_padding > 0.0 {
                                            // For flex-end containers, shift left by the padding amount to achieve symmetry
                                            let symmetry_shift = parent_padding;
                                            absolute_position.x -= symmetry_shift;
                                            
                                            eprintln!("🎯 [FLEX-END-SYMMETRY] Element {} (justify:end) adjusted by -{:.1}px for visual balance", 
                                                     element_id, symmetry_shift);
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            
            computed_positions.insert(element_id, absolute_position);
            computed_sizes.insert(element_id, taffy_size);

            // Process children
            if let Some(element) = elements.get(&element_id) {
                for &child_id in &element.children {
                    Self::compute_absolute_positions(
                        taffy,
                        elements,
                        element_to_node,
                        child_id,
                        absolute_position,
                        computed_positions,
                        computed_sizes,
                    )?;
                }
            }
        }
        
        Ok(())
    }

    /// Convert Taffy layout results to Kryon LayoutResult
    pub fn create_layout_result(
        taffy: &TaffyTree<ElementId>,
        elements: &HashMap<ElementId, Element>,
        element_to_node: &HashMap<ElementId, taffy::NodeId>,
        root_element_id: ElementId,
    ) -> LayoutResult {
        let mut computed_positions = HashMap::new();
        let mut computed_sizes = HashMap::new();

        if let Err(e) = Self::compute_absolute_positions(
            taffy,
            elements,
            element_to_node,
            root_element_id,
            Vec2::ZERO,
            &mut computed_positions,
            &mut computed_sizes,
        ) {
            tracing::error!("Failed to compute absolute positions: {:?}", e);
        }

        LayoutResult {
            computed_positions,
            computed_sizes,
        }
    }

    /// Get layout for a specific element
    pub fn get_element_layout(
        taffy: &TaffyTree<ElementId>,
        element_to_node: &HashMap<ElementId, taffy::NodeId>,
        element_id: ElementId,
    ) -> Option<Layout> {
        element_to_node.get(&element_id)
            .and_then(|&node| taffy.layout(node).ok())
            .copied()
    }

    /// Check if an element has valid layout
    pub fn has_valid_layout(
        taffy: &TaffyTree<ElementId>,
        element_to_node: &HashMap<ElementId, taffy::NodeId>,
        element_id: ElementId,
    ) -> bool {
        Self::get_element_layout(taffy, element_to_node, element_id).is_some()
    }

    /// Get all computed sizes for elements
    pub fn get_computed_sizes(
        taffy: &TaffyTree<ElementId>,
        element_to_node: &HashMap<ElementId, taffy::NodeId>,
        _elements: &HashMap<ElementId, Element>,
    ) -> HashMap<ElementId, Vec2> {
        let mut computed_sizes = HashMap::new();
        
        for (&element_id, &node) in element_to_node {
            if let Ok(layout) = taffy.layout(node) {
                let size = Vec2::new(layout.size.width, layout.size.height);
                computed_sizes.insert(element_id, size);
            }
        }
        
        computed_sizes
    }

    /// Get all computed positions for elements
    pub fn get_computed_positions(
        taffy: &TaffyTree<ElementId>,
        element_to_node: &HashMap<ElementId, taffy::NodeId>,
        elements: &HashMap<ElementId, Element>,
        root_element_id: ElementId,
    ) -> HashMap<ElementId, Vec2> {
        let mut computed_positions = HashMap::new();
        let mut computed_sizes = HashMap::new();

        if let Err(e) = Self::compute_absolute_positions(
            taffy,
            elements,
            element_to_node,
            root_element_id,
            Vec2::ZERO,
            &mut computed_positions,
            &mut computed_sizes,
        ) {
            tracing::error!("Failed to compute positions: {:?}", e);
        }

        computed_positions
    }

    /// Check if layout computation is needed
    pub fn needs_layout_computation(
        elements: &HashMap<ElementId, Element>,
        last_computed_elements: &HashMap<ElementId, Element>,
    ) -> bool {
        if elements.len() != last_computed_elements.len() {
            return true;
        }

        for (id, element) in elements {
            if let Some(last_element) = last_computed_elements.get(id) {
                if element.layout_size != last_element.layout_size ||
                   element.layout_position != last_element.layout_position ||
                   element.layout_flags != last_element.layout_flags {
                    return true;
                }
            } else {
                return true;
            }
        }

        false
    }

    /// Compute layout bounds for a set of elements
    pub fn compute_layout_bounds(
        computed_positions: &HashMap<ElementId, Vec2>,
        computed_sizes: &HashMap<ElementId, Vec2>,
        element_ids: &[ElementId],
    ) -> Option<(Vec2, Vec2)> {
        if element_ids.is_empty() {
            return None;
        }

        let mut min_pos = Vec2::new(f32::INFINITY, f32::INFINITY);
        let mut max_pos = Vec2::new(f32::NEG_INFINITY, f32::NEG_INFINITY);

        for &element_id in element_ids {
            if let (Some(&pos), Some(&size)) = (
                computed_positions.get(&element_id),
                computed_sizes.get(&element_id),
            ) {
                min_pos = min_pos.min(pos);
                max_pos = max_pos.max(pos + size);
            }
        }

        if min_pos.x != f32::INFINITY && min_pos.y != f32::INFINITY {
            Some((min_pos, max_pos - min_pos))
        } else {
            None
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kryon_core::ElementType;

    #[test]
    fn test_layout_computation() {
        let mut taffy = TaffyTree::new();
        let mut elements = HashMap::new();
        let mut element_to_node = HashMap::new();

        // Create a simple element
        let element = Element::new(ElementType::Container);
        elements.insert(0, element);

        // Create a Taffy node
        let node = taffy.new_leaf(Style::DEFAULT).unwrap();
        element_to_node.insert(0, node);

        // Compute layout
        let available_space = Size {
            width: AvailableSpace::Definite(100.0),
            height: AvailableSpace::Definite(100.0),
        };
        taffy.compute_layout(node, available_space).unwrap();

        // Test layout retrieval
        let layout = LayoutComputer::get_element_layout(&taffy, &element_to_node, 0);
        assert!(layout.is_some());

        // Test validity check
        assert!(LayoutComputer::has_valid_layout(&taffy, &element_to_node, 0));
        assert!(!LayoutComputer::has_valid_layout(&taffy, &element_to_node, 999));
    }

    #[test]
    fn test_needs_layout_computation() {
        let mut elements1 = HashMap::new();
        let mut elements2 = HashMap::new();

        let element1 = Element::new(ElementType::Container);
        let mut element2 = Element::new(ElementType::Container);
        element2.size = Vec2::new(100.0, 100.0);

        elements1.insert(0, element1);
        elements2.insert(0, element2);

        // Should need recomputation when elements are different
        assert!(LayoutComputer::needs_layout_computation(&elements2, &elements1));
        
        // Should not need recomputation when elements are the same
        assert!(!LayoutComputer::needs_layout_computation(&elements1, &elements1));
    }

    #[test]
    fn test_layout_bounds() {
        let mut positions = HashMap::new();
        let mut sizes = HashMap::new();

        positions.insert(0, Vec2::new(10.0, 20.0));
        sizes.insert(0, Vec2::new(100.0, 50.0));

        positions.insert(1, Vec2::new(50.0, 30.0));
        sizes.insert(1, Vec2::new(200.0, 100.0));

        let bounds = LayoutComputer::compute_layout_bounds(&positions, &sizes, &[0, 1]);
        assert!(bounds.is_some());

        let (min_pos, size) = bounds.unwrap();
        assert_eq!(min_pos, Vec2::new(10.0, 20.0));
        assert_eq!(size, Vec2::new(240.0, 110.0)); // (250-10, 130-20)
    }
}