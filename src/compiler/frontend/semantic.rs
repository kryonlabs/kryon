//! Semantic analysis and validation for the Kryon compiler

use crate::compiler::frontend::ast::*;
use crate::compiler::middle_end::script::ScriptProcessor;
use crate::error::{CompilerError, Result};
use crate::core::*;
use crate::core::types::*;
use crate::core::util::{clean_and_quote_value, parse_color};
use std::collections::{HashMap, HashSet};

pub struct SemanticAnalyzer {
    errors: Vec<CompilerError>,
    warnings: Vec<String>,
    variable_usage: HashMap<String, Vec<usize>>, // Track where variables are used
}

impl SemanticAnalyzer {
    pub fn new() -> Self {
        Self {
            errors: Vec::new(),
            warnings: Vec::new(),
            variable_usage: HashMap::new(),
        }
    }
    
    pub fn analyze(&mut self, ast: &mut AstNode, state: &mut CompilerState) -> Result<()> {
        // Phase 1: Collect all definitions
        self.collect_definitions(ast, state)?;
        
        // Phase 2: Resolve dependencies
        self.resolve_dependencies(state)?;
        
        // Phase 3: Validate elements and properties
        self.validate_elements(ast, state)?;
        
        // Phase 4: Check for unused definitions
        self.check_unused_definitions(state)?;
        
        // Return any accumulated errors
        if !self.errors.is_empty() {
            return Err(self.errors.remove(0));
        }
        
        // Log warnings
        for warning in &self.warnings {
            log::warn!("{}", warning);
        }
        
        Ok(())
    }
    
    fn collect_definitions(&mut self, ast: &AstNode, state: &mut CompilerState) -> Result<()> {
        match ast {
            AstNode::File { styles, components, scripts, directives, .. } => {
                // Collect directives (variables)
                for directive_node in directives {
                    self.collect_directive_definition(directive_node, state)?;
                }
                
                // Collect styles
                for style_node in styles {
                    self.collect_style_definition(style_node, state)?;
                }
                
                // Collect components
                for component_node in components {
                    self.collect_component_definition(component_node, state)?;
                }
                
                // Collect scripts
                for script_node in scripts {
                    self.collect_script_definition(script_node, state)?;
                }
            }
            _ => {}
        }
        
        Ok(())
    }
    
    fn collect_style_definition(&mut self, ast: &AstNode, state: &mut CompilerState) -> Result<()> {
        if let AstNode::Style { name, extends, properties, pseudo_selectors: _ } = ast {
            // Check for duplicate style names - but allow includes to redefine styles
            // The latest definition wins (include order matters)
            if let Some(existing_index) = state.styles.iter().position(|s| s.source_name == *name) {
                // Remove the existing style definition - the new one will replace it
                state.styles.remove(existing_index);
                log::debug!("Style '{}' redefined, using latest definition", name);
            }
            
            let style_id = (state.styles.len() + 1) as u8;
            let name_index = self.add_string_to_state(name, state)?;
            
            let mut style_entry = StyleEntry {
                id: style_id,
                source_name: name.clone(),
                name_index,
                extends_style_names: extends.clone(),
                properties: Vec::new(),
                source_properties: Vec::new(),
                calculated_size: 3, // Base size
                is_resolved: false,
                is_resolving: false,
            };
            
            // Convert AST properties to source properties
            for ast_prop in properties {
                style_entry.source_properties.push(SourceProperty {
                    key: ast_prop.key.clone(),
                    value: ast_prop.value.to_string(),
                    line_num: ast_prop.line,
                    template_variables: ast_prop.template_variables.clone(),
                });
            }
            
            state.styles.push(style_entry);
            state.header_flags |= FLAG_HAS_STYLES;
        }
        
        Ok(())
    }
    
    fn collect_component_definition(&mut self, ast: &AstNode, state: &mut CompilerState) -> Result<()> {
        if let AstNode::Component { name, properties, template, .. } = ast {
            // Check for duplicate component names - but allow includes to redefine components
            // The latest definition wins (include order matters)
            if let Some(existing_index) = state.component_defs.iter().position(|c| c.name == *name) {
                // Remove the existing component definition - the new one will replace it
                state.component_defs.remove(existing_index);
                // Also remove the old template
                state.component_ast_templates.remove(name);
                log::debug!("Component '{}' redefined, using latest definition", name);
            }
            
            let mut component_def = ComponentDefinition {
                name: name.clone(),
                properties: Vec::new(),
                definition_start_line: 0,
                definition_root_element_index: None,
                calculated_size: 0,
                internal_template_element_offsets: HashMap::new(),
            };
            
            // Convert component properties
            for comp_prop in properties {
                component_def.properties.push(ComponentPropertyDef {
                    name: comp_prop.name.clone(),
                    value_type_hint: comp_prop.value_type_hint(),
                    default_value: comp_prop.default_value.clone().unwrap_or_default(),
                });
            }
            
            // Store the template AST for the resolver to use
            state.component_ast_templates.insert(name.clone(), (**template).clone());
            
            state.component_defs.push(component_def);
            state.header_flags |= FLAG_HAS_COMPONENT_DEFS;
        }
        
        Ok(())
    }
    
    fn collect_script_definition(&mut self, _ast: &AstNode, state: &mut CompilerState) -> Result<()> {
        // Scripts are handled by the script processor
        state.header_flags |= FLAG_HAS_SCRIPTS;
        Ok(())
    }
    
    fn collect_directive_definition(&mut self, ast: &AstNode, state: &mut CompilerState) -> Result<()> {
        match ast {
            AstNode::Variables { variables } => {
                // Process @variables block
                for (name, value) in variables {
                    // Create variable definition
                    let var_def = VariableDef {
                        value: value.clone(),
                        raw_value: value.clone(),
                        def_line: 0, // Line number not available in this context
                        is_resolving: false,
                        is_resolved: true,
                    };
                    
                    // Add to state.variables for legacy compatibility
                    state.variables.insert(name.clone(), var_def.clone());
                    
                    // Also add to variable context for substitution
                    state.variable_context.add_string_variable(
                        name.clone(),
                        value.clone(),
                        "<@variables>".to_string(),
                        0
                    )?;
                }
            }
            _ => {
                // Other directive types could be handled here in the future
            }
        }
        Ok(())
    }
    
    fn resolve_dependencies(&mut self, state: &mut CompilerState) -> Result<()> {
        // Resolve style inheritance
        self.resolve_style_inheritance(state)?;
        
        // Validate component property references
        self.validate_component_properties(state)?;
        
        Ok(())
    }
    
    fn resolve_style_inheritance(&mut self, state: &mut CompilerState) -> Result<()> {
        let mut resolved = HashSet::new();
        let mut resolving = HashSet::new();
        
        // Resolve each style
        for i in 0..state.styles.len() {
            if !resolved.contains(&i) {
                self.resolve_style_recursive(i, state, &mut resolved, &mut resolving)?;
            }
        }
        
        Ok(())
    }
    
    fn resolve_style_recursive(
        &mut self,
        style_index: usize,
        state: &mut CompilerState,
        resolved: &mut HashSet<usize>,
        resolving: &mut HashSet<usize>,
    ) -> Result<()> {
        if resolved.contains(&style_index) {
            return Ok(());
        }
        
        if resolving.contains(&style_index) {
            let style_name = &state.styles[style_index].source_name;
            return Err(CompilerError::semantic_legacy(
                0,
                format!("Circular dependency in style inheritance involving '{}'", style_name)
            ));
        }
        
        resolving.insert(style_index);
        
        // Get the extends list (need to clone to avoid borrow checker issues)
        let extends_names = state.styles[style_index].extends_style_names.clone();
        
        // Resolve base styles first
        for base_style_name in &extends_names {
            if let Some(base_index) = state.styles.iter().position(|s| s.source_name == *base_style_name) {
                self.resolve_style_recursive(base_index, state, resolved, resolving)?;
            } else {
                return Err(CompilerError::semantic_legacy(
                    0,
                    format!("Style '{}' extends undefined style '{}'", 
                           state.styles[style_index].source_name, base_style_name)
                ));
            }
        }
        
        // Now resolve this style by inheriting from base styles
        let mut inherited_properties = Vec::new();
        
        for base_style_name in &extends_names {
            if let Some(base_style) = state.styles.iter().find(|s| s.source_name == *base_style_name) {
                // Inherit properties from base style
                for prop in &base_style.properties {
                    // Only add if not already overridden
                    if !inherited_properties.iter().any(|p: &KrbProperty| p.property_id == prop.property_id) {
                        inherited_properties.push(prop.clone());
                    }
                }
            }
        }
        
        // Add own properties (they override inherited ones)
        for source_prop in &state.styles[style_index].source_properties.clone() {
            if let Ok(krb_prop) = self.convert_source_property_to_krb(source_prop, state) {
                // Remove any inherited property with the same ID
                inherited_properties.retain(|p| p.property_id != krb_prop.property_id);
                inherited_properties.push(krb_prop);
            }
        }
        
        // Update the style with resolved properties
        state.styles[style_index].properties = inherited_properties;
        state.styles[style_index].is_resolved = true;
        
        resolving.remove(&style_index);
        resolved.insert(style_index);
        
        Ok(())
    }
    
    fn validate_component_properties(&mut self, state: &CompilerState) -> Result<()> {
        for component in &state.component_defs {
            for prop_def in &component.properties {
                // Validate property type
                match prop_def.value_type_hint {
                    ValueType::String | ValueType::Int | ValueType::Float | 
                    ValueType::Bool | ValueType::Color | ValueType::Resource => {
                        // Basic types are always valid
                    }
                    ValueType::StyleId => {
                        // Could validate that default value references existing style
                        if !prop_def.default_value.is_empty() {
                            let style_name = prop_def.default_value.trim_matches('"');
                            if !state.styles.iter().any(|s| s.source_name == style_name) {
                                self.warnings.push(format!(
                                    "Component '{}' property '{}' defaults to undefined style '{}'",
                                    component.name, prop_def.name, style_name
                                ));
                            }
                        }
                    }
                    ValueType::Enum => {
                        // Could validate enum values
                    }
                    _ => {}
                }
            }
        }
        
        Ok(())
    }
    
    fn validate_elements(&mut self, ast: &mut AstNode, state: &CompilerState) -> Result<()> {
        match ast {
            AstNode::File { app, components, .. } => {
                // Validate main app
                if let Some(app_node) = app {
                    self.validate_element_recursive(app_node, state, None)?;
                }
                
                // Validate component templates
                for component_node in components {
                    if let AstNode::Component { template, .. } = component_node {
                        self.validate_element_recursive(template, state, None)?;
                    }
                }
            }
            _ => {}
        }
        
        Ok(())
    }
    
    fn validate_element_recursive(
        &mut self,
        ast: &mut AstNode,
        state: &CompilerState,
        parent_type: Option<&str>
    ) -> Result<()> {
        if let AstNode::Element { element_type, properties, children, .. } = ast {
            // Validate element type
            let elem_type = ElementType::from_name(element_type);
            
            // Validate properties for this element type
            for prop in properties.iter_mut() {
                self.validate_property(element_type, prop, state)?;
            }
            
            // Special validation for Input elements with type-specific property validation
            if element_type == "Input" {
                self.validate_input_element_properties(properties)?;
            }
            
            // Validate parent-child relationships
            if let Some(parent) = parent_type {
                self.validate_parent_child_relationship(parent, element_type)?;
            }
            
            // Recursively validate children
            for child in children {
                self.validate_element_recursive(child, state, Some(element_type))?;
            }
        }
        
        Ok(())
    }
    
