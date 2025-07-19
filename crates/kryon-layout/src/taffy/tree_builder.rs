//! Tree building utilities for Taffy layout engine
//!
//! This module handles the construction of Taffy trees from KRB elements.

use kryon_core::{Element, ElementId};
use std::collections::HashMap;
use taffy::prelude::*;
use tracing::debug;

use super::style_converter::StyleConverter;

/// Builds Taffy layout trees from KRB elements
pub struct TreeBuilder {
    /// Map from element ID to Taffy node
    element_to_node: HashMap<ElementId, taffy::NodeId>,
    /// Map from Taffy node back to element ID
    node_to_element: HashMap<taffy::NodeId, ElementId>,
}

impl TreeBuilder {
    /// Create a new tree builder
    pub fn new() -> Self {
        Self {
            element_to_node: HashMap::new(),
            node_to_element: HashMap::new(),
        }
    }

    /// Build Taffy tree from KRB elements in deterministic order
    pub fn build_taffy_tree_deterministic(
        &mut self,
        taffy: &mut TaffyTree<ElementId>,
        elements: &HashMap<ElementId, Element>,
        root_element_id: ElementId,
    ) -> Result<taffy::NodeId, taffy::TaffyError> {
        // Clear previous state
        self.element_to_node.clear();
        self.node_to_element.clear();

        // Create all nodes in sorted order for deterministic behavior
        let mut sorted_elements: Vec<_> = elements.iter().collect();
        sorted_elements.sort_by_key(|(id, _)| *id);
        
        for (&element_id, element) in sorted_elements {
            let style = StyleConverter::krb_to_taffy_style(element);
            let node = taffy.new_leaf(style)?;
            
            self.element_to_node.insert(element_id, node);
            self.node_to_element.insert(node, element_id);

            debug!("Created Taffy node for element {}", element_id);
        }

        // Build parent-child relationships
        for (&element_id, element) in elements {
            if let Some(&parent_node) = self.element_to_node.get(&element_id) {
                let child_nodes: Vec<_> = element.children.iter()
                    .filter_map(|&child_id| self.element_to_node.get(&child_id).copied())
                    .collect();
                
                if !child_nodes.is_empty() {
                    taffy.set_children(parent_node, &child_nodes)?;
                    debug!("Set {} children for element {}", child_nodes.len(), element_id);
                }
            }
        }

        self.element_to_node.get(&root_element_id).copied()
            .ok_or(taffy::TaffyError::InvalidParentNode(taffy::NodeId::from(0u64)))
    }

    /// Get the Taffy node for a given element ID
    pub fn get_node(&self, element_id: ElementId) -> Option<taffy::NodeId> {
        self.element_to_node.get(&element_id).copied()
    }

    /// Get the element ID for a given Taffy node
    pub fn get_element_id(&self, node: taffy::NodeId) -> Option<ElementId> {
        self.node_to_element.get(&node).copied()
    }

    /// Get all element-to-node mappings
    pub fn get_element_to_node_map(&self) -> &HashMap<ElementId, taffy::NodeId> {
        &self.element_to_node
    }

    /// Get all node-to-element mappings
    pub fn get_node_to_element_map(&self) -> &HashMap<taffy::NodeId, ElementId> {
        &self.node_to_element
    }

    /// Clear all mappings
    pub fn clear(&mut self) {
        self.element_to_node.clear();
        self.node_to_element.clear();
    }

    /// Get the number of nodes in the tree
    pub fn node_count(&self) -> usize {
        self.element_to_node.len()
    }

    /// Check if the tree is empty
    pub fn is_empty(&self) -> bool {
        self.element_to_node.is_empty()
    }
}

impl Default for TreeBuilder {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kryon_core::ElementType;

    #[test]
    fn test_tree_building() {
        let mut tree_builder = TreeBuilder::new();
        let mut taffy = TaffyTree::new();
        
        // Create test elements
        let mut elements = HashMap::new();
        let root_element = Element::new(ElementType::App);
        let child_element = Element::new(ElementType::Container);
        
        elements.insert(0, root_element);
        elements.insert(1, child_element);

        // Build tree
        let result = tree_builder.build_taffy_tree_deterministic(
            &mut taffy,
            &elements,
            0
        );
        
        assert!(result.is_ok());
        assert_eq!(tree_builder.node_count(), 2);
        assert!(!tree_builder.is_empty());
    }

    #[test]
    fn test_mapping_retrieval() {
        let mut tree_builder = TreeBuilder::new();
        let mut taffy = TaffyTree::new();
        
        let mut elements = HashMap::new();
        let root_element = Element::new(ElementType::App);
        elements.insert(0, root_element);

        tree_builder.build_taffy_tree_deterministic(
            &mut taffy,
            &elements,
            0
        ).unwrap();

        // Test mappings
        assert!(tree_builder.get_node(0).is_some());
        
        let node = tree_builder.get_node(0).unwrap();
        assert_eq!(tree_builder.get_element_id(node), Some(0));
    }

    #[test]
    fn test_clear() {
        let mut tree_builder = TreeBuilder::new();
        let mut taffy = TaffyTree::new();
        
        let mut elements = HashMap::new();
        let root_element = Element::new(ElementType::App);
        elements.insert(0, root_element);

        tree_builder.build_taffy_tree_deterministic(
            &mut taffy,
            &elements,
            0
        ).unwrap();

        assert!(!tree_builder.is_empty());
        
        tree_builder.clear();
        assert!(tree_builder.is_empty());
        assert_eq!(tree_builder.node_count(), 0);
    }
}