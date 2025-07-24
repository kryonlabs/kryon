//! KRB binary code generation

// use crate::ast::*;
use crate::error::{CompilerError, Result};
use crate::core::*;
use crate::compiler::backend::size_calculator::SizeCalculator;
use byteorder::{LittleEndian, WriteBytesExt};
use std::collections::HashMap;
use std::io::Cursor;

pub struct CodeGenerator {
    output: Vec<u8>,
    string_offsets: HashMap<u8, u32>,
    element_offsets: HashMap<usize, u32>,
}

impl CodeGenerator {
    pub fn new() -> Self {
        Self {
            output: Vec::new(),
            string_offsets: HashMap::new(),
            element_offsets: HashMap::new(),
        }
    }
    fn write_header_with_offsets(&mut self, state: &CompilerState) -> Result<()> {
        // Magic number "KRB1"
        self.output.extend_from_slice(KRB_MAGIC);
        
        // Version (Major 0, Minor 5)
        self.output.write_u16::<LittleEndian>(
            ((KRB_VERSION_MAJOR as u16) << 8) | (KRB_VERSION_MINOR as u16)
        )?;
        
        // Flags
        self.output.write_u16::<LittleEndian>(state.header_flags)?;
        
        // Count main tree elements
        let main_element_count = state.elements.iter()
            .filter(|e| !e.is_definition_root)
            .count() as u16;
        
        // Section counts
        println!("DEBUG: Writing header_with_offsets counts - elements: {}, styles: {}, strings: {}, scripts: {}", 
                main_element_count, state.styles.len(), state.strings.len(), state.scripts.len());
        self.output.write_u16::<LittleEndian>(main_element_count)?;
        self.output.write_u16::<LittleEndian>(state.styles.len() as u16)?;
        self.output.write_u16::<LittleEndian>(state.component_defs.len() as u16)?;
        self.output.write_u16::<LittleEndian>(0)?; // animation count
        self.output.write_u16::<LittleEndian>(state.scripts.len() as u16)?;
        self.output.write_u16::<LittleEndian>(state.strings.len() as u16)?;
        self.output.write_u16::<LittleEndian>(state.resources.len() as u16)?;
        self.output.write_u16::<LittleEndian>(state.template_variables.len() as u16)?;
        self.output.write_u16::<LittleEndian>(state.template_bindings.len() as u16)?;
        self.output.write_u16::<LittleEndian>(state.transforms.len() as u16)?;
        
        // Section offsets (now correct)
        self.output.write_u32::<LittleEndian>(state.element_offset)?;
        self.output.write_u32::<LittleEndian>(state.style_offset)?;
        self.output.write_u32::<LittleEndian>(state.component_def_offset)?;
        self.output.write_u32::<LittleEndian>(state.anim_offset)?;
        self.output.write_u32::<LittleEndian>(state.script_offset)?;
        self.output.write_u32::<LittleEndian>(state.string_offset)?;
        self.output.write_u32::<LittleEndian>(state.resource_offset)?;
        self.output.write_u32::<LittleEndian>(state.template_variable_offset)?;
        self.output.write_u32::<LittleEndian>(state.template_binding_offset)?;
        self.output.write_u32::<LittleEndian>(state.transform_offset)?;
        
        // Total size
        self.output.write_u32::<LittleEndian>(state.total_size)?;
        
        Ok(())
    }

    pub fn generate(&mut self, state: &mut CompilerState) -> Result<Vec<u8>> {
        self.output.clear();
        
        // 1. Calculate all section sizes first. This must be done before writing anything.
        let size_calculator = SizeCalculator::new();
        size_calculator.calculate_sizes(state)?;

        // 2. Write the header with the now-correctly-calculated offsets from the temp_state.
        self.write_header_with_offsets(state)?;

        // 3. Write the sections in the EXACT order specified by the KRB format.        
        // Section: String Table
        self.write_string_table(state)?;
        
        // Section: Element Tree
        self.write_element_tree(state)?;
        
        // Section: Style Table
        self.write_style_table(state)?;
        
        // Section: Component Definitions
        self.write_component_table(state)?;
        
        // Section: Animation Data (currently empty/reserved)
        // No write function needed if it's always empty.

        // Section: Script Table
        self.write_script_table(state)?;
        
        // Section: Resource Table
        self.write_resource_table(state)?;
        
        // Section: Template Variables
        self.write_template_variable_table(state)?;
        
        // Section: Template Bindings
        self.write_template_binding_table(state)?;
        
        // Section: Transform Data
        self.write_transform_table(state)?;
        
        // 4. Final validation (optional but good practice)
        if self.output.len() != state.total_size as usize {
            return Err(CompilerError::CodeGen {
                message: format!(
                    "Final size mismatch! Expected {}, got {}. Check section writing order.",
                    state.total_size, self.output.len()
                ),
            });
        }

        Ok(self.output.clone())
    }
    
