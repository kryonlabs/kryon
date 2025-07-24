//! Calculate sizes and offsets for KRB generation

use crate::error::{CompilerError, Result};
use crate::core::*;
use crate::core::types::*;

pub struct SizeCalculator;

impl SizeCalculator {
    pub fn new() -> Self {
        Self
    }
    
    pub fn calculate_sizes(&self, state: &mut CompilerState) -> Result<()> {
        // Calculate string table size
        self.calculate_string_table_size(state);
        
        // Calculate element sizes
        self.calculate_element_sizes(state)?;
        
        // TODO: Extract header values from style properties (architectural optimization)
        // self.extract_header_values_from_styles(state)?;
        
        // Calculate style table size
        self.calculate_style_table_size(state);
        
        // Calculate component definition sizes
        self.calculate_component_def_sizes(state);
        
        // Calculate script table size
        self.calculate_script_table_size(state);
        
        // Calculate resource table size
        self.calculate_resource_table_size(state);
        
        // Calculate template variable sizes
        self.calculate_template_variable_sizes(state);
        
        // Calculate transform data sizes
        self.calculate_transform_sizes(state);
        
        // Calculate section offsets
        self.calculate_section_offsets(state);
        
        Ok(())
    }
    
    fn calculate_string_table_size(&self, state: &mut CompilerState) {
        let mut total_size = 0u32;
        
        for string_entry in &mut state.strings {
            // Each string: 1 byte length + string data
            let string_size = 1 + string_entry.text.len();
            total_size += string_size as u32;
        }
        
        state.total_string_data_size = total_size;
    }
    
    fn calculate_element_sizes(&self, state: &mut CompilerState) -> Result<()> {
        let mut total_size = 0u32;
        
        for element in &mut state.elements {
            let mut element_size = KRB_ELEMENT_HEADER_SIZE as u32;
            
            // Add property sizes
            for prop in &element.krb_properties {
                element_size += 3 + prop.size as u32; // prop_id + value_type + size + data
            }
            
            // Add custom property sizes
            for custom_prop in &element.krb_custom_properties {
                element_size += 3 + custom_prop.size as u32; // key_index + value_type + size + data
            }
            
            // Add state property set sizes
            for state_set in &element.state_property_sets {
                element_size += 2; // state_flags + property_count
                for prop in &state_set.properties {
                    element_size += 3 + prop.size as u32;
                }
            }
            
            // Add event sizes
            for _event in &element.krb_events {
                element_size += 2; // event_type + callback_id
            }
            
            // Add child offset space
            element_size += element.child_count as u32 * 2; // Each child offset is 2 bytes
            
            element.calculated_size = element_size;
            total_size += element_size;
        }
        
        state.total_element_data_size = total_size;
        Ok(())
    }
    
    fn calculate_style_table_size(&self, state: &mut CompilerState) {
        let mut total_size = 0u32;
        
        for style in &mut state.styles {
            let mut style_size = 3u32; // id + name_index + property_count
            
            for prop in &style.properties {
                style_size += 3 + prop.size as u32; // prop_id + value_type + size + data
            }
            
            style.calculated_size = style_size;
            total_size += style_size;
        }
        
        state.total_style_data_size = total_size;
    }
    
    fn calculate_component_def_sizes(&self, state: &mut CompilerState) {
        let mut total_size = 0u32;
        
        for component in &mut state.component_defs {
            let mut comp_size = 2u32; // name_index + property_count
            
            // Add property definition sizes
            for prop_def in &component.properties {
                comp_size += 3; // prop_name_index + value_type_hint + default_value_length
                comp_size += prop_def.default_value.len() as u32;
            }
            
            // Add template element size (if any)
            if let Some(root_index) = component.definition_root_element_index {
                if let Some(element) = state.elements.get(root_index) {
                    comp_size += element.calculated_size;
                }
            }
            
            component.calculated_size = comp_size;
            total_size += comp_size;
        }
        
        state.total_component_def_data_size = total_size;
    }
    
    fn calculate_script_table_size(&self, state: &mut CompilerState) {
        let mut total_size = 0u32;
        
        for script in &mut state.scripts {
            let mut script_size = 6u32; // language_id + name_index + storage_format + entry_point_count + data_size (2 bytes)
            
            // Add entry point sizes
            script_size += script.entry_points.len() as u32; // Each entry point is 1 byte (function name index)
            
            // Add code data size (if inline)
            if script.storage_format == 0 { // ScriptStorageInline
                script_size += script.code_data.len() as u32;
            }
            
            script.calculated_size = script_size;
            total_size += script_size;
        }
        
        state.total_script_data_size = total_size;
    }
    