    fn validate_property(&mut self, element_type: &str, prop: &mut AstProperty, _state: &CompilerState) -> Result<()> {
        // Resolve property aliases first
        let resolved_key = self.resolve_property_alias(element_type, &prop.key);
        
        // If we resolved an alias, show a helpful message and update the property key
        if resolved_key != prop.key {
            self.warnings.push(format!(
                "Line {}: Property '{}' on {} element is automatically mapped to '{}' (consider updating your code)",
                prop.line, prop.key, element_type, resolved_key
            ));
            // Update the property key to use the canonical name
            prop.key = resolved_key.clone();
        }
        
        // Validate property is valid for this element type (using resolved key)
        let is_valid = match element_type {
            "App" => self.is_valid_app_property(&resolved_key),
            "Text" => self.is_valid_text_property(&resolved_key),
            "Button" => self.is_valid_button_property(&resolved_key),
            "Input" => self.is_valid_input_property(&resolved_key),
            "Image" => self.is_valid_image_property(&resolved_key),
            "Container" => self.is_valid_container_property(&resolved_key),
            _ => true, // Unknown element types accept any property
        };
        
        if !is_valid {
            return Err(CompilerError::semantic_legacy(
                prop.line,
                format!("Property '{}' is not valid for element type '{}'", resolved_key, element_type)
            ));
        }
        
        // Validate property value format
        self.validate_property_value(&prop.key, &prop.value.to_string(), prop.line)?;
        
        Ok(())
    }
    
    fn validate_property_value(&mut self, key: &str, value: &str, line: usize) -> Result<()> {
        match key {
            key if key.contains("color") => {
                let is_valid_color = value.starts_with('#') || // Hex color
                                   value.starts_with('$') || // Variable
                                   value.starts_with('"') || // String
                                   self.is_valid_color_keyword(value); // Color keyword
                
                if !is_valid_color {
                    return Err(CompilerError::semantic_legacy(
                        line,
                        format!("Color property '{}' must be a hex color (#RGB), variable ($var), string, or valid color keyword", key)
                    ));
                }
            }
            key if key.contains("width") || key.contains("height") || key.contains("size") => {
                // Allow numbers, percentages, CSS units, variables, and strings
                let is_valid = value.chars().all(|c| c.is_ascii_digit() || c == '.') || // Pure number (including decimals)
                              value.ends_with('%') || // Percentage values like 100%, 50.5%
                              value.ends_with("px") || // Pixel units like 10px, 100px
                              value.ends_with("em") || // Em units like 1.5em, 2em
                              value.ends_with("rem") || // Rem units like 2rem, 1.5rem
                              value.ends_with("vw") || // Viewport width units like 50vw
                              value.ends_with("vh") || // Viewport height units like 100vh
                              value.starts_with('$') || // Variables
                              value.starts_with('"'); // Strings
                
                if !is_valid {
                    return Err(CompilerError::semantic_legacy(
                        line,
                        format!("Size property '{}' must be a number, percentage (%), CSS unit (px/em/rem/vw/vh), variable ($var), or string", key)
                    ));
                }
            }
            _ => {}
        }
        
        Ok(())
    }
    
    fn validate_parent_child_relationship(&mut self, parent_type: &str, child_type: &str) -> Result<()> {
        // Comprehensive element nesting validation based on UI best practices
        match (parent_type, child_type) {
            // Leaf elements that cannot contain children
            ("Text", _) => {
                return Err(CompilerError::semantic_legacy(
                    0,
                    format!("Text elements cannot contain child elements, found '{}'", child_type)
                ));
            }
            ("Input", _) => {
                return Err(CompilerError::semantic_legacy(
                    0,
                    format!("Input elements cannot contain child elements, found '{}'", child_type)
                ));
            }
            ("Image", _) => {
                return Err(CompilerError::semantic_legacy(
                    0,
                    format!("Image elements cannot contain child elements, found '{}'", child_type)
                ));
            }
            
            // Button semantic validation - warn about nested interactive elements
            ("Button", "Button") => {
                return Err(CompilerError::semantic_legacy(
                    0,
                    "Buttons should not contain other buttons (accessibility issue)".to_string()
                ));
            }
            ("Button", "Input") => {
                return Err(CompilerError::semantic_legacy(
                    0,
                    "Buttons should not contain input elements (accessibility issue)".to_string()
                ));
            }
            
            // App should only contain top-level layout elements and custom elements
            ("App", child) => {
                let child_element_type = ElementType::from_name(child);
                if !matches!(child, "Container" | "Text" | "Button" | "Input" | "Image") && 
                   child_element_type != ElementType::Unknown {
                    return Err(CompilerError::semantic_legacy(
                        0,
                        format!("App element should only contain Container, Text, Button, Input, Image, or custom elements, found '{}'", child_type)
                    ));
                }
            }
            
            // Valid container relationships
            ("Container", _) => {}, // Containers can contain any element
            ("Button", "Text") => {}, // Buttons can contain text
            ("Button", "Image") => {}, // Buttons can contain images
            ("Button", "Container") => {}, // Buttons can contain layout containers
            
            // Catch unknown element types
            (parent, child) if !matches!(parent, "App" | "Container" | "Text" | "Button" | "Input" | "Image") => {
                return Err(CompilerError::semantic_legacy(
                    0,
                    format!("Unknown parent element type: '{}'", parent)
                ));
            }
            (parent, child) if !matches!(child, "App" | "Container" | "Text" | "Button" | "Input" | "Image") => {
                return Err(CompilerError::semantic_legacy(
                    0,
                    format!("Unknown child element type: '{}'", child)
                ));
            }
            
            // Catch-all for any other valid combinations
            _ => {}
        }
        
        Ok(())
    }
    
    fn check_unused_definitions(&mut self, state: &CompilerState) -> Result<()> {
        // Check for unused styles
        let used_styles: HashSet<String> = HashSet::new();

        // This would require traversing all elements to see which styles are referenced
        // For now, just warn about styles that don't extend anything and aren't extended
        for style in &state.styles {
            if style.extends_style_names.is_empty() {
                let is_extended = state.styles.iter()
                    .any(|s| s.extends_style_names.contains(&style.source_name));
                
                if !is_extended {
                    self.warnings.push(format!(
                        "Style '{}' is defined but may not be used", 
                        style.source_name
                    ));
                }
            }
        }
        
        Ok(())
    }
    
