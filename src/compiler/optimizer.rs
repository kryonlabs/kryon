//! Optimization passes for KRB generation

use crate::error::{CompilerError, Result};
use crate::core::*;
use crate::core::types::*;
use std::collections::{HashMap, HashSet};

pub struct Optimizer {
    optimizations_applied: Vec<String>,
    size_savings: HashMap<String, u32>,
}

impl Optimizer {
    pub fn new() -> Self {
        Self {
            optimizations_applied: Vec::new(),
            size_savings: HashMap::new(),
        }
    }
    
    pub fn optimize(&mut self, state: &mut CompilerState, level: u8) -> Result<()> {
        match level {
            0 => {
                // No optimizations
            }
            1 => {
                // Basic optimizations
                self.optimize_string_deduplication(state)?;
                self.optimize_property_sharing(state)?;
            }
            2 => {
                // Aggressive optimizations
                self.optimize_string_deduplication(state)?;
                self.optimize_property_sharing(state)?;
                self.optimize_dead_code_elimination(state)?;
                self.optimize_element_merging(state)?;
            }
            _ => {
                return Err(CompilerError::CodeGen {
                    message: format!("Invalid optimization level: {}", level),
                });
            }
        }
        
        Ok(())
    }
    
    /// Remove duplicate strings and update references
    fn optimize_string_deduplication(&mut self, state: &mut CompilerState) -> Result<()> {
        let initial_count = state.strings.len();
        let mut string_map = HashMap::new();
        let mut new_strings = Vec::new();
        let mut index_mapping = HashMap::new();
        
        // Build deduplicated string table
        for (old_index, string_entry) in state.strings.iter().enumerate() {
            if let Some(&new_index) = string_map.get(&string_entry.text) {
                // String already exists, map old index to existing new index
                index_mapping.insert(old_index as u8, new_index);
            } else {
                // New string, add to deduplicated table
                let new_index = new_strings.len() as u8;
                string_map.insert(string_entry.text.clone(), new_index);
                index_mapping.insert(old_index as u8, new_index);
                
                new_strings.push(StringEntry {
                    text: string_entry.text.clone(),
                    length: string_entry.length,
                    index: new_index,
                });
            }
        }
        
        // Update string references throughout the state
        self.update_string_references(state, &index_mapping)?;
        
        // Replace string table
        state.strings = new_strings;
        
        let saved_strings = initial_count - state.strings.len();
        if saved_strings > 0 {
            self.optimizations_applied.push("String deduplication".to_string());
            self.size_savings.insert("string_deduplication".to_string(), saved_strings as u32);
            log::info!("String deduplication: {} strings -> {} strings (saved {})", 
                      initial_count, state.strings.len(), saved_strings);
        }
        
        Ok(())
    }
    
    fn update_string_references(&self, state: &mut CompilerState, mapping: &HashMap<u8, u8>) -> Result<()> {
        // Update element string references
        for element in &mut state.elements {
            if let Some(&new_index) = mapping.get(&element.id_string_index) {
                element.id_string_index = new_index;
            }
            
            // Update property string references
            for prop in &mut element.krb_properties {
                if prop.value_type == ValueType::String && prop.size == 1 && !prop.value.is_empty() {
                    if let Some(&new_index) = mapping.get(&prop.value[0]) {
                        prop.value[0] = new_index;
                    }
                }
            }
            
            // Update custom property string references
            for custom_prop in &mut element.krb_custom_properties {
                if let Some(&new_index) = mapping.get(&custom_prop.key_index) {
                    custom_prop.key_index = new_index;
                }
                if custom_prop.value_type == ValueType::String && custom_prop.size == 1 && !custom_prop.value.is_empty() {
                    if let Some(&new_index) = mapping.get(&custom_prop.value[0]) {
                        custom_prop.value[0] = new_index;
                    }
                }
            }
        }
        
        // Update style string references
        for style in &mut state.styles {
            if let Some(&new_index) = mapping.get(&style.name_index) {
                style.name_index = new_index;
            }
            
            for prop in &mut style.properties {
                if prop.value_type == ValueType::String && prop.size == 1 && !prop.value.is_empty() {
                    if let Some(&new_index) = mapping.get(&prop.value[0]) {
                        prop.value[0] = new_index;
                    }
                }
            }
        }
        
        // Update component definition string references
        for component in &mut state.component_defs {
            // Update property name references
            for _prop_def in &mut component.properties {
                // Note: prop_def doesn't have direct string indices in our current structure
                // This would need to be updated if we store string indices for property names
            }
        }
        
        // Update script string references
        for script in &mut state.scripts {
            if let Some(&new_index) = mapping.get(&script.name_index) {
                script.name_index = new_index;
            }
            
            for entry_point in &mut script.entry_points {
                if let Some(&new_index) = mapping.get(&entry_point.function_name_index) {
                    entry_point.function_name_index = new_index;
                }
            }
        }
        
        // Update resource string references
        for resource in &mut state.resources {
            if let Some(&new_index) = mapping.get(&resource.name_index) {
                resource.name_index = new_index;
            }
            if let Some(&new_index) = mapping.get(&resource.data_string_index) {
                resource.data_string_index = new_index;
            }
        }
        
        Ok(())
    }
    