    fn write_header(&mut self, state: &CompilerState) -> Result<()> {
        // Magic number "KRB1"
        self.output.extend_from_slice(KRB_MAGIC);
        
        // Version (little-endian) - correct format: 0x0005 for v1.2
        self.output.write_u16::<LittleEndian>(
            ((KRB_VERSION_MAJOR as u16) << 8) | (KRB_VERSION_MINOR as u16)
        )?;
        
        // Flags
        self.output.write_u16::<LittleEndian>(state.header_flags)?;
        
        // Count main tree elements (not component template elements)
        let main_element_count = state.elements.iter()
            .filter(|e| !e.is_definition_root)
            .count() as u16;
        
        // Section counts
        println!("DEBUG: Writing header counts - elements: {}, styles: {}, strings: {}, scripts: {}", 
                main_element_count, state.styles.len(), state.strings.len(), state.scripts.len());
        self.output.write_u16::<LittleEndian>(main_element_count)?;
        self.output.write_u16::<LittleEndian>(state.styles.len() as u16)?;
        self.output.write_u16::<LittleEndian>(state.component_defs.len() as u16)?;
        self.output.write_u16::<LittleEndian>(0)?; // animation count
        self.output.write_u16::<LittleEndian>(state.scripts.len() as u16)?;
        self.output.write_u16::<LittleEndian>(state.strings.len() as u16)?;
        self.output.write_u16::<LittleEndian>(state.resources.len() as u16)?;
        self.output.write_u16::<LittleEndian>(state.template_variables.len() as u16)?;
        self.output.write_u16::<LittleEndian>(state.template_bindings.len() as u16)?;
        self.output.write_u16::<LittleEndian>(state.transforms.len() as u16)?;
        
        // Section offsets (placeholders - will be updated later)
        for _ in 0..10 {
            self.output.write_u32::<LittleEndian>(0)?;
        }
        
        // Total size (placeholder)
        self.output.write_u32::<LittleEndian>(0)?;
        
        Ok(())
    }
    
    fn write_string_table(&mut self, state: &CompilerState) -> Result<()> {
        // String count (already written in header)
        
        // Write each string
        for (i, string_entry) in state.strings.iter().enumerate() {
            self.string_offsets.insert(i as u8, self.output.len() as u32);
            
            // Length byte
            let len = std::cmp::min(string_entry.text.len(), 255);
            self.output.push(len as u8);
            
            // String data (UTF-8)
            self.output.extend_from_slice(string_entry.text.as_bytes());
        }
        
        Ok(())
    }
    
    fn write_element_tree(&mut self, state: &CompilerState) -> Result<()> {
        // Find root element (first non-definition-root element)
        let root_index = state.elements.iter()
            .position(|e| !e.is_definition_root && e.parent_index.is_none())
            .ok_or_else(|| CompilerError::CodeGen {
                message: "No root element found".to_string(),
            })?;
        
        // Write elements in tree order
        self.write_element_recursive(root_index, state)?;
        
        Ok(())
    }
    fn write_element_recursive(&mut self, element_index: usize, state: &CompilerState) -> Result<()> {
        let element = &state.elements[element_index];
        self.element_offsets.insert(element_index, self.output.len() as u32);
        
        // Debug output to track what's being written
        println!("Writing element {}: type={:?} ({}), pos=({}, {}), size=({}, {}), id_string_index={}", 
                element_index, element.element_type, element.element_type as u8,
                element.pos_x, element.pos_y, element.width, element.height, element.id_string_index);
        
        // Element header (19 bytes)
        self.output.push(element.element_type as u8);
        self.output.push(element.id_string_index);
        self.output.write_u16::<LittleEndian>(element.pos_x)?;
        self.output.write_u16::<LittleEndian>(element.pos_y)?;
        self.output.write_u16::<LittleEndian>(element.width)?;
        self.output.write_u16::<LittleEndian>(element.height)?;
        self.output.push(element.layout);
        self.output.push(element.style_id);
        self.output.push(if element.checked { 1 } else { 0 });
        self.output.push(element.property_count);
        self.output.push(element.child_count);
        self.output.push(element.event_count);
        self.output.push(element.animation_count);
        self.output.push(element.custom_prop_count);
        self.output.push(element.state_prop_count);
        
        // Write standard properties
        for prop in &element.krb_properties {
            self.write_property(prop)?;
        }
        
        // Write custom properties
        for custom_prop in &element.krb_custom_properties {
            self.write_custom_property(custom_prop)?;
        }
        
        // Write state properties
        for state_set in &element.state_property_sets {
            self.write_state_property_set(state_set)?;
        }
        
        // Write events
        for event in &element.krb_events {
            self.write_event(event)?;
        }
        
        // Write child offsets (placeholders for now)
        for _ in 0..element.child_count {
            self.output.write_u16::<LittleEndian>(0)?;
        }
        
        // Recursively write children
        for &child_index in &element.children {
            self.write_element_recursive(child_index, state)?;
        }
        
        Ok(())
    }
    