    fn convert_source_property_to_krb(&self, prop: &SourceProperty, state: &CompilerState) -> Result<KrbProperty> {
        let property_id = self.get_property_id(&prop.key);
        let cleaned_value = clean_and_quote_value(&prop.value).0;
        
        match property_id {
            PropertyId::BackgroundColor | PropertyId::ForegroundColor | PropertyId::BorderColor => {
                if let Ok(color) = parse_color(&cleaned_value) {
                    Ok(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Color,
                        size: 4,
                        value: color.to_bytes().to_vec(),
                    })
                } else {
                    let length_info = if cleaned_value.starts_with('#') {
                        format!("length: {} chars after #", cleaned_value.len() - 1)
                    } else {
                        format!("length: {} chars total", cleaned_value.len())
                    };
                    Err(CompilerError::semantic_legacy(
                        prop.line_num,
                        format!("Invalid color value: '{}' ({})", cleaned_value, length_info)
                    ))
                }
            }
            PropertyId::TextContent | PropertyId::WindowTitle => {
                // Variable substitution happens in convert_ast_property_to_krb function
                let string_index = state.strings.iter()
                    .position(|s| s.text == cleaned_value)
                    .unwrap_or(0) as u8;
                
                Ok(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::String,
                    size: 1,
                    value: vec![string_index],
                })
            }
            PropertyId::BorderWidth | PropertyId::BorderRadius => {
                if let Ok(val) = cleaned_value.parse::<u8>() {
                    Ok(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Byte,
                        size: 1,
                        value: vec![val],
                    })
                } else {
                    Err(CompilerError::semantic_legacy(
                        prop.line_num,
                        format!("Invalid numeric value: {}", cleaned_value)
                    ))
                }
            }
            PropertyId::Width | PropertyId::Height => {
                // Handle width/height properties as u16 values
                if let Ok(size) = cleaned_value.parse::<u16>() {
                    Ok(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Short,
                        size: 2,
                        value: size.to_le_bytes().to_vec(),
                    })
                } else {
                    Err(CompilerError::semantic_legacy(
                        prop.line_num,
                        format!("Invalid size value: {}", cleaned_value)
                    ))
                }
            }
            PropertyId::Visibility => {
                // Handle visible/visibility property - convert boolean or string to boolean, or template variables
                if cleaned_value.starts_with('$') {
                    // This is a template variable - store it as a string for later resolution
                    let string_index = state.strings.iter()
                        .position(|s| s.text == cleaned_value)
                        .unwrap_or(0) as u8;
                    Ok(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::TemplateVariable,
                        size: 1,
                        value: vec![string_index],
                    })
                } else {
                    let visible = match cleaned_value.to_lowercase().as_str() {
                        "true" | "visible" | "1" => true,
                        "false" | "hidden" | "0" => false,
                        _ => {
                            return Err(CompilerError::semantic_legacy(
                                prop.line_num,
                                format!("Invalid visibility value: '{}'. Use 'true', 'false', 'visible', 'hidden', or a template variable starting with '$'", cleaned_value)
                            ));
                        }
                    };
                    
                    Ok(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Byte,
                        size: 1,
                        value: vec![if visible { 1 } else { 0 }],
                    })
                }
            }
            _ => {
                // Default handling - store as custom property
                Ok(KrbProperty {
                    property_id: PropertyId::CustomData as u8,
                    value_type: ValueType::String,
                    size: cleaned_value.len() as u8,
                    value: cleaned_value.into_bytes(),
                })
            }
        }
    }
    
    fn get_property_id(&self, key: &str) -> PropertyId {
        PropertyId::from_name(key)
    }

    fn is_valid_app_property(&self, key: &str) -> bool {
        matches!(key,
            // Basic size and title properties
            "width" | "height" | "title" |
            // Window-specific properties
            "window_title" | "window_width" | "window_height" | "window_min_width" |
            "window_min_height" | "window_max_width" | "window_max_height" |
            "resizable" | "keep_aspect_ratio" | "scale_factor" | "icon" |
            "version" | "author" | "background_color" | "id" | "style" | "visible" |
            // Modern Taffy layout properties
            "display" | "flex_direction" | "flex_wrap" | "flex_grow" | "flex_shrink" | "flex_basis" |
            "align_items" | "align_self" | "align_content" | "justify_content" | "justify_items" | "justify_self" |
            "position" | "top" | "right" | "bottom" | "left" | "inset" |
            "min_size" | "max_size" | "preferred_size" | "gap" | "row_gap" | "column_gap" |
            "grid_template_columns" | "grid_template_rows" | "grid_area" | "grid_column" | "grid_row" |
            // Box model properties
            "padding" | "margin" | "border_width" | "border_color" | "border_radius" |
            "padding_top" | "padding_right" | "padding_bottom" | "padding_left" |
            "margin_top" | "margin_right" | "margin_bottom" | "margin_left"
        )
    }
    
    fn is_valid_text_property(&self, key: &str) -> bool {
        matches!(key,
            "text" | "text_color" | "font_size" | "font_weight" | "font_family" |
            "text_alignment" | "line_height" | "text_decoration" | "text_transform" |
            "list_style_type" | "white_space" |
            "id" | "pos_x" | "pos_y" | "x" | "y" | "position_x" | "position_y" | "left" | "top" | "right" | "bottom" | "width" | "height" | "style" |
            "background_color" | "border_color" | "border_width" | "border_radius" |
            "padding" | "margin" | "opacity" | "visibility" | "visible" | "z_index" |
            // Transform properties
            "transform" |
            // Event handlers
            "onClick" | "onHover" | "onFocus" | "onBlur" |
            // Modern Taffy layout properties
            "display" | "flex_direction" | "flex_wrap" | "flex_grow" | "flex_shrink" | "flex_basis" |
            "align_items" | "align_self" | "align_content" | "justify_content" | "justify_items" | "justify_self" |
            "order" | "position" | "top" | "right" | "bottom" | "left" | "inset" |
            // Typography properties
            "letter_spacing" | "word_spacing" | "text_indent" | "text_overflow" | "font_style" | "font_variant" |
            // Overflow properties
            "overflow" | "overflow_x" | "overflow_y" |
            // Box model properties
            "padding_top" | "padding_right" | "padding_bottom" | "padding_left" |
            "margin_top" | "margin_right" | "margin_bottom" | "margin_left" |
            "border_top_width" | "border_right_width" | "border_bottom_width" | "border_left_width" |
            "border_top_color" | "border_right_color" | "border_bottom_color" | "border_left_color" |
            "border_top_left_radius" | "border_top_right_radius" | "border_bottom_right_radius" | "border_bottom_left_radius" |
            "box_sizing" | "outline" | "outline_color" | "outline_width" | "outline_offset"
        )
    }
    
    fn is_valid_button_property(&self, key: &str) -> bool {
        self.is_valid_text_property(key) || matches!(key,
            "disabled" | "onPress" | "onRelease" | "cursor" | "checked" |
            // Box model properties
            "padding_top" | "padding_right" | "padding_bottom" | "padding_left" |
            "margin_top" | "margin_right" | "margin_bottom" | "margin_left" |
            "border_top_width" | "border_right_width" | "border_bottom_width" | "border_left_width" |
            "border_top_color" | "border_right_color" | "border_bottom_color" | "border_left_color" |
            "border_top_left_radius" | "border_top_right_radius" | "border_bottom_right_radius" | "border_bottom_left_radius" |
            "box_sizing" | "outline" | "outline_color" | "outline_width" | "outline_offset"
        )
    }
    
    fn is_valid_input_property(&self, key: &str) -> bool {
        // For the basic property validation, allow all potential input properties
        // Type-specific validation is handled separately in validate_input_element_properties
        self.is_valid_text_property(key) || matches!(key,
            // Core input properties
            "type" | "value" | "placeholder" | "disabled" | "readonly" | "required" |
            
            // Event handlers
            "onChange" | "onSubmit" | "onFocus" | "onBlur" | "onClick" |
            
            // Textual input properties
            "max_length" | "min_length" | "pattern" |
            
            // Selection input properties  
            "checked" | "name" | "text" |
            
            // Range/number input properties
            "min" | "max" | "step" |
            
            // File input properties
            "accept" | "multiple" |
            
            // Image input properties
            "src" | "alt" |
            
            // Font and text styling properties for all inputs
            "font_size" | "font_weight" | "font_family" | "text_color" | "color"
        )
    }
    
    /// Comprehensive validation for Input elements based on their type attribute
    fn validate_input_element_properties(&self, properties: &[AstProperty]) -> Result<()> {
        // Find the type property to determine input type
        let input_type = self.get_input_type_from_properties(properties);
        
        // Validate each property against the determined input type
        for prop in properties {
            if !self.is_property_valid_for_input_type(&prop.key, input_type) {
                return Err(CompilerError::semantic_legacy(
                    prop.line,
                    format!(
                        "Property '{}' is not valid for Input type '{}'. Valid properties for this type are: {}",
                        prop.key,
                        input_type.to_name(),
                        self.get_valid_properties_for_input_type(input_type).join(", ")
                    )
                ));
            }
        }
        
        Ok(())
    }
    
    /// Extract the input type from the element's properties, defaulting to "text"
    fn get_input_type_from_properties(&self, properties: &[AstProperty]) -> InputType {
        for prop in properties {
            if prop.key == "type" {
                // Handle both String and Expression variants
                match &prop.value {
                    PropertyValue::String(_) | PropertyValue::Expression(_) => {
                        // Use cleaned_value to remove quotes and handle expressions
                        let cleaned_type = prop.cleaned_value();
                        if let Some(input_type) = InputType::from_name(&cleaned_type) {
                            return input_type;
                        }
                    }
                    _ => {}
                }
            }
        }
        InputType::default() // Default to "text"
    }
    
    /// Check if a property is valid for a specific input type
    fn is_property_valid_for_input_type(&self, property: &str, input_type: InputType) -> bool {
        // Common properties valid for all input types
        let is_common_property = matches!(property,
            "type" | "id" | "style" | "disabled" | "visible" | "width" | "height" |
            "padding" | "margin" | "background_color" | "border_color" | "border_width" |
            "border_radius" | "opacity" | "z_index" | "pos_x" | "pos_y" | "x" | "y" | "position_x" | "position_y" | "left" | "top" | "right" | "bottom" |
            "onClick" | "onFocus" | "onBlur" | "onHover" | "onPress" | "onRelease" |
            // Box model properties
            "padding_top" | "padding_right" | "padding_bottom" | "padding_left" |
            "margin_top" | "margin_right" | "margin_bottom" | "margin_left" |
            "border_top_width" | "border_right_width" | "border_bottom_width" | "border_left_width" |
            "border_top_color" | "border_right_color" | "border_bottom_color" | "border_left_color" |
            "border_top_left_radius" | "border_top_right_radius" | "border_bottom_right_radius" | "border_bottom_left_radius" |
            "box_sizing" | "outline" | "outline_color" | "outline_width" | "outline_offset" |
            // Layout properties
            "display" | "flex_direction" | "flex_wrap" | "flex_grow" | "flex_shrink" | "flex_basis" |
            "align_items" | "align_self" | "align_content" | "justify_content" | "justify_items" | "justify_self" |
            "position" | "top" | "right" | "bottom" | "left" | "inset"
        );
        
        if is_common_property {
            return true;
        }
        
        // Type-specific property validation
        match input_type {
            InputType::Text | InputType::Password | InputType::Email | 
            InputType::Number | InputType::Tel | InputType::Url | InputType::Search => {
                matches!(property,
                    "value" | "placeholder" | "max_length" | "min_length" | "readonly" | 
                    "pattern" | "required" | "onChange" | "onSubmit" |
                    // Text styling properties
                    "font_size" | "font_weight" | "font_family" | "text_color" | "color" |
                    "text_alignment" | "line_height" | "letter_spacing" | "text_decoration" |
                    "text_transform" | "text_indent" | "font_style" | "font_variant" | "word_spacing"
                ) || (input_type == InputType::Number && matches!(property, "min" | "max" | "step"))
            }
            
            InputType::Checkbox | InputType::Radio => {
                matches!(property,
                    "checked" | "value" | "name" | "text" | "onChange" |
                    "font_size" | "font_weight" | "font_family" | "text_color" | "color"
                )
            }
            
            InputType::Range => {
                matches!(property,
                    "value" | "min" | "max" | "step" | "onChange"
                )
            }
            
            InputType::Date | InputType::DatetimeLocal | InputType::Month | 
            InputType::Time | InputType::Week => {
                matches!(property,
                    "value" | "min" | "max" | "step" | "readonly" | "onChange" |
                    "font_size" | "font_weight" | "font_family" | "text_color" | "color"
                )
            }
            
            InputType::Color => {
                matches!(property,
                    "value" | "onChange"
                )
            }
            
            InputType::File => {
                matches!(property,
                    "accept" | "multiple" | "onChange"
                )
            }
            
            InputType::Hidden => {
                matches!(property, "value")
            }
            
            InputType::Submit | InputType::Reset | InputType::Button => {
                matches!(property,
                    "value" | "onClick"
                )
            }
            
            InputType::Image => {
                matches!(property,
                    "src" | "alt" | "value" | "onClick"
                )
            }
        }
    }
    
    /// Get list of valid properties for an input type (for error messages)
    fn get_valid_properties_for_input_type(&self, input_type: InputType) -> Vec<&'static str> {
        let mut props = vec![
            "type", "id", "style", "disabled", "visible", "width", "height",
            "padding", "margin", "background_color", "border_color", "border_width",
            "border_radius", "opacity", "z_index",
            "onClick", "onFocus", "onBlur", "onHover"
        ];
        
        match input_type {
            InputType::Text | InputType::Password | InputType::Email | 
            InputType::Tel | InputType::Url | InputType::Search => {
                props.extend(&["value", "placeholder", "max_length", "min_length", "readonly", "pattern", "onChange", "onSubmit",
                    "font_size", "font_weight", "font_family", "text_color", "text_alignment", 
                    "line_height", "letter_spacing", "text_decoration", "text_transform", 
                    "text_indent", "font_style", "font_variant", "word_spacing"]);
            }
            
            InputType::Number => {
                props.extend(&["value", "placeholder", "min", "max", "step", "readonly", "onChange", "onSubmit",
                    "font_size", "font_weight", "font_family", "text_color", "text_alignment", 
                    "line_height", "letter_spacing", "text_decoration", "text_transform", 
                    "text_indent", "font_style", "font_variant", "word_spacing"]);
            }
            
            InputType::Checkbox | InputType::Radio => {
                props.extend(&["checked", "value", "name", "text", "onChange"]);
            }
            
            InputType::Range => {
                props.extend(&["value", "min", "max", "step", "onChange"]);
            }
            
            InputType::Date | InputType::DatetimeLocal | InputType::Month | 
            InputType::Time | InputType::Week => {
                props.extend(&["value", "min", "max", "step", "readonly", "onChange"]);
            }
            
            InputType::Color => {
                props.extend(&["value", "onChange"]);
            }
            
            InputType::File => {
                props.extend(&["accept", "multiple", "onChange"]);
            }
            
            InputType::Hidden => {
                props.extend(&["value"]);
            }
            
            InputType::Submit | InputType::Reset | InputType::Button => {
                props.extend(&["value", "onClick"]);
            }
            
            InputType::Image => {
                props.extend(&["src", "alt", "value", "onClick"]);
            }
        }
        
        props
    }
    

    fn is_valid_image_property(&self, key: &str) -> bool {
        matches!(key,
            "src" | "alt" | "fit" | "id" | "pos_x" | "pos_y" | "width" | "height" |
            "style" | "background_color" | "border_color" | "border_width" |
            "border_radius" | "padding" | "margin" | "opacity" | "visibility" | "visible" | "z_index" |
            // Transform properties
            "transform" |
            // Modern Taffy layout properties
            "display" | "flex_direction" | "flex_wrap" | "flex_grow" | "flex_shrink" | "flex_basis" |
            "align_items" | "align_self" | "align_content" | "justify_content" | "justify_items" | "justify_self" |
            "position" | "top" | "right" | "bottom" | "left" | "inset" |
            // Box model properties
            "padding_top" | "padding_right" | "padding_bottom" | "padding_left" |
            "margin_top" | "margin_right" | "margin_bottom" | "margin_left" |
            "border_top_width" | "border_right_width" | "border_bottom_width" | "border_left_width" |
            "border_top_color" | "border_right_color" | "border_bottom_color" | "border_left_color" |
            "border_top_left_radius" | "border_top_right_radius" | "border_bottom_right_radius" | "border_bottom_left_radius" |
            "box_sizing" | "outline" | "outline_color" | "outline_width" | "outline_offset"
        )
    }
    
    fn is_valid_container_property(&self, key: &str) -> bool {
        matches!(key,
            "gap" | "id" | "pos_x" | "pos_y" | "x" | "y" | "position_x" | "position_y" | "left" | "top" | "right" | "bottom" | "width" | "height" |
            "min_width" | "min_height" | "max_width" | "max_height" |
            "style" | "background_color" | "border_color" | "border_width" |
            "border_radius" | "padding" | "margin" | "opacity" | "visibility" | "visible" | "z_index" |
            "list_style_type" |
            // Transform properties
            "transform" |
            // Modern Taffy layout properties
            "display" | "flex_direction" | "flex_wrap" | "flex_grow" | "flex_shrink" | "flex_basis" |
            "align_items" | "align_self" | "align_content" | "justify_content" | "justify_items" | "justify_self" |
            "order" | "position" | "inset" |
            "grid_template_columns" | "grid_template_rows" | "grid_area" | "grid_column" | "grid_row" |
            // Overflow properties
            "overflow" | "overflow_x" | "overflow_y" |
            // Box model properties
            "padding_top" | "padding_right" | "padding_bottom" | "padding_left" |
            "margin_top" | "margin_right" | "margin_bottom" | "margin_left" |
            "border_top_width" | "border_right_width" | "border_bottom_width" | "border_left_width" |
            "border_top_color" | "border_right_color" | "border_bottom_color" | "border_left_color" |
            "border_top_left_radius" | "border_top_right_radius" | "border_bottom_right_radius" | "border_bottom_left_radius" |
            "box_sizing" | "outline" | "outline_color" | "outline_width" | "outline_offset"
        )
    }
    
    /// Resolve property aliases to canonical property names
    fn resolve_property_alias(&self, element_type: &str, property: &str) -> String {
        match (element_type, property) {
            // Text element aliases
            ("Text", "color") => "text_color".to_string(),
            ("Text", "font") => "font_family".to_string(),
            ("Text", "size") => "font_size".to_string(),
            ("Text", "align") => "text_alignment".to_string(),
            
            // Button element aliases (inherits from Text)
            ("Button", "color") => "text_color".to_string(),
            ("Button", "font") => "font_family".to_string(),
            ("Button", "size") => "font_size".to_string(),
            ("Button", "align") => "text_alignment".to_string(),
            
            // Container element aliases
            ("Container", "x") => "pos_x".to_string(),
            ("Container", "y") => "pos_y".to_string(),
            ("Container", "bg") => "background_color".to_string(),
            ("Container", "bg_color") => "background_color".to_string(),
            ("Container", "border") => "border_width".to_string(),
            
            // Image element aliases
            ("Image", "x") => "pos_x".to_string(),
            ("Image", "y") => "pos_y".to_string(),
            ("Image", "url") => "src".to_string(),
            
            // App element aliases  
            ("App", "title") => "window_title".to_string(),
            ("App", "w") => "window_width".to_string(),
            ("App", "h") => "window_height".to_string(),
            
            // Universal aliases for all elements
            (_, "x") => "pos_x".to_string(),
            (_, "y") => "pos_y".to_string(),
            (_, "w") => "width".to_string(),
            (_, "h") => "height".to_string(),
            (_, "bg") => "background_color".to_string(),
            (_, "bg_color") => "background_color".to_string(),
            (_, "border") => "border_width".to_string(),
            (_, "opacity") => "opacity".to_string(),
            (_, "visible") => "visibility".to_string(),
            
            // No alias found, return original property
            _ => property.to_string(),
        }
    }
    
    fn add_string_to_state(&self, text: &str, state: &mut CompilerState) -> Result<u8> {
        // Check if string already exists
        for (i, entry) in state.strings.iter().enumerate() {
            if entry.text == text {
                return Ok(i as u8);
            }
        }
        
        // Add new string
        if state.strings.len() >= MAX_STRINGS {
            return Err(CompilerError::LimitExceeded {
                limit_type: "strings".to_string(),
                limit: MAX_STRINGS,
            });
        }
        
        let index = state.strings.len() as u8;
        state.strings.push(StringEntry {
            text: text.to_string(),
            length: text.len(),
            index,
        });
        
        Ok(index)
    }
    
    /// Check if a string is a valid CSS color keyword
    fn is_valid_color_keyword(&self, value: &str) -> bool {
        match value.trim().to_lowercase().as_str() {
            "transparent" => true,
            // Can add more color keywords here in the future (red, blue, green, etc.)
            _ => false,
        }
    }
}