    /// Share identical property blocks between elements
    fn optimize_property_sharing(&mut self, state: &mut CompilerState) -> Result<()> {
        let _initial_props = self.count_total_properties(state);
        let mut property_blocks = HashMap::new();
        let mut shared_blocks = 0;
        
        // Group elements by their property signatures
        for element in &mut state.elements {
            if !element.krb_properties.is_empty() {
                let signature = self.calculate_property_signature(&element.krb_properties);
                
                property_blocks.entry(signature)
                    .or_insert_with(Vec::new)
                    .push(element.self_index);
            }
        }
        
        // Count shared property blocks
        for (_, element_indices) in property_blocks {
            if element_indices.len() > 1 {
                shared_blocks += element_indices.len() - 1; // One original + N-1 sharing
            }
        }
        
        if shared_blocks > 0 {
            self.optimizations_applied.push("Property block sharing".to_string());
            self.size_savings.insert("property_sharing".to_string(), shared_blocks as u32);
            log::info!("Property sharing: {} property blocks can be shared", shared_blocks);
        }
        
        Ok(())
    }
    
    fn calculate_property_signature(&self, properties: &[KrbProperty]) -> String {
        let mut signature = String::new();
        
        for prop in properties {
            signature.push_str(&format!("{}:{}:{},", 
                                      prop.property_id, 
                                      prop.value_type as u8,
                                      hex::encode(&prop.value)));
        }
        
        signature
    }
    
    /// Remove unused styles, components, and resources
    fn optimize_dead_code_elimination(&mut self, state: &mut CompilerState) -> Result<()> {
        let mut eliminated_count = 0;
        
        // Find unused styles
        let used_styles = self.find_used_styles(state);
        let unused_styles: Vec<_> = state.styles.iter()
            .enumerate()
            .filter(|(_, style)| !used_styles.contains(&style.source_name))
            .map(|(i, _)| i)
            .collect();
        
        // Remove unused styles (in reverse order to maintain indices)
        for &index in unused_styles.iter().rev() {
            state.styles.remove(index);
            eliminated_count += 1;
        }
        
        // Find unused components
        let used_components = self.find_used_components(state);
        let unused_components: Vec<_> = state.component_defs.iter()
            .enumerate()
            .filter(|(_, comp)| !used_components.contains(&comp.name))
            .map(|(i, _)| i)
            .collect();
        
        // Remove unused components
        for &index in unused_components.iter().rev() {
            state.component_defs.remove(index);
            eliminated_count += 1;
        }
        
        // Find unused resources
        let used_resources = self.find_used_resources(state);
        let unused_resources: Vec<_> = state.resources.iter()
            .enumerate()
            .filter(|(i, _)| !used_resources.contains(i))
            .map(|(i, _)| i)
            .collect();
        
        // Remove unused resources
        for &index in unused_resources.iter().rev() {
            state.resources.remove(index);
            eliminated_count += 1;
        }
        
        if eliminated_count > 0 {
            self.optimizations_applied.push("Dead code elimination".to_string());
            self.size_savings.insert("dead_code_elimination".to_string(), eliminated_count);
            log::info!("Dead code elimination: removed {} unused definitions", eliminated_count);
        }
        
        Ok(())
    }
    
    fn find_used_styles(&self, state: &CompilerState) -> HashSet<String> {
        let mut used_styles = HashSet::new();
        
        // Check element style references
        for element in &state.elements {
            if element.style_id > 0 {
                if let Some(style) = state.styles.get((element.style_id - 1) as usize) {
                    used_styles.insert(style.source_name.clone());
                    
                    // Also mark extended styles as used
                    self.mark_extended_styles_as_used(&style.source_name, state, &mut used_styles);
                }
            }
        }
        
        // Check style inheritance chains
        for style in &state.styles {
            if used_styles.contains(&style.source_name) {
                for extended_style in &style.extends_style_names {
                    self.mark_extended_styles_as_used(extended_style, state, &mut used_styles);
                }
            }
        }
        
        used_styles
    }
    