    fn calculate_resource_table_size(&self, state: &mut CompilerState) {
        let mut total_size = 0u32;
        
        for resource in &mut state.resources {
            let resource_size = 4u32; // resource_type + name_index + format + data_string_index
            resource.calculated_size = resource_size;
            total_size += resource_size;
        }
        
        state.total_resource_table_size = total_size;
    }
    
    fn calculate_template_variable_sizes(&self, state: &mut CompilerState) {
        // Calculate template variables table size
        let mut template_var_size = 0u32;
        for _template_var in &state.template_variables {
            // Each template variable: name_index (1) + value_type (1) + default_value_index (1) = 3 bytes
            template_var_size += 3;
        }
        state.total_template_variable_size = template_var_size;
        
        // Calculate template bindings table size
        let mut template_binding_size = 0u32;
        for binding in &state.template_bindings {
            // Each binding: element_index (2) + property_id (1) + expression_index (1) + variable_count (1) + variable_indices
            template_binding_size += 5 + binding.variable_indices.len() as u32;
        }
        state.total_template_binding_size = template_binding_size;
    }
    
    fn calculate_transform_sizes(&self, state: &mut CompilerState) {
        let mut total_size = 0u32;
        
        for transform in &state.transforms {
            // Each transform: transform_type (1) + property_count (1) = 2 bytes
            let mut transform_size = 2u32;
            
            // Add property sizes
            for prop in &transform.properties {
                // Each property: property_type (1) + value_type (1) + size (1) + data
                transform_size += 3 + prop.value.len() as u32;
            }
            
            total_size += transform_size;
        }
        
        state.total_transform_size = total_size;
    }
        
    pub fn calculate_section_offsets(&self, state: &mut CompilerState) {
        let mut current_offset = KRB_HEADER_SIZE as u32;
        
        println!("DEBUG: Calculating offsets - header size = {} (0x{:X})", KRB_HEADER_SIZE, KRB_HEADER_SIZE);
        
        // The order of these additions MUST match the physical write order in codegen.
        
        // 1. String table
        state.string_offset = current_offset;
        println!("DEBUG: String offset = {} (0x{:X}), string size = {}", current_offset, current_offset, state.total_string_data_size);
        current_offset += state.total_string_data_size;
        
        // 2. Element tree
        state.element_offset = current_offset;
        println!("DEBUG: Element offset = {} (0x{:X}), element size = {}", current_offset, current_offset, state.total_element_data_size);
        current_offset += state.total_element_data_size;
        
        // 3. Style table
        state.style_offset = current_offset;
        current_offset += state.total_style_data_size;
        
        // --- THIS IS THE LIKELY BUG LOCATION ---
        // 4. Component definitions
        // Ensure this line is present and correct. It was likely missing or flawed.
        state.component_def_offset = current_offset;
        current_offset += state.total_component_def_data_size;
        
        // 5. Animations (currently reserved, size is 0)
        state.anim_offset = current_offset;
        // current_offset += state.total_anim_data_size; // No size to add yet

        // 6. Scripts
        state.script_offset = current_offset;
        current_offset += state.total_script_data_size;
        
        // 7. Resources
        state.resource_offset = current_offset;
        current_offset += state.total_resource_table_size;
        
        // 8. Template Variables
        state.template_variable_offset = current_offset;
        current_offset += state.total_template_variable_size;
        
        // 9. Template Bindings
        state.template_binding_offset = current_offset;
        current_offset += state.total_template_binding_size;
        
        // 10. Transform Data
        state.transform_offset = current_offset;
        current_offset += state.total_transform_size;
        
        // 11. Total file size
        state.total_size = current_offset;
    }
    /// Validate that all sizes are within limits
    pub fn validate_limits(&self, state: &CompilerState) -> Result<()> {
        // Check element count
        if state.elements.len() > MAX_ELEMENTS {
            return Err(CompilerError::LimitExceeded {
                limit_type: "elements".to_string(),
                limit: MAX_ELEMENTS,
            });
        }
        
        // Check string count
        if state.strings.len() > MAX_STRINGS {
            return Err(CompilerError::LimitExceeded {
                limit_type: "strings".to_string(),
                limit: MAX_STRINGS,
            });
        }
        
        // Check style count
        if state.styles.len() > MAX_STYLES {
            return Err(CompilerError::LimitExceeded {
                limit_type: "styles".to_string(),
                limit: MAX_STYLES,
            });
        }
        
        // Check component count
        if state.component_defs.len() > MAX_COMPONENT_DEFS {
            return Err(CompilerError::LimitExceeded {
                limit_type: "component definitions".to_string(),
                limit: MAX_COMPONENT_DEFS,
            });
        }
        
        // Check resource count
        if state.resources.len() > MAX_RESOURCES {
            return Err(CompilerError::LimitExceeded {
                limit_type: "resources".to_string(),
                limit: MAX_RESOURCES,
            });
        }
        
        // Check total file size (2GB limit for safety)
        if state.total_size > 2_147_483_648 {
            return Err(CompilerError::CodeGen {
                message: "Generated file would exceed 2GB size limit".to_string(),
            });
        }
        
        // Check individual element sizes
        for (i, element) in state.elements.iter().enumerate() {
            if element.calculated_size > 65535 {
                return Err(CompilerError::CodeGen {
                    message: format!(
                        "Element {} size ({} bytes) exceeds maximum (65535 bytes)",
                        i, element.calculated_size
                    ),
                });
            }
            
            if element.krb_properties.len() > MAX_PROPERTIES {
                return Err(CompilerError::LimitExceeded {
                    limit_type: format!("properties for element {}", i),
                    limit: MAX_PROPERTIES,
                });
            }
            
            if element.krb_custom_properties.len() > MAX_CUSTOM_PROPERTIES {
                return Err(CompilerError::LimitExceeded {
                    limit_type: format!("custom properties for element {}", i),
                    limit: MAX_CUSTOM_PROPERTIES,
                });
            }
            
            if element.children.len() > MAX_CHILDREN {
                return Err(CompilerError::LimitExceeded {
                    limit_type: format!("children for element {}", i),
                    limit: MAX_CHILDREN,
                });
            }
        }
        
        Ok(())
    }
    