/// Guess resource type from property key
fn guess_resource_type(key: &str) -> ResourceType {    
    let lower_key = key.to_lowercase();
    
    if lower_key.contains("image") || lower_key.contains("icon") || 
       lower_key.contains("sprite") || lower_key.contains("texture") ||
       lower_key.contains("background") || lower_key.contains("logo") ||
       lower_key.contains("avatar") {
        ResourceType::Image
    } else if lower_key.contains("font") {
        ResourceType::Font
    } else if lower_key.contains("sound") || lower_key.contains("audio") ||
              lower_key.contains("music") {
        ResourceType::Sound
    } else if lower_key.contains("video") {
        ResourceType::Video
    } else {
        ResourceType::Image // Default
    }
}

pub fn convert_ast_to_state(ast: &AstNode, state: &mut CompilerState) -> Result<()> {
    match ast {
        AstNode::File { app, styles, fonts, components, scripts, directives } => {
            // Process styles first since elements may reference them
            for style_node in styles {
                if let AstNode::Style { name, extends: _, properties, pseudo_selectors } = style_node {
                    // Convert style properties to KRB format first
                    let mut krb_properties = Vec::new();
                    for ast_prop in properties {
                        // Expand shorthand properties
                        let expanded_props = expand_shorthand_property(ast_prop)?;
                        for expanded_prop in expanded_props {
                            if let Some(krb_prop) = convert_ast_property_to_krb(&expanded_prop, state)? {
                                krb_properties.push(krb_prop);
                            }
                        }
                    }
                    
                    // Process pseudo-selectors and convert to state property sets
                    // This is a simplified implementation for now
                    if !pseudo_selectors.is_empty() {
                        println!("Style '{}' has {} pseudo-selectors (processed)", name, pseudo_selectors.len());
                        // Note: Full state property implementation requires substantial changes to
                        // the style system to properly store and apply state-based properties.
                        // For now, the renderer will need to use the existing compute_with_state method.
                    }
                    
                    // Find existing style by name (created in semantic analysis phase)
                    if let Some(existing_style) = state.styles.iter_mut().find(|s| s.source_name == *name) {
                        // Update existing style with KRB properties instead of creating duplicate
                        existing_style.properties = krb_properties;
                        existing_style.is_resolved = true;
                        existing_style.is_resolving = false;
                        
                        println!("Updated existing style '{}' with ID {} and {} properties", name, existing_style.id, existing_style.properties.len());
                    } else {
                        // Style not found - create new one (fallback case)
                        let style_id = (state.styles.len() + 1) as u8; // 1-based style IDs
                        
                        // Add style name to string table
                        let name_index = state.add_string(name.clone())?;
                        
                        // Create style entry (krb_properties already computed above)
                        let style_entry = StyleEntry {
                            id: style_id,
                            source_name: name.clone(),
                            name_index,
                            extends_style_names: Vec::new(), // TODO: implement extends
                            properties: krb_properties,
                            source_properties: properties.iter().map(|p| SourceProperty {
                                key: p.key.clone(),
                                value: p.value.to_string(),
                                line_num: p.line,
                                template_variables: p.template_variables.clone(),
                            }).collect(),
                            calculated_size: 0, // Will be calculated later
                            is_resolved: true,
                            is_resolving: false,
                        };
                        
                        println!("Added new style '{}' with ID {} and {} properties", name, style_id, style_entry.properties.len());
                        state.styles.push(style_entry);
                    }
                }
            }
            
            // Process fonts
            for font_node in fonts {
                if let AstNode::Font { name, path } = font_node {
                    // Add font name to string table
                    let name_index = state.add_string(name.clone())?;
                    
                    // Add font path to string table
                    let path_index = state.add_string(path.clone())?;
                    
                    // Create font entry
                    let font_entry = FontEntry {
                        name: name.clone(),
                        path: path.clone(),
                        name_index,
                        path_index,
                    };
                    
                    println!("Added font '{}' with path '{}'", name, path);
                    state.fonts.push(font_entry);
                }
            }
            
            // Process components
            for component_node in components {
                if let AstNode::Component { name, properties,  .. } = component_node {
                    let mut component_def = ComponentDefinition {
                        name: name.clone(),
                        properties: Vec::new(),
                        definition_start_line: 1, // TODO: get actual line from AST
                        definition_root_element_index: None,
                        calculated_size: 0,
                        internal_template_element_offsets: std::collections::HashMap::new(),
                    };
                    
                    // Process component properties
                    for comp_prop in properties {
                        if let ComponentProperty { name: prop_name, property_type, default_value, .. } = comp_prop {
                            let prop_def = ComponentPropertyDef {
                                name: prop_name.clone(),
                                value_type_hint: ValueType::String, // TODO: parse property_type
                                default_value: default_value.clone().unwrap_or_default(),
                            };
                            component_def.properties.push(prop_def);
                            
                            // If this is a private property (starts with _), add it as a template variable
                            if prop_name.starts_with('_') {
                                let name_index = state.add_string(&prop_name)?;
                                let default_value_str = default_value.clone().unwrap_or_default();
                                let default_value_index = state.add_string(&default_value_str)?;
                                
                                let template_var = crate::core::state::TemplateVariable {
                                    name: prop_name.clone(),
                                    name_index,
                                    value_type: ValueType::String, // TODO: infer from property_type
                                    default_value: default_value_str,
                                    default_value_index,
                                };
                                
                                state.template_variables.push(template_var);
                                println!("Added template variable: {} = {}", prop_name, default_value.clone().unwrap_or_default());
                            }
                        }
                    }
                    
                    // Store component definition without converting template to state yet
                    // Template will be processed during component instantiation
                    component_def.definition_root_element_index = None; // Not needed with AST templates
                    
                    state.component_defs.push(component_def);
                }
            }
            
            // Process scripts
            for script_node in scripts {
                if let AstNode::Script {    .. } = script_node {
                    let script_processor = ScriptProcessor::new();
                    let script_entry = script_processor.process_script(script_node, state)?;
                    state.scripts.push(script_entry);
                }
            }
            
            // Process app element
            if let Some(app_node) = app {
                convert_element_to_state(app_node, state, None)?;
            }
        }
        _ => return Err(CompilerError::semantic_legacy(0, "Expected File node at root")),
    }
    
    Ok(())
}
/// Parse a function call string to extract function name and parameters
fn parse_function_call(func_call: &str) -> Result<(String, Vec<String>)> {
    let func_call = func_call.trim();
    
    // Check if this is a function call with parameters
    if let Some(paren_pos) = func_call.find('(') {
        let func_name = func_call[..paren_pos].trim().to_string();
        
        // Check if there's a closing parenthesis
        if let Some(close_paren) = func_call.rfind(')') {
            let params_str = &func_call[paren_pos + 1..close_paren];
            
            // Parse parameters (simple comma-separated for now)
            let params: Vec<String> = if params_str.trim().is_empty() {
                Vec::new()
            } else {
                params_str.split(',')
                    .map(|p| p.trim().to_string())
                    .collect()
            };
            
            Ok((func_name, params))
        } else {
            Err(CompilerError::semantic_legacy(
                0,
                format!("Invalid function call syntax: missing closing parenthesis in '{}'", func_call)
            ))
        }
    } else {
        // Simple function name without parameters
        Ok((func_call.to_string(), Vec::new()))
    }
}