    fn mark_extended_styles_as_used(&self, style_name: &str, state: &CompilerState, used_styles: &mut HashSet<String>) {
        if used_styles.insert(style_name.to_string()) {
            // If this style wasn't already marked, mark its dependencies too
            if let Some(style) = state.styles.iter().find(|s| s.source_name == style_name) {
                for extended_style in &style.extends_style_names {
                    self.mark_extended_styles_as_used(extended_style, state, used_styles);
                }
            }
        }
    }
    
    fn find_used_components(&self, state: &CompilerState) -> HashSet<String> {
        let mut used_components = HashSet::new();
        
        for element in &state.elements {
            if element.is_component_instance {
                if let Some(_component_def) = &element.component_def {
                    used_components.insert(element.source_element_name.clone());
                }
            }
        }
        
        used_components
    }
    
    fn find_used_resources(&self, state: &CompilerState) -> HashSet<usize> {
        let mut used_resources = HashSet::new();
        
        // Check element property references to resources
        for element in &state.elements {
            for prop in &element.krb_properties {
                if prop.value_type == ValueType::Resource && !prop.value.is_empty() {
                    used_resources.insert(prop.value[0] as usize);
                }
            }
        }
        
        // Check script external references
        for script in &state.scripts {
            if let Some(resource_index) = script.resource_index {
                used_resources.insert(resource_index as usize);
            }
        }
        
        used_resources
    }
    
    /// Merge consecutive similar elements
    fn optimize_element_merging(&mut self, state: &mut CompilerState) -> Result<()> {
        let _initial_count = state.elements.len();
        let mut merged_count = 0;
        
        // This is a simple example - in practice, element merging is complex
        // and must preserve semantic meaning
        
        // For now, just merge consecutive Text elements with identical properties
        // (except for text content)
        let mut i = 0;
        while i < state.elements.len() - 1 {
            let can_merge = {
                let current = &state.elements[i];
                let next = &state.elements[i + 1];
                
                current.element_type == ElementType::Text &&
                next.element_type == ElementType::Text &&
                current.parent_index == next.parent_index &&
                self.elements_have_similar_properties(current, next)
            };
            
            if can_merge {
                // Merge the text content
                self.merge_text_elements(state, i)?;
                merged_count += 1;
            } else {
                i += 1;
            }
        }
        
        if merged_count > 0 {
            self.optimizations_applied.push("Element merging".to_string());
            self.size_savings.insert("element_merging".to_string(), merged_count);
            log::info!("Element merging: merged {} elements", merged_count);
        }
        
        Ok(())
    }
    
    fn elements_have_similar_properties(&self, elem1: &Element, elem2: &Element) -> bool {
        // Check if properties are identical (except text content)
        if elem1.krb_properties.len() != elem2.krb_properties.len() {
            return false;
        }
        
        for (prop1, prop2) in elem1.krb_properties.iter().zip(&elem2.krb_properties) {
            if prop1.property_id == PropertyId::TextContent as u8 {
                continue; // Skip text content comparison
            }
            
            if prop1.property_id != prop2.property_id ||
               prop1.value_type != prop2.value_type ||
               prop1.value != prop2.value {
                return false;
            }
        }
        
        true
    }
    
    fn merge_text_elements(&mut self, state: &mut CompilerState, index: usize) -> Result<()> {
        // This is a simplified implementation
        // In practice, you'd need to carefully merge text content and update parent references
        
        let text1 = self.get_text_content(&state.elements[index], state)?;
        let text2 = self.get_text_content(&state.elements[index + 1], state)?;
        let merged_text = format!("{} {}", text1, text2);
        
        // Update the first element with merged text
        let string_index = state.strings.len() as u8;
        state.strings.push(StringEntry {
            length: merged_text.len(),
            text: merged_text,
            index: string_index,
        });
        
        // Find existing text property and update it, or add new one
        let mut found = false;
        for prop in &mut state.elements[index].krb_properties {
            if prop.property_id == PropertyId::TextContent as u8 {
                prop.value = vec![string_index];
                found = true;
                break;
            }
        }

        if !found {
            state.elements[index].krb_properties.push(KrbProperty {
                property_id: PropertyId::TextContent as u8,
                value_type: ValueType::String,
                size: 1,
                value: vec![string_index],
            });
        }

        // Remove the second element
        state.elements.remove(index + 1);
        
        // Update parent's child references
        self.update_parent_child_references(state, index + 1)?;
        
        Ok(())
    }
    