    /// Get size statistics for reporting
    pub fn get_size_stats(&self, state: &CompilerState) -> SizeStatistics {
        SizeStatistics {
            total_size: state.total_size,
            header_size: KRB_HEADER_SIZE as u32,
            string_table_size: state.total_string_data_size,
            element_tree_size: state.total_element_data_size,
            style_table_size: state.total_style_data_size,
            component_def_size: state.total_component_def_data_size,
            script_table_size: state.total_script_data_size,
            resource_table_size: state.total_resource_table_size,
            template_variable_size: state.total_template_variable_size,
            template_binding_size: state.total_template_binding_size,
            transform_size: state.total_transform_size,
            element_count: state.elements.len(),
            string_count: state.strings.len(),
            style_count: state.styles.len(),
            component_count: state.component_defs.len(),
            script_count: state.scripts.len(),
            resource_count: state.resources.len(),
            template_variable_count: state.template_variables.len(),
            template_binding_count: state.template_bindings.len(),
            transform_count: state.transforms.len(),
        }
    }
    
    /// Extract style property values to element headers for renderer optimization
    /// This is the architecturally correct place - after element parsing but before finalization
    fn extract_header_values_from_styles(&self, state: &mut CompilerState) -> Result<()> {
        // Process each element and extract relevant style properties to headers
        for element_index in 0..state.elements.len() {
            let style_id = state.elements[element_index].style_id;
            
            // Skip elements without styles
            if style_id == 0 {
                continue;
            }
            
            // Find the style and extract header values
            if let Some(style) = state.styles.iter().find(|s| s.id == style_id) {
                for property in &style.properties {
                    match property.property_id {
                        0x1A => { // PropertyId::Width
                            if property.value_type as u8 == ValueType::Short as u8 && property.size == 2 {
                                let width = u16::from_le_bytes([property.value[0], property.value[1]]);
                                state.elements[element_index].width = width;
                            }
                        },
                        0x1C => { // PropertyId::Height
                            if property.value_type as u8 == ValueType::Short as u8 && property.size == 2 {
                                let height = u16::from_le_bytes([property.value[0], property.value[1]]);
                                state.elements[element_index].height = height;
                            }
                        },
                        0x51 => { // PropertyId::Left -> pos_x
                            if property.value_type as u8 == ValueType::Short as u8 && property.size == 2 {
                                let pos_x = u16::from_le_bytes([property.value[0], property.value[1]]);
                                state.elements[element_index].pos_x = pos_x;
                            }
                        },
                        0x52 => { // PropertyId::Top -> pos_y
                            if property.value_type as u8 == ValueType::Short as u8 && property.size == 2 {
                                let pos_y = u16::from_le_bytes([property.value[0], property.value[1]]);
                                state.elements[element_index].pos_y = pos_y;
                            }
                        },
                        _ => {} // Ignore other properties
                    }
                }
            }
        }
        
        Ok(())
    }
}