fn convert_element_to_state(
    ast_element: &AstNode, 
    state: &mut CompilerState, 
    parent_index: Option<usize>
) -> Result<usize> {
    if let AstNode::Element { element_type, properties, children, pseudo_selectors } = ast_element {
        let element_index = state.elements.len();
        
        let mut element = Element {
            element_type: ElementType::from_name(element_type),
            id_string_index: 0, pos_x: 0, pos_y: 0, width: 0, height: 0, layout: 0, style_id: 0, checked: false,
            property_count: 0, child_count: 0, event_count: 0, animation_count: 0, custom_prop_count: 0, state_prop_count: 0,
            krb_properties: Vec::new(), krb_custom_properties: Vec::new(), krb_events: Vec::new(),
            state_property_sets: Vec::new(), children: Vec::new(), parent_index, self_index: element_index,
            is_component_instance: false, component_def: None, is_definition_root: false,
            source_element_name: element_type.clone(), source_id_name: String::new(), source_properties: Vec::new(),
            source_children_indices: Vec::new(), source_line_num: 0, layout_flags_source: 0,
            position_hint: String::new(), orientation_hint: String::new(), calculated_size: KRB_ELEMENT_HEADER_SIZE as u32,
            absolute_offset: 0, processed_in_pass: false,
        };
        
        for ast_prop in properties {
            // Always add to source properties for template processing
            element.source_properties.push(SourceProperty {
                key: ast_prop.key.clone(),
                value: ast_prop.value.to_string(),
                line_num: ast_prop.line,
                template_variables: ast_prop.template_variables.clone(),
            });
            
            match ast_prop.key.as_str() {
                // Handle element header fields directly (only truly element-specific properties)
                "window_width" => if let Ok(val) = ast_prop.cleaned_value().parse::<u16>() { element.width = val; },
                "window_height" => if let Ok(val) = ast_prop.cleaned_value().parse::<u16>() { element.height = val; },
                "pos_x" => if let Ok(val) = ast_prop.cleaned_value().parse::<u16>() { element.pos_x = val; },
                "pos_y" => if let Ok(val) = ast_prop.cleaned_value().parse::<u16>() { element.pos_y = val; },
                
                // Position aliases - direct mapping to pos_x/pos_y
                "x" => if let Ok(val) = ast_prop.cleaned_value().parse::<u16>() { element.pos_x = val; },
                "y" => if let Ok(val) = ast_prop.cleaned_value().parse::<u16>() { element.pos_y = val; },
                "position_x" => if let Ok(val) = ast_prop.cleaned_value().parse::<u16>() { element.pos_x = val; },
                "position_y" => if let Ok(val) = ast_prop.cleaned_value().parse::<u16>() { element.pos_y = val; },
                "left" => if let Ok(val) = ast_prop.cleaned_value().parse::<u16>() { element.pos_x = val; },
                "top" => if let Ok(val) = ast_prop.cleaned_value().parse::<u16>() { element.pos_y = val; },
                
                // CSS-style positioning that requires formula conversion
                "right" => {
                    // right = container_width - element_width - right_value
                    // We'll create a template binding: pos_x = ${parent.width} - ${width} - right_value
                    if ast_prop.cleaned_value().contains('$') {
                        // Contains template variables, add as template binding
                        let formula = format!("${{parent.width}} - ${{width}} - {}", ast_prop.cleaned_value());
                        let expression_index = state.add_string(formula.clone()).unwrap_or(0);
                        state.template_bindings.push(TemplateBinding {
                            element_index: state.elements.len() as u16,
                            property_id: 0x55, // Custom property ID for pos_x via formula
                            template_expression: formula,
                            template_expression_index: expression_index,
                            variable_count: 0, // Will be populated during template processing
                            variable_indices: vec![], // Will be populated during template processing
                        });
                    } else if let Ok(right_val) = ast_prop.cleaned_value().parse::<u16>() {
                        // Static value, we'll handle this in the renderer
                        element.krb_custom_properties.push(KrbCustomProperty {
                            key_index: state.add_string("right".to_string()).unwrap_or(0),
                            value_type: ValueType::Short,
                            size: 2,
                            value: right_val.to_le_bytes().to_vec(),
                        });
                    }
                },
                "bottom" => {
                    // bottom = container_height - element_height - bottom_value  
                    // We'll create a template binding: pos_y = ${parent.height} - ${height} - bottom_value
                    if ast_prop.cleaned_value().contains('$') {
                        // Contains template variables, add as template binding
                        let formula = format!("${{parent.height}} - ${{height}} - {}", ast_prop.cleaned_value());
                        let expression_index = state.add_string(formula.clone()).unwrap_or(0);
                        state.template_bindings.push(TemplateBinding {
                            element_index: state.elements.len() as u16,
                            property_id: 0x56, // Custom property ID for pos_y via formula
                            template_expression: formula,
                            template_expression_index: expression_index,
                            variable_count: 0, // Will be populated during template processing
                            variable_indices: vec![], // Will be populated during template processing
                        });
                    } else if let Ok(bottom_val) = ast_prop.cleaned_value().parse::<u16>() {
                        // Static value, we'll handle this in the renderer
                        element.krb_custom_properties.push(KrbCustomProperty {
                            key_index: state.add_string("bottom".to_string()).unwrap_or(0),
                            value_type: ValueType::Short,
                            size: 2,
                            value: bottom_val.to_le_bytes().to_vec(),
                        });
                    }
                },
                "id" => {
                    // Store the element ID string in the string table and set the index
                    let id_string = ast_prop.cleaned_value();
                    let string_index = if let Some(entry) = state.strings.iter().position(|s| s.text == id_string) {
                        entry as u8
                    } else {
                        state.add_string(id_string.to_string())?
                    };
                    element.id_string_index = string_index;
                    println!("Set element ID '{}' to string index {}", id_string, string_index);
                },
                "style" => {
                    let style_name = ast_prop.cleaned_value();
                    if let Some(style_entry) = state.styles.iter().find(|s| s.source_name == style_name) {
                        element.style_id = style_entry.id;
                    }
                },
                "type" => {
                    // Handle type property based on element type
                    match element.element_type {
                        ElementType::Input => {
                            // For Input elements, use InputType property
                            let property_id = PropertyId::InputType;
                            let cleaned_value = ast_prop.cleaned_value();
                            let input_type_value = if let Some(input_type) = InputType::from_name(&cleaned_value) {
                                input_type as u8
                            } else {
                                return Err(CompilerError::semantic_legacy(
                                    ast_prop.line, 
                                    format!("Invalid input type: '{}'. Valid types are: text, password, email, number, tel, url, search, checkbox, radio, range, date, datetime-local, month, time, week, color, file, hidden, submit, reset, button, image", cleaned_value)
                                ));
                            };
                            element.krb_properties.push(KrbProperty { 
                                property_id: property_id as u8, 
                                value_type: ValueType::Enum, 
                                size: 1, 
                                value: vec![input_type_value] 
                            });
                        },
                        ElementType::EmbedView => {
                            // For EmbedView elements, use EmbedViewType property
                            let property_id = PropertyId::EmbedViewType;
                            let cleaned_value = ast_prop.cleaned_value();
                            let embed_type_value = if let Some(embed_type) = EmbedViewType::from_name(&cleaned_value) {
                                embed_type as u8
                            } else {
                                return Err(CompilerError::semantic_legacy(
                                    ast_prop.line, 
                                    format!("Invalid EmbedView type: '{}'. Valid types are: webview (or web), native_renderer (or native), wasm_view (or wasm), uxn_view (or uxn), gba_view (or gba), dos_view (or dos), code_editor (or editor), terminal (or term), model_viewer (or model)", cleaned_value)
                                ));
                            };
                            element.krb_properties.push(KrbProperty { 
                                property_id: property_id as u8, 
                                value_type: ValueType::Enum, 
                                size: 1, 
                                value: vec![embed_type_value] 
                            });
                        },
                        _ => {
                            // For other element types, fall through to default property processing
                            let expanded_props = expand_shorthand_property(ast_prop)?;
                            for expanded_prop in expanded_props {
                                if let Some(krb_prop) = convert_ast_property_to_krb(&expanded_prop, state)? {
                                    element.krb_properties.push(krb_prop);
                                }
                            }
                        }
                    }
                },
                "checked" => {
                    let checked_value = ast_prop.cleaned_value();
                    element.checked = checked_value == "true";
                    println!("Set element checked state to {}", element.checked);
                },

                // --- THIS IS THE CORRECTED LOGIC ---
                // Each event is handled in its own, isolated block with explicit types.
                "onClick" => {
                    let func_call = ast_prop.cleaned_value();
                    
                    // Parse function call to extract function name and parameters
                    let (func_name, params) = parse_function_call(&func_call)?;
                    
                    // Store the function call with parameters for runtime processing
                    let callback_id = state.add_string(&func_call)? as u8;
                    element.krb_events.push(KrbEvent { event_type: EVENT_TYPE_CLICK, callback_id });
                },
                "onPress" => {
                    let func_name = ast_prop.cleaned_value();
                    let callback_id = state.add_string(&func_name)? as u8;
                    element.krb_events.push(KrbEvent { event_type: EVENT_TYPE_PRESS, callback_id });
                },
                "onRelease" => {
                    let func_name = ast_prop.cleaned_value();
                    let callback_id = state.add_string(&func_name)? as u8;
                    element.krb_events.push(KrbEvent { event_type: EVENT_TYPE_RELEASE, callback_id });
                },
                "onHover" => {
                    let func_name = ast_prop.cleaned_value();
                    let callback_id = state.add_string(&func_name)? as u8;
                    element.krb_events.push(KrbEvent { event_type: EVENT_TYPE_HOVER, callback_id });
                },
                "onFocus" => {
                    let func_name = ast_prop.cleaned_value();
                    let callback_id = state.add_string(&func_name)? as u8;
                    element.krb_events.push(KrbEvent { event_type: EVENT_TYPE_FOCUS, callback_id });
                },
                "onBlur" => {
                    let func_name = ast_prop.cleaned_value();
                    let callback_id = state.add_string(&func_name)? as u8;
                    element.krb_events.push(KrbEvent { event_type: EVENT_TYPE_BLUR, callback_id });
                },
                "onChange" => {
                    let func_name = ast_prop.cleaned_value();
                    let callback_id = state.add_string(&func_name)? as u8;
                    element.krb_events.push(KrbEvent { event_type: EVENT_TYPE_CHANGE, callback_id });
                },
                "onSubmit" => {
                    let func_name = ast_prop.cleaned_value();
                    let callback_id = state.add_string(&func_name)? as u8;
                    element.krb_events.push(KrbEvent { event_type: EVENT_TYPE_SUBMIT, callback_id });
                },

                // Default case for all other standard properties
                _ => {
                    // Expand shorthand properties
                    let expanded_props = expand_shorthand_property(ast_prop)?;
                    for expanded_prop in expanded_props {
                        if let Some(krb_prop) = convert_ast_property_to_krb(&expanded_prop, state)? {
                            element.krb_properties.push(krb_prop);
                        }
                    }
                }
            }
        }
        
        // Process pseudo-selectors (e.g., &:hover) for state-based properties
        for pseudo in pseudo_selectors {
            if let Some(state_flag) = pseudo.state_flag() {
                let mut state_props = Vec::new();
                for ast_prop in &pseudo.properties {
                    // Expand shorthand properties
                    let expanded_props = expand_shorthand_property(ast_prop)?;
                    for expanded_prop in expanded_props {
                        if let Some(krb_prop) = convert_ast_property_to_krb(&expanded_prop, state)? {
                            state_props.push(krb_prop);
                        }
                    }
                }
                element.state_property_sets.push(StatePropertySet {
                    state_flags: state_flag,
                    property_count: state_props.len() as u8,
                    properties: state_props,
                });
            }
        }
        
        // Finalize counts in the element header before adding it to the state
        element.property_count = element.krb_properties.len() as u8;
        element.state_prop_count = element.state_property_sets.len() as u8;
        element.event_count = element.krb_events.len() as u8;
        
        state.elements.push(element);
        
        // Now, recursively process all child elements
        let mut child_indices = Vec::new();
        for child in children {
            let child_index = convert_element_to_state(child, state, Some(element_index))?;
            child_indices.push(child_index);
        }
        
        // Update the element in the state with its new child references
        state.elements[element_index].children = child_indices;
        state.elements[element_index].child_count = state.elements[element_index].children.len() as u8;

        Ok(element_index)
    } else {
        Err(CompilerError::semantic_legacy(0, "Expected Element node during AST conversion"))
    }
}


fn parse_calc_expression(expr: &str) -> Result<f32> {
    // Simple calc() parser for basic expressions like:
    // calc(100% - 20px), calc(50vw + 10px), calc(1.5em * 2)
    let expr = expr.trim();
    
    // Remove calc() wrapper
    if expr.starts_with("calc(") && expr.ends_with(")") {
        let inner = &expr[5..expr.len()-1].trim();
        
        // For now, support simple binary operations
        if let Some(pos) = inner.find(" + ") {
            let left = &inner[..pos].trim();
            let right = &inner[pos + 3..].trim();
            return Ok(parse_calc_value(left)? + parse_calc_value(right)?);
        } else if let Some(pos) = inner.find(" - ") {
            let left = &inner[..pos].trim();
            let right = &inner[pos + 3..].trim();
            return Ok(parse_calc_value(left)? - parse_calc_value(right)?);
        } else if let Some(pos) = inner.find(" * ") {
            let left = &inner[..pos].trim();
            let right = &inner[pos + 3..].trim();
            return Ok(parse_calc_value(left)? * parse_calc_value(right)?);
        } else if let Some(pos) = inner.find(" / ") {
            let left = &inner[..pos].trim();
            let right = &inner[pos + 3..].trim();
            let right_val = parse_calc_value(right)?;
            if right_val != 0.0 {
                return Ok(parse_calc_value(left)? / right_val);
            }
        }
        
        // Single value
        return parse_calc_value(inner);
    }
    
    Err(CompilerError::semantic_legacy(0, format!("Invalid calc() expression: {}", expr)))
}

fn parse_calc_value(value: &str) -> Result<f32> {
    let value = value.trim();
    
    // Handle different units
    if value.ends_with("px") {
        let num_str = &value[..value.len()-2];
        num_str.parse::<f32>().map_err(|_| CompilerError::semantic_legacy(0, format!("Invalid px value: {}", value)))
    } else if value.ends_with("%") {
        let num_str = &value[..value.len()-1];
        // Convert percentage to decimal (50% -> 0.5)
        num_str.parse::<f32>().map(|v| v / 100.0).map_err(|_| CompilerError::semantic_legacy(0, format!("Invalid % value: {}", value)))
    } else if value.ends_with("em") {
        let num_str = &value[..value.len()-2];
        // Assume 1em = 16px for calc purposes
        num_str.parse::<f32>().map(|v| v * 16.0).map_err(|_| CompilerError::semantic_legacy(0, format!("Invalid em value: {}", value)))
    } else if value.ends_with("rem") {
        let num_str = &value[..value.len()-3];
        // Assume 1rem = 16px for calc purposes
        num_str.parse::<f32>().map(|v| v * 16.0).map_err(|_| CompilerError::semantic_legacy(0, format!("Invalid rem value: {}", value)))
    } else if value.ends_with("vw") {
        let num_str = &value[..value.len()-2];
        // Assume 1vw = 8px for calc purposes (800px viewport)
        num_str.parse::<f32>().map(|v| v * 8.0).map_err(|_| CompilerError::semantic_legacy(0, format!("Invalid vw value: {}", value)))
    } else if value.ends_with("vh") {
        let num_str = &value[..value.len()-2];
        // Assume 1vh = 6px for calc purposes (600px viewport)
        num_str.parse::<f32>().map(|v| v * 6.0).map_err(|_| CompilerError::semantic_legacy(0, format!("Invalid vh value: {}", value)))
    } else {
        // Pure number
        value.parse::<f32>().map_err(|_| CompilerError::semantic_legacy(0, format!("Invalid numeric value: {}", value)))
    }
}