    fn write_property(&mut self, prop: &KrbProperty) -> Result<()> {
        self.output.push(prop.property_id);
        self.output.push(prop.value_type as u8);
        self.output.push(prop.size);
        self.output.extend_from_slice(&prop.value);
        Ok(())
    }
    
    fn write_custom_property(&mut self, prop: &KrbCustomProperty) -> Result<()> {
        self.output.push(prop.key_index);
        self.output.push(prop.value_type as u8);
        self.output.push(prop.size);
        self.output.extend_from_slice(&prop.value);
        Ok(())
    }
    
    fn write_state_property_set(&mut self, state_set: &StatePropertySet) -> Result<()> {
        self.output.push(state_set.state_flags);
        self.output.push(state_set.property_count);
        
        for prop in &state_set.properties {
            self.write_property(prop)?;
        }
        
        Ok(())
    }
    
    fn write_event(&mut self, event: &KrbEvent) -> Result<()> {
        self.output.push(event.event_type);
        self.output.push(event.callback_id);
        Ok(())
    }
    
    fn write_style_table(&mut self, state: &CompilerState) -> Result<()> {
        // Deduplicate styles by ID to prevent writing duplicates
        let mut unique_styles = std::collections::HashMap::new();
        for style in &state.styles {
            unique_styles.insert(style.id, style);
        }
        
        println!("Writing {} unique styles to KRB (from {} total)", unique_styles.len(), state.styles.len());
        for (_, style) in unique_styles {
            // Style entry header
            println!("Writing style '{}': id={}, name_index={}, props={}", 
                style.source_name, style.id, style.name_index, style.properties.len());
            self.output.push(style.id);
            self.output.push(style.name_index);
            self.output.push(style.properties.len() as u8);
            
            // Write properties
            for prop in &style.properties {
                self.write_property(prop)?;
            }
        }
        
        Ok(())
    }
    
    fn write_component_table(&mut self, state: &CompilerState) -> Result<()> {
        for component in &state.component_defs {
            // Component header
            let name_index = state.strings.iter()
                .position(|s| s.text == component.name)
                .unwrap_or(0) as u8;
            
            self.output.push(name_index);
            self.output.push(component.properties.len() as u8);
            
            // Write property definitions
            for prop_def in &component.properties {
                let prop_name_index = state.strings.iter()
                    .position(|s| s.text == prop_def.name)
                    .unwrap_or(0) as u8;
                
                self.output.push(prop_name_index);
                self.output.push(prop_def.value_type_hint as u8);
                
                // Default value
                let default_bytes = prop_def.default_value.as_bytes();
                let default_len = std::cmp::min(default_bytes.len(), 255);
                self.output.push(default_len as u8);
                self.output.extend_from_slice(&default_bytes[..default_len]);
            }
            
            // Write template element (if any)
            if let Some(root_index) = component.definition_root_element_index {
                self.write_element_recursive(root_index, state)?;
            }
        }
        
        Ok(())
    }
    
    fn write_script_table(&mut self, state: &CompilerState) -> Result<()> {
        for script in &state.scripts {
            // Script header
            self.output.push(script.language_id as u8);
            self.output.push(script.name_index);
            self.output.push(script.storage_format);
            self.output.push(script.entry_point_count);
            self.output.write_u16::<LittleEndian>(script.data_size)?;
            
            // Write entry points
            for entry_point in &script.entry_points {
                self.output.push(entry_point.function_name_index);
            }
            
            // Write code data (if inline)
            if script.storage_format == SCRIPT_STORAGE_INLINE {
                self.output.extend_from_slice(&script.code_data);
            }
        }
        
        Ok(())
    }
    