#[derive(Debug, Clone)]
pub struct SizeStatistics {
    pub total_size: u32,
    pub header_size: u32,
    pub string_table_size: u32,
    pub element_tree_size: u32,
    pub style_table_size: u32,
    pub component_def_size: u32,
    pub script_table_size: u32,
    pub resource_table_size: u32,
    pub template_variable_size: u32,
    pub template_binding_size: u32,
    pub transform_size: u32,
    pub element_count: usize,
    pub string_count: usize,
    pub style_count: usize,
    pub component_count: usize,
    pub script_count: usize,
    pub resource_count: usize,
    pub template_variable_count: usize,
    pub template_binding_count: usize,
    pub transform_count: usize,
}

impl SizeStatistics {
    pub fn print_breakdown(&self) {
        println!("KRB Size Breakdown:");
        println!("  Total size: {} bytes", self.total_size);
        println!("  Header: {} bytes ({:.1}%)", 
                self.header_size, 
                self.header_size as f64 / self.total_size as f64 * 100.0);
        println!("  String table: {} bytes ({:.1}%)", 
                self.string_table_size,
                self.string_table_size as f64 / self.total_size as f64 * 100.0);
        println!("  Element tree: {} bytes ({:.1}%)", 
                self.element_tree_size,
                self.element_tree_size as f64 / self.total_size as f64 * 100.0);
        println!("  Style table: {} bytes ({:.1}%)", 
                self.style_table_size,
                self.style_table_size as f64 / self.total_size as f64 * 100.0);
        println!("  Component defs: {} bytes ({:.1}%)", 
                self.component_def_size,
                self.component_def_size as f64 / self.total_size as f64 * 100.0);
        println!("  Script table: {} bytes ({:.1}%)", 
                self.script_table_size,
                self.script_table_size as f64 / self.total_size as f64 * 100.0);
        println!("  Resource table: {} bytes ({:.1}%)", 
                self.resource_table_size,
                self.resource_table_size as f64 / self.total_size as f64 * 100.0);
        println!("  Template variables: {} bytes ({:.1}%)", 
                self.template_variable_size,
                self.template_variable_size as f64 / self.total_size as f64 * 100.0);
        println!("  Template bindings: {} bytes ({:.1}%)", 
                self.template_binding_size,
                self.template_binding_size as f64 / self.total_size as f64 * 100.0);
        println!("  Transform data: {} bytes ({:.1}%)", 
                self.transform_size,
                self.transform_size as f64 / self.total_size as f64 * 100.0);
        
        println!("\nCounts:");
        println!("  Elements: {}", self.element_count);
        println!("  Strings: {}", self.string_count);
        println!("  Styles: {}", self.style_count);
        println!("  Components: {}", self.component_count);
        println!("  Scripts: {}", self.script_count);
        println!("  Resources: {}", self.resource_count);
        println!("  Template variables: {}", self.template_variable_count);
        println!("  Template bindings: {}", self.template_binding_count);
        println!("  Transforms: {}", self.transform_count);
    }
    
    pub fn compression_ratio(&self, original_size: u64) -> f64 {
        if original_size > 0 {
            self.total_size as f64 / original_size as f64
        } else {
            0.0
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_string_table_size_calculation() {
        let calculator = SizeCalculator::new();
        let mut state = CompilerState::new();
        
        // Add some test strings
        state.strings.push(StringEntry {
            text: "".to_string(),
            length: 0,
            index: 0,
        });
        
        state.strings.push(StringEntry {
            text: "Hello".to_string(),
            length: 5,
            index: 1,
        });
        
        state.strings.push(StringEntry {
            text: "World".to_string(),
            length: 5,
            index: 2,
        });
        
        calculator.calculate_string_table_size(&mut state);
        
        // Expected: 1 + 0 + 1 + 5 + 1 + 5 = 13 bytes
        assert_eq!(state.total_string_data_size, 13);
    }
    
    #[test]
    fn test_section_offset_calculation() {
        let calculator = SizeCalculator::new();
        let mut state = CompilerState::new();
        
        state.total_string_data_size = 100;
        state.total_element_data_size = 200;
        state.total_style_data_size = 50;
        state.total_component_def_data_size = 75;
        state.total_script_data_size = 25;
        state.total_resource_table_size = 10;
        
        calculator.calculate_section_offsets(&mut state);
        
        assert_eq!(state.string_offset, KRB_HEADER_SIZE as u32);
        assert_eq!(state.element_offset, KRB_HEADER_SIZE as u32 + 100);
        assert_eq!(state.style_offset, KRB_HEADER_SIZE as u32 + 100 + 200);
        assert_eq!(state.total_size, KRB_HEADER_SIZE as u32 + 100 + 200 + 50 + 75 + 25 + 10);
    }
}