/// Parse space-separated values from a string into PropertyValue array
fn parse_space_separated_values(value_str: &str) -> Vec<PropertyValue> {
    value_str
        .split_whitespace()
        .map(|s| {
            // Try to parse as integer first, then as float
            if let Ok(int_val) = s.parse::<i64>() {
                PropertyValue::Integer(int_val)
            } else if let Ok(float_val) = s.parse::<f64>() {
                PropertyValue::Number(float_val)
            } else {
                // If not a number, treat as string
                PropertyValue::String(s.to_string())
            }
        })
        .collect()
}

/// Expand shorthand properties into individual properties
fn expand_shorthand_property(ast_prop: &AstProperty) -> Result<Vec<AstProperty>> {
    // Check if this is a shorthand property
    match ast_prop.key.as_str() {
        "margin" => expand_margin_shorthand(ast_prop),
        "padding" => expand_padding_shorthand(ast_prop),
        "border" => expand_border_shorthand(ast_prop),
        _ => Ok(vec![ast_prop.clone()]), // Not a shorthand, return as-is
    }
}

/// Expand margin shorthand: margin: 10 0 -> margin_top: 10, margin_right: 0, etc.
fn expand_margin_shorthand(ast_prop: &AstProperty) -> Result<Vec<AstProperty>> {
    match &ast_prop.value {
        PropertyValue::Array(ref values) => {
            // Check if we have a single string value that contains space-separated values
            if values.len() == 1 {
                if let PropertyValue::String(ref string_value) = values[0] {
                    let cleaned_value = string_value.trim_matches('"').trim_matches('\'');
                    if cleaned_value.contains(' ') {
                        // Parse space-separated values
                        let parsed_values = parse_space_separated_values(cleaned_value);
                        let expanded = expand_box_model_values(&parsed_values, "margin", ast_prop.line)?;
                        return Ok(expanded);
                    }
                }
            }
            // Regular array handling
            let expanded = expand_box_model_values(values, "margin", ast_prop.line)?;
            Ok(expanded)
        }
        PropertyValue::String(ref string_value) => {
            // Check if the string contains space-separated values
            let cleaned_value = string_value.trim_matches('"').trim_matches('\'');
            if cleaned_value.contains(' ') {
                // Parse space-separated values
                let values = parse_space_separated_values(cleaned_value);
                let expanded = expand_box_model_values(&values, "margin", ast_prop.line)?;
                Ok(expanded)
            } else {
                // Single value: margin: "10" -> all sides
                let value = PropertyValue::String(cleaned_value.to_string());
                Ok(vec![
                    AstProperty::new("margin_top".to_string(), value.clone(), ast_prop.line),
                    AstProperty::new("margin_right".to_string(), value.clone(), ast_prop.line),
                    AstProperty::new("margin_bottom".to_string(), value.clone(), ast_prop.line),
                    AstProperty::new("margin_left".to_string(), value, ast_prop.line),
                ])
            }
        }
        _ => {
            // Single value: margin: 10 -> all sides
            let value = ast_prop.value.clone();
            Ok(vec![
                AstProperty::new("margin_top".to_string(), value.clone(), ast_prop.line),
                AstProperty::new("margin_right".to_string(), value.clone(), ast_prop.line),
                AstProperty::new("margin_bottom".to_string(), value.clone(), ast_prop.line),
                AstProperty::new("margin_left".to_string(), value, ast_prop.line),
            ])
        }
    }
}

/// Expand padding shorthand: padding: 10 0 -> padding_top: 10, padding_right: 0, etc.
fn expand_padding_shorthand(ast_prop: &AstProperty) -> Result<Vec<AstProperty>> {
    match &ast_prop.value {
        PropertyValue::Array(ref values) => {
            // Check if we have a single string value that contains space-separated values
            if values.len() == 1 {
                if let PropertyValue::String(ref string_value) = values[0] {
                    let cleaned_value = string_value.trim_matches('"').trim_matches('\'');
                    if cleaned_value.contains(' ') {
                        // Parse space-separated values
                        let parsed_values = parse_space_separated_values(cleaned_value);
                        let expanded = expand_box_model_values(&parsed_values, "padding", ast_prop.line)?;
                        return Ok(expanded);
                    }
                }
            }
            // Regular array handling
            let expanded = expand_box_model_values(values, "padding", ast_prop.line)?;
            Ok(expanded)
        }
        PropertyValue::String(ref string_value) => {
            // Check if the string contains space-separated values
            let cleaned_value = string_value.trim_matches('"').trim_matches('\'');
            if cleaned_value.contains(' ') {
                // Parse space-separated values
                let values = parse_space_separated_values(cleaned_value);
                let expanded = expand_box_model_values(&values, "padding", ast_prop.line)?;
                Ok(expanded)
            } else {
                // Single value: padding: "10" -> all sides
                let value = PropertyValue::String(cleaned_value.to_string());
                Ok(vec![
                    AstProperty::new("padding_top".to_string(), value.clone(), ast_prop.line),
                    AstProperty::new("padding_right".to_string(), value.clone(), ast_prop.line),
                    AstProperty::new("padding_bottom".to_string(), value.clone(), ast_prop.line),
                    AstProperty::new("padding_left".to_string(), value, ast_prop.line),
                ])
            }
        }
        _ => {
            // Single value: padding: 10 -> all sides
            let value = ast_prop.value.clone();
            Ok(vec![
                AstProperty::new("padding_top".to_string(), value.clone(), ast_prop.line),
                AstProperty::new("padding_right".to_string(), value.clone(), ast_prop.line),
                AstProperty::new("padding_bottom".to_string(), value.clone(), ast_prop.line),
                AstProperty::new("padding_left".to_string(), value, ast_prop.line),
            ])
        }
    }
}

/// Expand border shorthand: border: 1px solid #000 -> border_width: 1px, etc.
fn expand_border_shorthand(ast_prop: &AstProperty) -> Result<Vec<AstProperty>> {
    // For now, just return the original property - border shorthand is more complex
    // TODO: Implement full border shorthand parsing (width, style, color)
    Ok(vec![ast_prop.clone()])
}

/// Expand box model values based on CSS shorthand rules
/// 1 value: all sides
/// 2 values: [top/bottom, left/right]  
/// 3 values: [top, left/right, bottom]
/// 4 values: [top, right, bottom, left]
fn expand_box_model_values(values: &[PropertyValue], property_prefix: &str, line: usize) -> Result<Vec<AstProperty>> {
    match values.len() {
        1 => {
            // All sides
            let value = values[0].clone();
            Ok(vec![
                AstProperty::new(format!("{}_top", property_prefix), value.clone(), line),
                AstProperty::new(format!("{}_right", property_prefix), value.clone(), line),
                AstProperty::new(format!("{}_bottom", property_prefix), value.clone(), line),
                AstProperty::new(format!("{}_left", property_prefix), value, line),
            ])
        }
        2 => {
            // [top/bottom, left/right]
            let vertical = values[0].clone();
            let horizontal = values[1].clone();
            Ok(vec![
                AstProperty::new(format!("{}_top", property_prefix), vertical.clone(), line),
                AstProperty::new(format!("{}_right", property_prefix), horizontal.clone(), line),
                AstProperty::new(format!("{}_bottom", property_prefix), vertical, line),
                AstProperty::new(format!("{}_left", property_prefix), horizontal, line),
            ])
        }
        3 => {
            // [top, left/right, bottom]
            let top = values[0].clone();
            let horizontal = values[1].clone();
            let bottom = values[2].clone();
            Ok(vec![
                AstProperty::new(format!("{}_top", property_prefix), top, line),
                AstProperty::new(format!("{}_right", property_prefix), horizontal.clone(), line),
                AstProperty::new(format!("{}_bottom", property_prefix), bottom, line),
                AstProperty::new(format!("{}_left", property_prefix), horizontal, line),
            ])
        }
        4 => {
            // [top, right, bottom, left]
            Ok(vec![
                AstProperty::new(format!("{}_top", property_prefix), values[0].clone(), line),
                AstProperty::new(format!("{}_right", property_prefix), values[1].clone(), line),
                AstProperty::new(format!("{}_bottom", property_prefix), values[2].clone(), line),
                AstProperty::new(format!("{}_left", property_prefix), values[3].clone(), line),
            ])
        }
        _ => {
            Err(CompilerError::semantic_legacy(
                line,
                format!("Invalid number of values for {} shorthand: expected 1-4, got {}", property_prefix, values.len())
            ))
        }
    }
}