    fn write_resource_table(&mut self, state: &CompilerState) -> Result<()> {
        for resource in &state.resources {
            self.output.push(resource.resource_type as u8);
            self.output.push(resource.name_index);
            self.output.push(resource.format as u8);
            self.output.push(resource.data_string_index);
        }
        
        Ok(())
    }
    
    fn write_template_variable_table(&mut self, state: &CompilerState) -> Result<()> {
        for template_var in &state.template_variables {
            self.output.push(template_var.name_index);
            self.output.push(template_var.value_type as u8);
            self.output.push(template_var.default_value_index);
        }
        
        Ok(())
    }
    
    fn write_template_binding_table(&mut self, state: &CompilerState) -> Result<()> {
        for binding in &state.template_bindings {
            self.output.write_u16::<LittleEndian>(binding.element_index)?;
            self.output.push(binding.property_id);
            self.output.push(binding.template_expression_index);
            self.output.push(binding.variable_count);
            
            // Write variable indices
            for &var_index in &binding.variable_indices {
                self.output.push(var_index);
            }
        }
        
        Ok(())
    }
    
    fn write_transform_table(&mut self, state: &CompilerState) -> Result<()> {
        for transform in &state.transforms {
            // Transform header
            self.output.push(transform.transform_type as u8);
            self.output.push(transform.properties.len() as u8);
            
            // Write transform properties
            for prop in &transform.properties {
                self.output.push(prop.property_type as u8);
                self.output.push(prop.value_type as u8);
                self.output.push(prop.value.len() as u8);
                self.output.extend_from_slice(&prop.value);
            }
        }
        
        Ok(())
    }
    
    fn update_header_offsets(
        &mut self,
        element_offset: u32,
        style_offset: u32,
        component_offset: u32,
        animation_offset: u32,
        script_offset: u32,
        string_offset: u32,
        resource_offset: u32,
        template_variable_offset: u32,
        template_binding_offset: u32,
        transform_offset: u32,
        total_size: u32,
    ) -> Result<()> {
        let mut cursor = Cursor::new(&mut self.output);
        
        // Skip to offset section (magic + version + flags + counts = 30 bytes)
        cursor.set_position(30);
        
        // Write offsets in correct order per spec
        cursor.write_u32::<LittleEndian>(element_offset)?;
        cursor.write_u32::<LittleEndian>(style_offset)?;
        cursor.write_u32::<LittleEndian>(component_offset)?;
        cursor.write_u32::<LittleEndian>(animation_offset)?;
        cursor.write_u32::<LittleEndian>(script_offset)?;
        cursor.write_u32::<LittleEndian>(string_offset)?;
        cursor.write_u32::<LittleEndian>(resource_offset)?;
        cursor.write_u32::<LittleEndian>(template_variable_offset)?;
        cursor.write_u32::<LittleEndian>(template_binding_offset)?;
        cursor.write_u32::<LittleEndian>(transform_offset)?;
        cursor.write_u32::<LittleEndian>(total_size)?;
        
        Ok(())
    }
}

// Add constants for script storage format
pub const SCRIPT_STORAGE_INLINE: u8 = 0;
pub const SCRIPT_STORAGE_EXTERNAL: u8 = 1;

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_header_generation() {
        let mut generator = CodeGenerator::new();
        let state = CompilerState::new();
        
        generator.write_header(&state).unwrap();
        
        // Check magic number
        assert_eq!(&generator.output[0..4], KRB_MAGIC);
        
        // Check version
        let version = u16::from_le_bytes([generator.output[4], generator.output[5]]);
        assert_eq!((version >> 8) & 0xFF, KRB_VERSION_MAJOR as u16);
        assert_eq!(version & 0xFF, KRB_VERSION_MINOR as u16);
    }
    
    #[test]
    fn test_string_table_generation() {
        let mut generator = CodeGenerator::new();
        let mut state = CompilerState::new();
        
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
        
        generator.write_string_table(&state).unwrap();
        
        // Check that strings are written correctly
        assert_eq!(generator.output[0], 0); // Empty string length
        assert_eq!(generator.output[1], 5); // "Hello" length
        assert_eq!(&generator.output[2..7], b"Hello");
    }
}