    fn get_text_content(&self, element: &Element, state: &CompilerState) -> Result<String> {
        for prop in &element.krb_properties {
            if prop.property_id == PropertyId::TextContent as u8 &&
               prop.value_type == ValueType::String &&
               !prop.value.is_empty() {
                let string_index = prop.value[0] as usize;
                if let Some(string_entry) = state.strings.get(string_index) {
                    return Ok(string_entry.text.clone());
                }
            }
        }
        
        Ok(String::new())
    }
    
    fn set_text_content(&self, element: &mut Element, text: &str, state: &mut CompilerState) -> Result<()> {
        // Find existing text property and update it
        for prop in &mut element.krb_properties {
            if prop.property_id == PropertyId::TextContent as u8 {
                // Add new string to string table
                let string_index = state.strings.len() as u8;
                state.strings.push(StringEntry {
                    text: text.to_string(),
                    length: text.len(),
                    index: string_index,
                });
                
                // Update property value
                prop.value = vec![string_index];
                return Ok(());
            }
        }
        
        // If no text property exists, create one
        let string_index = state.strings.len() as u8;
        state.strings.push(StringEntry {
            text: text.to_string(),
            length: text.len(),
            index: string_index,
        });
        
        element.krb_properties.push(KrbProperty {
            property_id: PropertyId::TextContent as u8,
            value_type: ValueType::String,
            size: 1,
            value: vec![string_index],
        });
        
        element.property_count = element.krb_properties.len() as u8;
        
        Ok(())
    }
    
    fn update_parent_child_references(&self, state: &mut CompilerState, removed_index: usize) -> Result<()> {
        // Update all child indices that are greater than removed_index
        for element in &mut state.elements {
            for child_index in &mut element.children {
                if *child_index > removed_index {
                    *child_index -= 1;
                }
            }
            
            // Remove the deleted child from parent's child list
            element.children.retain(|&child| child != removed_index);
            element.child_count = element.children.len() as u8;
        }
        
        // Update self_index for all elements after the removed one
        for (i, element) in state.elements.iter_mut().enumerate() {
            element.self_index = i;
        }
        
        Ok(())
    }
    
    fn count_total_properties(&self, state: &CompilerState) -> usize {
        state.elements.iter()
            .map(|e| e.krb_properties.len() + e.krb_custom_properties.len())
            .sum()
    }
    
    /// Get optimization statistics
    pub fn get_optimization_stats(&self) -> OptimizationStats {
        OptimizationStats {
            optimizations_applied: self.optimizations_applied.clone(),
            size_savings: self.size_savings.clone(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct OptimizationStats {
    pub optimizations_applied: Vec<String>,
    pub size_savings: HashMap<String, u32>,
}

impl OptimizationStats {
    pub fn print_summary(&self) {
        if self.optimizations_applied.is_empty() {
            println!("No optimizations applied");
            return;
        }
        
        println!("Optimizations Applied:");
        for optimization in &self.optimizations_applied {
            println!("  ✓ {}", optimization);
            
            if let Some(&savings) = self.size_savings.get(&optimization.to_lowercase().replace(" ", "_")) {
                println!("    Saved: {} items", savings);
            }
        }
        
        let total_savings: u32 = self.size_savings.values().sum();
        if total_savings > 0 {
            println!("Total savings: {} items", total_savings);
        }
    }
    
    pub fn total_savings(&self) -> u32 {
        self.size_savings.values().sum()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_property_signature_calculation() {
        let optimizer = Optimizer::new();
        
        let prop1 = KrbProperty {
            property_id: 1,
            value_type: ValueType::Color,
            size: 4,
            value: vec![255, 0, 0, 255],
        };
        
        let prop2 = KrbProperty {
            property_id: 2,
            value_type: ValueType::Byte,
            size: 1,
            value: vec![10],
        };
        
        let signature = optimizer.calculate_property_signature(&[prop1, prop2]);
        assert!(!signature.is_empty());
        assert!(signature.contains("1:3:ff0000ff")); // property_id:value_type:hex_value
    }
    
    #[test]
    fn test_string_deduplication() {
        let mut optimizer = Optimizer::new();
        let mut state = CompilerState::new();
        
        // Add duplicate strings
        state.strings.push(StringEntry { text: "Hello".to_string(), length: 5, index: 0 });
        state.strings.push(StringEntry { text: "World".to_string(), length: 5, index: 1 });
        state.strings.push(StringEntry { text: "Hello".to_string(), length: 5, index: 2 }); // Duplicate
        
        let initial_count = state.strings.len();
        optimizer.optimize_string_deduplication(&mut state).unwrap();
        
        assert!(state.strings.len() < initial_count);
        assert_eq!(state.strings.len(), 2); // Should have only 2 unique strings
    }
}