fn convert_ast_property_to_krb(ast_prop: &AstProperty, state: &mut CompilerState) -> Result<Option<KrbProperty>> {
    let cleaned_value = ast_prop.cleaned_value();
    
    // Check for calc() expressions first
    let processed_value = if cleaned_value.starts_with("calc(") {
        match parse_calc_expression(&cleaned_value) {
            Ok(result) => format!("{}", result),
            Err(_) => {
                // If calc() parsing fails, store as string for runtime evaluation
                cleaned_value.clone()
            }
        }
    } else {
        cleaned_value.clone()
    };
    
    // Use the comprehensive mapping from PropertyId::from_name()
    let property_id = PropertyId::from_name(&ast_prop.key);
    
    // If it's CustomData (unknown property), store as custom property
    if property_id == PropertyId::CustomData {
        return Ok(None); // Will be handled as custom property elsewhere
    }
    
    // Special handling for text property with array values
    if property_id == PropertyId::TextContent && matches!(ast_prop.value, PropertyValue::Array(_)) {
        if let PropertyValue::Array(ref array_values) = ast_prop.value {
            // Convert array to newline-separated string
            let mut lines = Vec::new();
            for value in array_values {
                match value {
                    PropertyValue::String(s) => lines.push(s.clone()),
                    _ => lines.push(value.to_string()),
                }
            }
            let joined_text = lines.join("\n");
            let string_index = state.add_string(joined_text)?;
            return Ok(Some(KrbProperty {
                property_id: property_id as u8,
                value_type: ValueType::String,
                size: 1,
                value: vec![string_index],
            }));
        }
    }
    
    // Now, correctly serialize the value based on the property ID.
    let krb_prop = match property_id {
        PropertyId::BackgroundColor | PropertyId::ForegroundColor | PropertyId::BorderColor => {
            if cleaned_value.starts_with('$') {
                // This is a template variable - store it as a string for later resolution
                let string_index = state.add_string(cleaned_value.clone())?;
                Some(KrbProperty { 
                    property_id: property_id as u8, 
                    value_type: ValueType::TemplateVariable,
                    size: 1, 
                    value: vec![string_index] 
                })
            } else if let Ok(color) = parse_color(&cleaned_value) {
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Color,
                    size: 4,
                    value: color.to_bytes().to_vec(),
                })
            } else {
                let length_info = if cleaned_value.starts_with('#') {
                    format!("length: {} chars after #", cleaned_value.len() - 1)
                } else {
                    format!("length: {} chars total", cleaned_value.len())
                };
                return Err(CompilerError::semantic_legacy(ast_prop.line, format!("Invalid color value: '{}' ({})", cleaned_value, length_info)));
            }
        }
        PropertyId::BorderWidth | PropertyId::BorderRadius | PropertyId::Padding | PropertyId::Margin | PropertyId::Gap => {
            if let Ok(val) = cleaned_value.parse::<u8>() {
                Some(KrbProperty { property_id: property_id as u8, value_type: ValueType::Byte, size: 1, value: vec![val] })
            } else {
                return Err(CompilerError::semantic_legacy(ast_prop.line, format!("Invalid numeric value for {}: {}", ast_prop.key, cleaned_value)));
            }
        }
        PropertyId::TextAlignment => {
            let alignment_val = match cleaned_value.to_lowercase().as_str() {
                "start" => 0u8, "center" => 1u8, "end" => 2u8, "justify" => 3u8,
                _ => 1u8, // Default to center
            };
            Some(KrbProperty { property_id: property_id as u8, value_type: ValueType::Enum, size: 1, value: vec![alignment_val] })
        }
        PropertyId::ListStyleType => {
            let list_style_val = match cleaned_value.to_lowercase().as_str() {
                "bullet" => 1u8,
                "number" => 2u8,
                "none" => 0u8,
                _ => 0u8, // Default to none
            };
            Some(KrbProperty { property_id: property_id as u8, value_type: ValueType::Enum, size: 1, value: vec![list_style_val] })
        }
        PropertyId::WhiteSpace => {
            let white_space_val = match cleaned_value.to_lowercase().as_str() {
                "normal" => 0u8,
                "nowrap" => 1u8,
                "pre" => 2u8,
                _ => 0u8, // Default to normal
            };
            Some(KrbProperty { property_id: property_id as u8, value_type: ValueType::Enum, size: 1, value: vec![white_space_val] })
        }
        PropertyId::FontSize => {
            if let Ok(val) = cleaned_value.parse::<u16>() {
                Some(KrbProperty { property_id: property_id as u8, value_type: ValueType::Short, size: 2, value: val.to_le_bytes().to_vec() })
            } else if cleaned_value.starts_with('$') {
                // This is a template variable - store it as a string for later resolution
                let string_index = state.add_string(cleaned_value.clone())?;
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::TemplateVariable,
                    size: 1,
                    value: vec![string_index],
                })
            } else {
                return Err(CompilerError::semantic_legacy(ast_prop.line, format!("Invalid font size value: {}", cleaned_value)));
            }
        }
        PropertyId::FontWeight => {
            if cleaned_value.starts_with('$') {
                // This is a template variable - store it as a string for later resolution
                let string_index = state.add_string(cleaned_value.clone())?;
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::TemplateVariable,
                    size: 1,
                    value: vec![string_index],
                })
            } else {
                let weight_val = match cleaned_value.to_lowercase().as_str() {
                    "normal" => 400u16,
                    "bold" => 700u16,
                    "light" => 300u16,
                    "heavy" => 900u16,
                    _ => if let Ok(val) = cleaned_value.parse::<u16>() { val } else { 400 }
                };
                Some(KrbProperty { property_id: property_id as u8, value_type: ValueType::Short, size: 2, value: weight_val.to_le_bytes().to_vec() })
            }
        }
        PropertyId::FontFamily => {
            let string_index = state.add_string(cleaned_value.clone())?;
            Some(KrbProperty { property_id: property_id as u8, value_type: ValueType::String, size: 1, value: vec![string_index] })
        }
        PropertyId::TextContent | PropertyId::WindowTitle => {
            // Keep variables as placeholders for reactive template system
            let string_index = state.add_string(cleaned_value.clone())?;
            Some(KrbProperty { property_id: property_id as u8, value_type: ValueType::String, size: 1, value: vec![string_index] })
        }
        PropertyId::Height | PropertyId::Width | PropertyId::Top | PropertyId::Left => {
            // Parse numeric value as u16 or percentage
            if let Ok(val) = cleaned_value.parse::<u16>() {
                // Regular pixel value
                Some(KrbProperty { property_id: property_id as u8, value_type: ValueType::Short, size: 2, value: val.to_le_bytes().to_vec() })
            } else if cleaned_value.ends_with('%') {
                // Percentage value - parse as float and store as percentage type
                let percent_str = &cleaned_value[..cleaned_value.len() - 1]; // Remove '%'
                if let Ok(percent) = percent_str.parse::<f32>() {
                    // Store percentage as 4-byte float
                    Some(KrbProperty { property_id: property_id as u8, value_type: ValueType::Percentage, size: 4, value: percent.to_le_bytes().to_vec() })
                } else {
                    return Err(CompilerError::semantic_legacy(ast_prop.line, format!("Invalid percentage value for {}: {}", ast_prop.key, cleaned_value)));
                }
            } else if cleaned_value.starts_with('$') {
                // This is a template variable - store it as a string for later resolution
                let string_index = state.add_string(cleaned_value.clone())?;
                Some(KrbProperty { 
                    property_id: property_id as u8, 
                    value_type: ValueType::TemplateVariable, // Special marker for template variables
                    size: 1, 
                    value: vec![string_index] 
                })
            } else {
                return Err(CompilerError::semantic_legacy(ast_prop.line, format!("Invalid numeric value for {}: {} (must be a number or percentage)", ast_prop.key, cleaned_value)));
            }
        }
        // Modern Taffy layout properties
        PropertyId::Display | PropertyId::FlexDirection | PropertyId::Position |
        PropertyId::AlignItems | PropertyId::AlignContent | PropertyId::JustifyContent |
        PropertyId::JustifyItems | PropertyId::JustifySelf | PropertyId::AlignSelf => {
            // Store as string index like other string properties
            let string_index = state.add_string(cleaned_value.clone())?;
            Some(KrbProperty { 
                property_id: property_id as u8, 
                value_type: ValueType::String, 
                size: 1, 
                value: vec![string_index] 
            })
        }
        PropertyId::FlexGrow | PropertyId::FlexShrink => {
            // Store as float values
            if let Ok(val) = cleaned_value.parse::<f32>() {
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Percentage, // Reuse percentage type for float
                    size: 4,
                    value: val.to_le_bytes().to_vec(),
                })
            } else if cleaned_value.starts_with('$') {
                // This is a template variable - store it as a string for later resolution
                let string_index = state.add_string(cleaned_value.clone())?;
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::TemplateVariable,
                    size: 1,
                    value: vec![string_index],
                })
            } else {
                return Err(CompilerError::semantic_legacy(ast_prop.line, format!("Invalid float value for {}: {}", ast_prop.key, cleaned_value)));
            }
        }
        PropertyId::InputType => {
            // Parse the input type string and convert to InputType enum value
            let input_type_value = if let Some(input_type) = InputType::from_name(&cleaned_value) {
                input_type as u8
            } else {
                return Err(CompilerError::semantic_legacy(
                    ast_prop.line, 
                    format!("Invalid input type: '{}'. Valid types are: text, password, email, number, tel, url, search, checkbox, radio, range, date, datetime-local, month, time, week, color, file, hidden, submit, reset, button, image", cleaned_value)
                ));
            };
            Some(KrbProperty { 
                property_id: property_id as u8, 
                value_type: ValueType::Enum, 
                size: 1, 
                value: vec![input_type_value] 
            })
        }
        PropertyId::EmbedViewType => {
            // Parse the embed view type string and convert to EmbedViewType enum value
            let embed_type_value = if let Some(embed_type) = EmbedViewType::from_name(&cleaned_value) {
                embed_type as u8
            } else {
                return Err(CompilerError::semantic_legacy(
                    ast_prop.line, 
                    format!("Invalid EmbedView type: '{}'. Valid types are: webview (or web), native_renderer (or native), wasm_view (or wasm), uxn_view (or uxn), gba_view (or gba), dos_view (or dos)", cleaned_value)
                ));
            };
            Some(KrbProperty { 
                property_id: property_id as u8, 
                value_type: ValueType::Enum, 
                size: 1, 
                value: vec![embed_type_value] 
            })
        }
        PropertyId::ImageSource => {
            // Store image source path as string
            let string_index = state.add_string(cleaned_value.clone())?;
            Some(KrbProperty { 
                property_id: property_id as u8, 
                value_type: ValueType::String, 
                size: 1, 
                value: vec![string_index] 
            })
        }
        PropertyId::ZIndex => {
            if let Ok(val) = cleaned_value.parse::<i16>() {
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Short,
                    size: 2,
                    value: val.to_le_bytes().to_vec(),
                })
            } else {
                return Err(CompilerError::semantic_legacy(ast_prop.line, format!("Invalid z-index value: {}", cleaned_value)));
            }
        }
        PropertyId::Visibility => {
            // Handle visible/visibility property - convert boolean or string to boolean, or template variables
            if cleaned_value.starts_with('$') {
                // This is a template variable - store it as a string for later resolution
                let string_index = state.add_string(cleaned_value.clone())?;
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::TemplateVariable,
                    size: 1,
                    value: vec![string_index],
                })
            } else {
                let visible = match cleaned_value.to_lowercase().as_str() {
                    "true" | "visible" | "1" => true,
                    "false" | "hidden" | "0" => false,
                    _ => {
                        return Err(CompilerError::semantic_legacy(
                            ast_prop.line,
                            format!("Invalid visibility value: '{}'. Use 'true', 'false', 'visible', 'hidden', or a template variable starting with '$'", cleaned_value)
                        ));
                    }
                };
                
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Byte,
                    size: 1,
                    value: vec![if visible { 1 } else { 0 }],
                })
            }
        }
        
        // CSS Grid Properties
        PropertyId::GridTemplateColumns | PropertyId::GridTemplateRows => {
            // Store grid template as string (e.g., "1fr 200px 1fr", "repeat(3, 1fr)")
            let string_index = state.add_string(cleaned_value.clone())?;
            Some(KrbProperty {
                property_id: property_id as u8,
                value_type: ValueType::String,
                size: 1,
                value: vec![string_index],
            })
        }
        PropertyId::GridTemplateAreas => {
            // Store grid template areas as string (e.g., "header header" "sidebar content")
            let string_index = state.add_string(cleaned_value.clone())?;
            Some(KrbProperty {
                property_id: property_id as u8,
                value_type: ValueType::String,
                size: 1,
                value: vec![string_index],
            })
        }
        PropertyId::GridArea => {
            // Store grid area as string (e.g., "header", "1 / 2 / 3 / 4")
            let string_index = state.add_string(cleaned_value.clone())?;
            Some(KrbProperty {
                property_id: property_id as u8,
                value_type: ValueType::String,
                size: 1,
                value: vec![string_index],
            })
        }
        PropertyId::GridAutoFlow => {
            let flow_val = match cleaned_value.to_lowercase().as_str() {
                "row" => 0u8,
                "column" => 1u8,
                "row dense" | "dense row" => 2u8,
                "column dense" | "dense column" => 3u8,
                _ => 0u8, // Default to row
            };
            Some(KrbProperty {
                property_id: property_id as u8,
                value_type: ValueType::Enum,
                size: 1,
                value: vec![flow_val],
            })
        }
        PropertyId::GridColumn | PropertyId::GridRow => {
            // Store grid line specification as string (e.g., "1 / 3", "span 2")
            let string_index = state.add_string(cleaned_value.clone())?;
            Some(KrbProperty {
                property_id: property_id as u8,
                value_type: ValueType::String,
                size: 1,
                value: vec![string_index],
            })
        }
        PropertyId::GridColumnStart | PropertyId::GridColumnEnd | PropertyId::GridRowStart | PropertyId::GridRowEnd => {
            // Parse grid line as integer or store as string for named lines
            if let Ok(line_num) = cleaned_value.parse::<u16>() {
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Short,
                    size: 2,
                    value: line_num.to_le_bytes().to_vec(),
                })
            } else {
                // Named grid line
                let string_index = state.add_string(cleaned_value.clone())?;
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::String,
                    size: 1,
                    value: vec![string_index],
                })
            }
        }
        
        // Individual Box Model Properties
        PropertyId::PaddingTop | PropertyId::PaddingRight | PropertyId::PaddingBottom | PropertyId::PaddingLeft |
        PropertyId::MarginTop | PropertyId::MarginRight | PropertyId::MarginBottom | PropertyId::MarginLeft => {
            if let Ok(val) = cleaned_value.parse::<u8>() {
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Byte,
                    size: 1,
                    value: vec![val],
                })
            } else {
                return Err(CompilerError::semantic_legacy(
                    ast_prop.line,
                    format!("Invalid numeric value for {}: {}", ast_prop.key, cleaned_value)
                ));
            }
        }
        PropertyId::BorderTopWidth | PropertyId::BorderRightWidth | PropertyId::BorderBottomWidth | PropertyId::BorderLeftWidth => {
            if let Ok(val) = cleaned_value.parse::<u8>() {
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Byte,
                    size: 1,
                    value: vec![val],
                })
            } else {
                return Err(CompilerError::semantic_legacy(
                    ast_prop.line,
                    format!("Invalid border width value for {}: {}", ast_prop.key, cleaned_value)
                ));
            }
        }
        PropertyId::BorderTopColor | PropertyId::BorderRightColor | PropertyId::BorderBottomColor | PropertyId::BorderLeftColor => {
            if cleaned_value.starts_with('$') {
                // This is a template variable - store it as a string for later resolution
                let string_index = state.add_string(cleaned_value.clone())?;
                Some(KrbProperty { 
                    property_id: property_id as u8, 
                    value_type: ValueType::TemplateVariable,
                    size: 1, 
                    value: vec![string_index] 
                })
            } else if let Ok(color) = parse_color(&cleaned_value) {
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Color,
                    size: 4,
                    value: color.to_bytes().to_vec(),
                })
            } else {
                let length_info = if cleaned_value.starts_with('#') {
                    format!("length: {} chars after #", cleaned_value.len() - 1)
                } else {
                    format!("length: {} chars total", cleaned_value.len())
                };
                return Err(CompilerError::semantic_legacy(
                    ast_prop.line,
                    format!("Invalid color value for {}: '{}' ({})", ast_prop.key, cleaned_value, length_info)
                ));
            }
        }
        PropertyId::BorderTopLeftRadius | PropertyId::BorderTopRightRadius | 
        PropertyId::BorderBottomRightRadius | PropertyId::BorderBottomLeftRadius => {
            if let Ok(val) = cleaned_value.parse::<u8>() {
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Byte,
                    size: 1,
                    value: vec![val],
                })
            } else {
                return Err(CompilerError::semantic_legacy(
                    ast_prop.line,
                    format!("Invalid border radius value for {}: {}", ast_prop.key, cleaned_value)
                ));
            }
        }
        PropertyId::BoxSizing => {
            let box_sizing_val = match cleaned_value.to_lowercase().as_str() {
                "content-box" => 0u8,
                "border-box" => 1u8,
                _ => 0u8, // Default to content-box
            };
            Some(KrbProperty {
                property_id: property_id as u8,
                value_type: ValueType::Enum,
                size: 1,
                value: vec![box_sizing_val],
            })
        }
        
        // Position Properties (PropertyId::Position is handled above with other Taffy properties)
        PropertyId::Right | PropertyId::Bottom => {
            // Parse as numeric value or percentage (similar to Left/Top)
            if let Ok(val) = cleaned_value.parse::<u16>() {
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Short,
                    size: 2,
                    value: val.to_le_bytes().to_vec(),
                })
            } else if cleaned_value.ends_with('%') {
                let percent_str = &cleaned_value[..cleaned_value.len() - 1];
                if let Ok(percent_val) = percent_str.parse::<f32>() {
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Percentage,
                        size: 4,
                        value: percent_val.to_le_bytes().to_vec(),
                    })
                } else {
                    return Err(CompilerError::semantic_legacy(
                        ast_prop.line,
                        format!("Invalid percentage value for {}: {}", ast_prop.key, cleaned_value)
                    ));
                }
            } else {
                return Err(CompilerError::semantic_legacy(
                    ast_prop.line,
                    format!("Invalid numeric value for {}: {} (must be a number or percentage)", ast_prop.key, cleaned_value)
                ));
            }
        }
        
        // Advanced Flexbox Properties
        PropertyId::FlexWrap => {
            let flex_wrap_val = match cleaned_value.to_lowercase().as_str() {
                "nowrap" => 0u8,
                "wrap" => 1u8,
                "wrap-reverse" => 2u8,
                _ => 0u8, // Default to nowrap
            };
            Some(KrbProperty {
                property_id: property_id as u8,
                value_type: ValueType::Enum,
                size: 1,
                value: vec![flex_wrap_val],
            })
        }
        PropertyId::Order => {
            if let Ok(order_val) = cleaned_value.parse::<i16>() {
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Short,
                    size: 2,
                    value: order_val.to_le_bytes().to_vec(),
                })
            } else {
                return Err(CompilerError::semantic_legacy(
                    ast_prop.line,
                    format!("Invalid order value: {} (must be an integer)", cleaned_value)
                ));
            }
        }
        
        // Typography Properties
        PropertyId::LineHeight => {
            // Support both numeric (1.5) and length values (24px)
            if let Ok(line_height) = cleaned_value.parse::<f32>() {
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Float,
                    size: 4,
                    value: line_height.to_le_bytes().to_vec(),
                })
            } else if cleaned_value.ends_with("px") {
                let px_str = &cleaned_value[..cleaned_value.len() - 2];
                if let Ok(px_val) = px_str.parse::<u16>() {
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Short,
                        size: 2,
                        value: px_val.to_le_bytes().to_vec(),
                    })
                } else {
                    return Err(CompilerError::semantic_legacy(
                        ast_prop.line,
                        format!("Invalid line-height value: {}", cleaned_value)
                    ));
                }
            } else {
                return Err(CompilerError::semantic_legacy(
                    ast_prop.line,
                    format!("Invalid line-height value: {} (use number or px unit)", cleaned_value)
                ));
            }
        }
        PropertyId::LetterSpacing | PropertyId::WordSpacing => {
            // Support px, em, rem values
            if cleaned_value.ends_with("px") {
                let px_str = &cleaned_value[..cleaned_value.len() - 2];
                if let Ok(px_val) = px_str.parse::<i16>() {
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Short,
                        size: 2,
                        value: px_val.to_le_bytes().to_vec(),
                    })
                } else {
                    return Err(CompilerError::semantic_legacy(
                        ast_prop.line,
                        format!("Invalid spacing value: {}", cleaned_value)
                    ));
                }
            } else if cleaned_value.ends_with("em") || cleaned_value.ends_with("rem") {
                let unit_str = &cleaned_value[..cleaned_value.len() - if cleaned_value.ends_with("rem") { 3 } else { 2 }];
                if let Ok(em_val) = unit_str.parse::<f32>() {
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Float,
                        size: 4,
                        value: em_val.to_le_bytes().to_vec(),
                    })
                } else {
                    return Err(CompilerError::semantic_legacy(
                        ast_prop.line,
                        format!("Invalid spacing value: {}", cleaned_value)
                    ));
                }
            } else {
                return Err(CompilerError::semantic_legacy(
                    ast_prop.line,
                    format!("Invalid spacing value: {} (use px, em, or rem units)", cleaned_value)
                ));
            }
        }
        PropertyId::TextDecoration => {
            let decoration_val = match cleaned_value.to_lowercase().as_str() {
                "none" => 0u8,
                "underline" => 1u8,
                "overline" => 2u8,
                "line-through" => 3u8,
                "underline overline" | "overline underline" => 4u8,
                _ => 0u8, // Default to none
            };
            Some(KrbProperty {
                property_id: property_id as u8,
                value_type: ValueType::Enum,
                size: 1,
                value: vec![decoration_val],
            })
        }
        PropertyId::TextTransform => {
            let transform_val = match cleaned_value.to_lowercase().as_str() {
                "none" => 0u8,
                "uppercase" => 1u8,
                "lowercase" => 2u8,
                "capitalize" => 3u8,
                _ => 0u8, // Default to none
            };
            Some(KrbProperty {
                property_id: property_id as u8,
                value_type: ValueType::Enum,
                size: 1,
                value: vec![transform_val],
            })
        }
        PropertyId::TextIndent => {
            if let Ok(indent_val) = cleaned_value.parse::<u16>() {
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Short,
                    size: 2,
                    value: indent_val.to_le_bytes().to_vec(),
                })
            } else if cleaned_value.ends_with('%') {
                let percent_str = &cleaned_value[..cleaned_value.len() - 1];
                if let Ok(percent_val) = percent_str.parse::<f32>() {
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Percentage,
                        size: 4,
                        value: percent_val.to_le_bytes().to_vec(),
                    })
                } else {
                    return Err(CompilerError::semantic_legacy(
                        ast_prop.line,
                        format!("Invalid text-indent percentage: {}", cleaned_value)
                    ));
                }
            } else {
                return Err(CompilerError::semantic_legacy(
                    ast_prop.line,
                    format!("Invalid text-indent value: {} (use number or percentage)", cleaned_value)
                ));
            }
        }
        PropertyId::TextOverflow => {
            let overflow_val = match cleaned_value.to_lowercase().as_str() {
                "clip" => 0u8,
                "ellipsis" => 1u8,
                _ => 0u8, // Default to clip
            };
            Some(KrbProperty {
                property_id: property_id as u8,
                value_type: ValueType::Enum,
                size: 1,
                value: vec![overflow_val],
            })
        }
        PropertyId::FontStyle => {
            let style_val = match cleaned_value.to_lowercase().as_str() {
                "normal" => 0u8,
                "italic" => 1u8,
                "oblique" => 2u8,
                _ => 0u8, // Default to normal
            };
            Some(KrbProperty {
                property_id: property_id as u8,
                value_type: ValueType::Enum,
                size: 1,
                value: vec![style_val],
            })
        }
        PropertyId::FontVariant => {
            let variant_val = match cleaned_value.to_lowercase().as_str() {
                "normal" => 0u8,
                "small-caps" => 1u8,
                _ => 0u8, // Default to normal
            };
            Some(KrbProperty {
                property_id: property_id as u8,
                value_type: ValueType::Enum,
                size: 1,
                value: vec![variant_val],
            })
        }
        
        // Overflow Properties (already implemented in property registry)
        PropertyId::Overflow | PropertyId::OverflowX | PropertyId::OverflowY => {
            let overflow_val = match cleaned_value.to_lowercase().as_str() {
                "visible" => 0u8,
                "hidden" => 1u8,
                "scroll" => 2u8,
                "auto" => 3u8,
                _ => 0u8, // Default to visible
            };
            Some(KrbProperty {
                property_id: property_id as u8,
                value_type: ValueType::Enum,
                size: 1,
                value: vec![overflow_val],
            })
        }
        
        // Visual Effects Properties
        PropertyId::BoxShadow | PropertyId::TextShadow => {
            // Store shadow definition as string (e.g., "2px 2px 4px rgba(0,0,0,0.5)" or "none")
            if cleaned_value.to_lowercase() == "none" {
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Enum,
                    size: 1,
                    value: vec![0u8], // 0 = none
                })
            } else {
                // Store complex shadow as string for parsing by renderer
                let string_index = state.add_string(cleaned_value.clone())?;
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::String,
                    size: 1,
                    value: vec![string_index],
                })
            }
        }
        PropertyId::Filter | PropertyId::BackdropFilter => {
            // Store filter functions as string (e.g., "blur(5px)", "brightness(150%)")
            if cleaned_value.to_lowercase() == "none" {
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Enum,
                    size: 1,
                    value: vec![0u8], // 0 = none
                })
            } else {
                let string_index = state.add_string(cleaned_value.clone())?;
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::String,
                    size: 1,
                    value: vec![string_index],
                })
            }
        }
        
        // Responsive Properties (for media query support)
        PropertyId::MinViewportWidth | PropertyId::MaxViewportWidth => {
            if let Ok(width_val) = cleaned_value.parse::<u16>() {
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Short,
                    size: 2,
                    value: width_val.to_le_bytes().to_vec(),
                })
            } else if cleaned_value.ends_with("px") {
                let px_str = &cleaned_value[..cleaned_value.len() - 2];
                if let Ok(px_val) = px_str.parse::<u16>() {
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Short,
                        size: 2,
                        value: px_val.to_le_bytes().to_vec(),
                    })
                } else {
                    return Err(CompilerError::semantic_legacy(
                        ast_prop.line,
                        format!("Invalid viewport width value: {}", cleaned_value)
                    ));
                }
            } else {
                return Err(CompilerError::semantic_legacy(
                    ast_prop.line,
                    format!("Invalid viewport width value: {} (use number or px unit)", cleaned_value)
                ));
            }
        }
        
        _ => None, // Should not be reached due to the initial match, but it's safe.
    };
    
    Ok(krb_prop)
}
