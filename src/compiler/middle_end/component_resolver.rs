//! Component instantiation and resolution system

use crate::compiler::frontend::ast::*;
use crate::compiler::middle_end::script::ScriptProcessor;
use crate::error::{CompilerError, Result};
use crate::core::*;
use crate::core::types::*;
use crate::compiler::middle_end::variable_context::VariableScope;
use std::collections::HashMap;
use regex;
use crate::core::{FunctionScope, ResolvedFunction};

pub struct ComponentResolver {
    instantiation_stack: Vec<String>, // Track instantiation to detect recursion
}

impl ComponentResolver {
    pub fn new() -> Self {
        Self {
            instantiation_stack: Vec::new(),
        }
    }
    
    /// Substitute variables in a string using the provided mapping
    /// Supports both $variable and ${variable} syntax
    pub fn substitute_variables(&self, input: &str, mapping: &HashMap<String, String>) -> Result<String> {
        let mut result = input.to_string();
        
        // Support both $variable and ${variable} syntax
        let var_regex = regex::Regex::new(r"\$\{([a-zA-Z_][a-zA-Z0-9_]*)\}|\$([a-zA-Z_][a-zA-Z0-9_]*)").unwrap();
        
        // Process both $variable and ${variable} syntax
        for capture in var_regex.captures_iter(input) {
            let full_match = capture.get(0).unwrap().as_str();
            
            // Check which capture group matched (1 for ${variable}, 2 for $variable)
            let var_name_str = if let Some(braced_var) = capture.get(1) {
                braced_var.as_str()
            } else if let Some(simple_var) = capture.get(2) {
                simple_var.as_str()
            } else {
                continue;
            };
            
            if let Some(value) = mapping.get(var_name_str) {
                result = result.replace(full_match, value);
            } else {
                println!("DEBUG: Component resolver variable substitution failed for '{}'", var_name_str);
                println!("  Input: {}", input);
                println!("  Available in mapping:");
                for (k, v) in mapping {
                    println!("    {} = {}", k, v);
                }
                return Err(CompilerError::Variable {
                    file: "test".to_string(),
                    line: 0,
                    message: format!("Undefined variable: {}", var_name_str),
                });
            }
        }
        
        Ok(result)
    }
    
    pub fn resolve_components(&mut self, ast: &mut AstNode, state: &mut CompilerState) -> Result<()> {
        println!("DEBUG: Starting component resolution");
        
        // First, resolve template structures (like @for loops) in the main App element
        println!("DEBUG: Processing template structures (for loops, if statements)");
        self.resolve_template_structures(ast, state)?;
        println!("DEBUG: Template structures processed");
        
        // Then resolve component instances
        println!("DEBUG: Processing component instances");
        self.resolve_recursive(ast, state)?;
        println!("DEBUG: Component instances processed");
        
        self.update_component_statistics(state);
        println!("DEBUG: Component resolution complete");
        Ok(())
    }
    
    fn resolve_template_structures(&mut self, ast: &mut AstNode, state: &mut CompilerState) -> Result<()> {
        match ast {
            AstNode::File { app, .. } => {
                if let Some(app_node) = app {
                    self.process_template_in_element(app_node, state)?;
                }
            }
            _ => {
                self.process_template_in_element(ast, state)?;
            }
        }
        Ok(())
    }
    
    fn process_template_in_element(&mut self, element: &mut AstNode, state: &mut CompilerState) -> Result<()> {
        match element {
            AstNode::Element { children, .. } => {
                let mut i = 0;
                while i < children.len() {
                    match &children[i] {
                        AstNode::For { index_variable, variable, collection, body } => {
                            // Extract values to avoid borrowing issues
                            let index_var = index_variable.clone();
                            let var_name = variable.clone();
                            let collection_name = collection.clone();
                            let body_clone = body.clone();
                            
                            // Expand the @for loop
                            let mut expanded = AstNode::Element {
                                element_type: "Container".to_string(),
                                properties: vec![],
                                pseudo_selectors: vec![],
                                children: vec![],
                            };
                            
                            self.expand_for_loop(&mut expanded, index_var.as_deref(), &var_name, &collection_name, &body_clone, state)?;
                            
                            // Replace the @for with the expanded container
                            if let AstNode::Element { children: expanded_children, .. } = expanded {
                                let len = expanded_children.len();
                                children.remove(i);
                                for (j, child) in expanded_children.into_iter().enumerate() {
                                    children.insert(i + j, child);
                                }
                                i += len;
                            } else {
                                i += 1;
                            }
                        }
                        AstNode::If { condition, then_body, elif_branches, else_body } => {
                            // Similar expansion for @if
                            let cond = condition.clone();
                            let then_clone = then_body.clone();
                            let elif_clone = elif_branches.clone();
                            let else_clone = else_body.clone();
                            
                            let mut expanded = AstNode::Element {
                                element_type: "Container".to_string(),
                                properties: vec![],
                                pseudo_selectors: vec![],
                                children: vec![],
                            };
                            
                            self.expand_if_conditional(&mut expanded, &cond, &then_clone, &elif_clone, &else_clone, state)?;
                            
                            // Replace the @if with the expanded container
                            if let AstNode::Element { children: expanded_children, .. } = expanded {
                                let len = expanded_children.len();
                                children.remove(i);
                                for (j, child) in expanded_children.into_iter().enumerate() {
                                    children.insert(i + j, child);
                                }
                                i += len;
                            } else {
                                i += 1;
                            }
                        }
                        _ => {
                            // Recursively process children
                            self.process_template_in_element(&mut children[i], state)?;
                            i += 1;
                        }
                    }
                }
            }
            _ => {}
        }
        Ok(())
    }
    
    fn resolve_recursive(&mut self, ast: &mut AstNode, state: &mut CompilerState) -> Result<()> {
        match ast {
            AstNode::File { app, components, .. } => {
                // First pass: collect all component definitions
                for component_node in components {
                    if let AstNode::Component { name, .. } = component_node {
                        if self.instantiation_stack.contains(name) {
                            return Err(CompilerError::component_legacy(
                                0,
                                format!("Recursive component definition detected: {}", name)
                            ));
                        }
                    }
                }
                
                // Second pass: resolve component instances and template structures in the main app
                if let Some(app_node) = app {
                    self.resolve_element_components(app_node, state)?;
                }
            }
            // Handle standalone template structures at any level
            _ => {
                self.resolve_element_components(ast, state)?;
            }
        }
        
        Ok(())
    }
    
    fn resolve_element_components(&mut self, element: &mut AstNode, state: &mut CompilerState) -> Result<()> {
        match element {
            AstNode::Element { element_type,  children, .. } => {
                // Check if this element is a component instance
                if let Some(component_def) = self.find_component_definition(element_type, state) {
                    self.instantiate_component(element, &component_def, state)?;
                } else {
                    // Regular element - just process children
                    for child in children {
                        self.resolve_element_components(child, state)?;
                    }
                }
            }
            AstNode::For { index_variable, variable, collection, body } => {
                // Extract values to avoid borrowing issues
                let index_var = index_variable.clone();
                let var_name = variable.clone();
                let collection_name = collection.clone();
                let body_clone = body.clone();
                
                // Expand @for loops into multiple elements
                self.expand_for_loop(element, index_var.as_deref(), &var_name, &collection_name, &body_clone, state)?;
            }
            AstNode::If { condition, then_body, elif_branches, else_body } => {
                // Extract values to avoid borrowing issues
                let cond = condition.clone();
                let then_clone = then_body.clone();
                let elif_clone = elif_branches.clone();
                let else_clone = else_body.clone();
                
                // Evaluate @if conditions and expand accordingly
                self.expand_if_conditional(element, &cond, &then_clone, &elif_clone, &else_clone, state)?;
            }
            _ => {}
        }
        
        Ok(())
    }
    
    fn find_component_definition(&self, name: &str, state: &CompilerState) -> Option<ComponentDefinition> {
        println!("DEBUG: Looking for component '{}' in {} available components:", name, state.component_defs.len());
        for comp in &state.component_defs {
            println!("  Available component: {}", comp.name);
        }
        state.component_defs.iter()
            .find(|comp| comp.name == name)
            .cloned()
    }
    
    fn instantiate_component(
        &mut self,
        element: &mut AstNode,
        component_def: &ComponentDefinition,
        state: &mut CompilerState
    ) -> Result<()> {
        if let AstNode::Element {  properties, children, .. } = element {
            // Check for recursive instantiation
            if self.instantiation_stack.contains(&component_def.name) {
                return Err(CompilerError::component_legacy(
                    0,
                    format!("Recursive component instantiation: {}", component_def.name)
                ));
            }
            
            self.instantiation_stack.push(component_def.name.clone());
            
            // Get the component template
            let template = self.get_component_template(component_def, state)?;
            
            // Push component scope and add component properties
            state.variable_context.push_scope(VariableScope::Component);
            
            println!("DEBUG: Instantiating component '{}' with properties:", component_def.name);
            
            // Add component properties to variable context
            for prop_def in &component_def.properties {
                // Strip quotes from default values for variable substitution
                let clean_default = if prop_def.default_value.starts_with('"') && prop_def.default_value.ends_with('"') {
                    prop_def.default_value[1..prop_def.default_value.len()-1].to_string()
                } else {
                    prop_def.default_value.clone()
                };
                
                state.variable_context.add_string_variable(
                    prop_def.name.clone(),
                    clean_default.clone(),
                    state.current_file_path.clone(),
                    0 // TODO: get actual line from component definition
                )?;
                println!("  Added property: {} = {}", prop_def.name, clean_default);
            }
            
            // Override with instance properties
            for instance_prop in properties {
                // Strip quotes from string values for variable substitution
                let clean_value = match &instance_prop.value {
                    PropertyValue::String(s) => {
                        if s.starts_with('"') && s.ends_with('"') {
                            s[1..s.len()-1].to_string()
                        } else {
                            s.clone()
                        }
                    }
                    _ => instance_prop.value.to_string(),
                };
                
                state.variable_context.add_string_variable(
                    instance_prop.key.clone(),
                    clean_value.clone(),
                    state.current_file_path.clone(),
                    instance_prop.line
                )?;
                println!("  Overrode property: {} = {}", instance_prop.key, clean_value);
            }
            
            // Clone and customize the template using the variable context
            let mut instantiated_template = template.clone();
            
            // Process template structures (like @for) now that variables are available
            self.process_template_in_element(&mut instantiated_template, state)?;
            
            self.apply_variable_substitution(&mut instantiated_template, state)?;
            
            // Handle instance children (slot content)
            if !children.is_empty() {
                self.inject_slot_content(&mut instantiated_template, children)?;
            }
            
            // Replace the component instance with the instantiated template
            *element = instantiated_template;
            
            // Resolve component function templates while in component scope
            self.resolve_component_function_templates(&component_def.name, state)?;
            
            // Process deferred component scripts while in component scope
            self.process_component_scripts(&component_def.name, state)?;
            
            // Recursively resolve any nested components
            self.resolve_element_components(element, state)?;
            
            // Pop component scope
            state.variable_context.pop_scope()?;
            
            self.instantiation_stack.pop();
        }
        
        Ok(())
    }
    
    fn get_component_template(&self, component_def: &ComponentDefinition, state: &CompilerState) -> Result<AstNode> {
        // Look for the template in our temporary AST storage
        if let Some(template_ast) = state.component_ast_templates.get(&component_def.name) {
            Ok(template_ast.clone())
        } else {
            Err(CompilerError::component_legacy(
                0,
                format!("Component '{}' has no template defined", component_def.name)
            ))
        }
    }
    
    fn convert_element_to_ast(&self, element: &Element, state: &CompilerState) -> Result<AstNode> {
        let mut properties = Vec::new();
        
        // Convert source properties back to AST properties
        for source_prop in &element.source_properties {
            properties.push(AstProperty::new(
                source_prop.key.clone(),
                PropertyValue::String(source_prop.value.clone()),
                source_prop.line_num,
            ));
        }
        
        // Convert children
        let mut children = Vec::new();
        for &child_index in &element.children {
            if let Some(child_element) = state.elements.get(child_index) {
                children.push(self.convert_element_to_ast(child_element, state)?);
            }
        }
        
        Ok(AstNode::Element {
            element_type: element.source_element_name.clone(),
            properties,
            pseudo_selectors: Vec::new(), // TODO: Convert state property sets back
            children,
        })
    }
    
    fn apply_variable_substitution(&self, template: &mut AstNode, state: &mut CompilerState) -> Result<()> {
        match template {
            AstNode::Element { properties, children, .. } => {
                // Replace variable references in properties using the variable context
                for prop in properties {
                    let value_str = prop.value.to_string();
                    
                    // Check if this property should preserve template variables for runtime binding
                    let property_line = format!("{}: {}", prop.key, value_str);
                    if self.should_preserve_template_variables(&property_line, &prop.key) {
                        // Preserve template variables that start with underscore (private/template variables)
                        // Only substitute non-template variables (component properties, regular variables)
                        let substituted = self.substitute_preserving_template_variables(&value_str, state)?;
                        
                        // Re-extract template variables after partial substitution
                        let re = regex::Regex::new(r"\$([a-zA-Z_][a-zA-Z0-9_]*)").unwrap();
                        let mut template_vars = Vec::new();
                        for capture in re.captures_iter(&substituted) {
                            if let Some(var_name) = capture.get(1) {
                                let name = var_name.as_str().to_string();
                                if !template_vars.contains(&name) {
                                    template_vars.push(name);
                                }
                            }
                        }
                        prop.template_variables = template_vars;
                        prop.value = PropertyValue::String(substituted);
                    } else {
                        // Normal substitution for non-template properties
                        let substituted = state.variable_context.substitute_variables(&value_str)?;
                        prop.value = PropertyValue::String(substituted);
                    }
                }
                
                // Recursively process children
                for child in children {
                    self.apply_variable_substitution(child, state)?;
                }
            }
            _ => {}
        }
        
        Ok(())
    }
    
    /// Check if a property should preserve template variables for runtime binding
    fn should_preserve_template_variables(&self, _property_line: &str, property_key: &str) -> bool {
        // Properties that support runtime template binding
        let template_properties = [
            "text", "value", "placeholder", "title", "label", "content", "visible"
        ];
        
        template_properties.contains(&property_key)
    }
    
    /// Substitute variables while preserving template variables (those starting with _)
    fn substitute_preserving_template_variables(&self, input: &str, state: &mut CompilerState) -> Result<String> {
        let mut result = input.to_string();
        
        // Support both $variable and ${variable} syntax
        let var_regex = regex::Regex::new(r"\$\{([a-zA-Z_][a-zA-Z0-9_]*)\}|\$([a-zA-Z_][a-zA-Z0-9_]*)")
            .map_err(|e| CompilerError::variable_legacy(0, format!("Regex error: {}", e)))?;
        
        // Process both $variable and ${variable} syntax
        let matches: Vec<_> = var_regex.captures_iter(input).collect();
        for captures in matches {
            if let Some(var_match) = captures.get(0) {
                // Check which capture group matched (1 for ${variable}, 2 for $variable)
                let var_name = if let Some(braced_var) = captures.get(1) {
                    braced_var.as_str()
                } else if let Some(simple_var) = captures.get(2) {
                    simple_var.as_str()
                } else {
                    continue;
                };
                
                // Only substitute non-template variables (template variables start with _)
                if !var_name.starts_with('_') {
                    if let Some(var_entry) = state.variable_context.get_variable(var_name) {
                        result = result.replace(var_match.as_str(), &var_entry.value);
                    } else {
                        return Err(CompilerError::variable_legacy(
                            0,
                            format!("Undefined variable: {}", var_name)
                        ));
                    }
                }
                // Template variables (starting with _) are left as-is for runtime binding
            }
        }
        
        Ok(result)
    }
    
    fn inject_slot_content(&self, template: &mut AstNode, slot_content: &[AstNode]) -> Result<()> {
        // Find content slot in template (element with id="content_slot" or similar)
        if let Some(slot_element) = self.find_content_slot(template) {
            // Add slot content as children
            if let AstNode::Element { children, .. } = slot_element {
                children.extend_from_slice(slot_content);
            }
        } else {
            // No explicit slot found - append to root element
            if let AstNode::Element { children, .. } = template {
                children.extend_from_slice(slot_content);
            }
        }
        
        Ok(())
    }
    
    fn find_content_slot<'a>(&self, element: &'a mut AstNode) -> Option<&'a mut AstNode> {
        let is_slot = if let AstNode::Element { properties, .. } = element {
            properties.iter().any(|prop| {
                prop.key == "id"
                    && (prop.cleaned_value() == "content_slot"
                        || prop.cleaned_value() == "slot"
                        || prop.cleaned_value().contains("slot"))
            })
        } else {
            false
        };

        if is_slot {
            return Some(element);
        }

        if let AstNode::Element { children, .. } = element {
            for child in children {
                if let Some(slot) = self.find_content_slot(child) {
                    return Some(slot);
                }
            }
        }

        None
    }

    
    fn update_component_statistics(&self, state: &mut CompilerState) {
        // Update header flags if components were used
        if !state.component_defs.is_empty() {
            state.header_flags |= FLAG_HAS_COMPONENT_DEFS;
        }
        
        // Calculate component definition sizes
        for component in &mut state.component_defs {
            let mut size = 2u32; // name_index + property_count
            
            // Add property definitions size
            for prop_def in &component.properties {
                size += 3; // name_index + type_hint + default_value_length
                size += prop_def.default_value.len() as u32;
            }
            
            component.calculated_size = size;
        }
    }
    
    /// Validate component definitions for correctness
    pub fn validate_component_definitions(&self, state: &CompilerState) -> Result<()> {
        for component in &state.component_defs {
            // Check for valid property types
            for prop_def in &component.properties {
                self.validate_property_type(&prop_def.value_type_hint, &component.name)?;
            }
            
            // Check that component has a template
            if component.definition_root_element_index.is_none() {
                return Err(CompilerError::component_legacy(
                    component.definition_start_line,
                    format!("Component '{}' has no template element defined", component.name)
                ));
            }
        }
        
        Ok(())
    }
    
    fn validate_property_type(&self, value_type: &ValueType, component_name: &str) -> Result<()> {
        match value_type {
            ValueType::String | ValueType::Int | ValueType::Float | 
            ValueType::Bool | ValueType::Color | ValueType::Resource => {
                // Basic types are always valid
                Ok(())
            }
            ValueType::StyleId => {
                // Style references should be validated against available styles
                // This would require access to the style table
                Ok(())
            }
            ValueType::Enum => {
                // Enum types need validation of their values
                // This would require parsing the enum definition
                Ok(())
            }
            ValueType::Custom => {
                log::warn!("Component '{}' uses custom property type", component_name);
                Ok(())
            }
            _ => {
                Err(CompilerError::component_legacy(
                    0,
                    format!("Invalid property type in component '{}'", component_name)
                ))
            }
        }
    }
    
    /// Get statistics about component usage
    pub fn get_component_stats(&self, state: &CompilerState) -> ComponentStats {
        let mut stats = ComponentStats {
            total_definitions: state.component_defs.len(),
            total_instantiations: 0,
            max_instantiation_depth: 0,
            definitions_by_complexity: HashMap::new(),
        };
        
        // Count component usage in element tree
        for element in &state.elements {
            if element.is_component_instance {
                stats.total_instantiations += 1;
            }
        }
        
        // Analyze component complexity
        for component in &state.component_defs {
            let complexity = self.calculate_component_complexity(component, state);
            stats.definitions_by_complexity.insert(component.name.clone(), complexity);
        }
        
        stats
    }
    
    fn calculate_component_complexity(&self, component: &ComponentDefinition, state: &CompilerState) -> ComponentComplexity {
        let mut complexity = ComponentComplexity {
            property_count: component.properties.len(),
            template_element_count: 0,
            max_nesting_depth: 0,
            has_slot_content: false,
        };
        
        // Analyze template complexity
        if let Some(root_index) = component.definition_root_element_index {
            if let Some(root_element) = state.elements.get(root_index) {
                complexity.template_element_count = self.count_template_elements(root_element, state);
                complexity.max_nesting_depth = self.calculate_nesting_depth(root_element, state, 0);
                complexity.has_slot_content = self.has_slot_markers(root_element);
            }
        }
        
        complexity
    }
    
    fn count_template_elements(&self, element: &Element, state: &CompilerState) -> usize {
        let mut count = 1; // This element
        
        for &child_index in &element.children {
            if let Some(child) = state.elements.get(child_index) {
                count += self.count_template_elements(child, state);
            }
        }
        
        count
    }
    
    fn calculate_nesting_depth(&self, element: &Element, state: &CompilerState, current_depth: usize) -> usize {
        let mut max_depth = current_depth;
        
        for &child_index in &element.children {
            if let Some(child) = state.elements.get(child_index) {
                let child_depth = self.calculate_nesting_depth(child, state, current_depth + 1);
                max_depth = max_depth.max(child_depth);
            }
        }
        
        max_depth
    }
    
    fn has_slot_markers(&self, element: &Element) -> bool {
        // Check if element has id containing "slot"
        for prop in &element.source_properties {
            if prop.key == "id" && prop.value.contains("slot") {
                return true;
            }
        }
        
        false
    }
    
    /// Resolve component function templates with current variable context
    fn resolve_component_function_templates(
        &self, 
        component_name: &str, 
        state: &mut CompilerState
    ) -> Result<()> {
        
        // Find all function templates for this component
        let component_templates: Vec<_> = state.function_templates
            .iter()
            .filter(|template| {
                matches!(&template.scope, FunctionScope::Component(name) if name == component_name)
            })
            .cloned()
            .collect();
        
        // DEBUG: Resolving function templates for component
        
        for template in component_templates {
            // DEBUG: Checking template
            
            // Check if all required variables are available
            let mut all_vars_available = true;
            for var_name in &template.required_vars {
                if !state.variable_context.has_variable(var_name) {
                    all_vars_available = false;
                    break;
                }
            }
            
            if !all_vars_available {
                continue; // Skip this template
            }
            
            // Workaround for parser bug: convert $id_prefix_toggle to ${id_prefix}_toggle
            let fixed_name_pattern = if template.name_pattern.starts_with("$id_prefix_") {
                template.name_pattern.replace("$id_prefix_", "${id_prefix}_")
            } else {
                template.name_pattern.clone()
            };
            
            // Also fix the function body which may contain the malformed function declaration
            let fixed_body = template.body.replace("$id_prefix_", "${id_prefix}_");
            
            // DEBUG: Applying parser bug workaround
            
            // Resolve the template with current variable context
            let resolved_name = state.variable_context.substitute_variables(&fixed_name_pattern)?;
            let resolved_body = state.variable_context.substitute_variables(&fixed_body)?;
            
            println!("✅ Resolved function: '{}' for component '{}'", resolved_name, component_name);
            
            // Build complete function code
            let param_list = template.parameters.join(", ");
            let code = format!(
                "function {}({})\n{}\nend",
                resolved_name,
                param_list,
                resolved_body
            );
            
            // Create instance context identifier  
            let instance_context = format!("{}:{}", component_name, resolved_name);
            
            let resolved_function = ResolvedFunction {
                name: resolved_name.clone(),
                code,
                template_id: template.id,
                instance_context: Some(instance_context.clone()),
                language: template.language.clone(),
                parameters: template.parameters.clone(),
            };
            
            // Add to resolved functions
            state.resolved_functions.insert(resolved_name.clone(), resolved_function);
            
            // Track component functions
            state.component_functions
                .entry(instance_context)
                .or_insert_with(Vec::new)
                .push(resolved_name);
        }
        
        Ok(())
    }
    
    /// Process deferred component scripts with variable substitution
    fn process_component_scripts(
        &self,
        component_name: &str,
        state: &mut CompilerState
    ) -> Result<()> {
        println!("DEBUG: Processing component scripts for: {}", component_name);
        println!("DEBUG: Available component scripts: {:?}", state.component_scripts.keys().collect::<Vec<_>>());
        
        // Get deferred scripts for this component
        if let Some(script_nodes) = state.component_scripts.get(component_name).cloned() {
            println!("DEBUG: Found {} stored scripts for component {}", script_nodes.len(), component_name);
            for (i, script_node) in script_nodes.iter().enumerate() {
                if matches!(script_node, AstNode::Script { .. }) {
                    println!("DEBUG: Processing script {} for component {}", i, component_name);
                    // Process the script with current variable context (component variables available)
                    let script_processor = ScriptProcessor::new();
                    let script_entry = script_processor.process_script(&script_node, state)?;
                    state.scripts.push(script_entry);
                    println!("DEBUG: Successfully processed script {} for component {}", i, component_name);
                }
            }
        } else {
            println!("DEBUG: No stored scripts found for component {}", component_name);
        }
        
        Ok(())
    }
    
    /// Expand @for loop into multiple elements
    fn expand_for_loop(
        &mut self,
        element: &mut AstNode,
        index_variable: Option<&str>,
        variable: &str,
        collection: &str,
        body: &Vec<AstNode>,
        state: &mut CompilerState,
    ) -> Result<()> {
        // Parse the collection - can be a comma-separated list or a property reference
        let items: Vec<String> = if collection.contains(',') {
            // Comma-separated list: "Option 1,Option 2,Option 3"
            collection.split(',')
                .map(|item| item.trim().to_string())
                .collect()
        } else {
            // Property reference - look up in variable context
            // Strip $ prefix if present for variable lookup
            let lookup_name = if collection.starts_with('$') {
                &collection[1..]
            } else {
                collection
            };
            match state.variable_context.get_variable(lookup_name) {
                Some(var_entry) => {
                    println!("DEBUG: Found variable '{}' with value '{}'", lookup_name, var_entry.value);
                    let var_value = &var_entry.value;
                    
                    // Handle array syntax like ["apple", "banana", "orange"]
                    if var_value.starts_with('[') && var_value.ends_with(']') {
                        // Parse array syntax
                        let array_content = &var_value[1..var_value.len()-1]; // Remove [ and ]
                        if array_content.is_empty() {
                            vec![]
                        } else {
                            array_content.split(',')
                                .map(|item| {
                                    let trimmed = item.trim();
                                    // Remove surrounding quotes if present
                                    if (trimmed.starts_with('"') && trimmed.ends_with('"')) ||
                                       (trimmed.starts_with('\'') && trimmed.ends_with('\'')) {
                                        trimmed[1..trimmed.len()-1].to_string()
                                    } else {
                                        trimmed.to_string()
                                    }
                                })
                                .collect()
                        }
                    } else if var_value.contains(',') {
                        // If it's a comma-separated string, split it
                        var_value.split(',')
                            .map(|item| item.trim().to_string())
                            .collect()
                    } else {
                        // Single item
                        vec![var_value.clone()]
                    }
                }
                None => {
                    // If variable not found, try to use the collection name as a literal comma-separated list
                    // This allows for fallback when component properties aren't resolved yet
                    if collection.contains(',') {
                        collection.split(',')
                            .map(|item| item.trim().to_string())
                            .collect()
                    } else {
                        // Use the collection name as a single item
                        vec![collection.to_string()]
                    }
                }
            }
        };
        
        // Generate elements for each item in the collection
        let mut expanded_elements = Vec::new();
        
        for (index, item) in items.iter().enumerate() {
            // Push a new scope for this iteration
            state.variable_context.push_scope(VariableScope::Function);
            
            // Add the loop variable
            state.variable_context.add_string_variable(
                variable.to_string(),
                item.clone(),
                state.current_file_path.clone(),
                0,
            )?;
            
            // Add the index variable (either explicit or auto-generated)
            let index_var_name = if let Some(idx_var) = index_variable {
                // Use explicit index variable name
                idx_var.to_string()
            } else {
                // Use auto-generated index variable for backward compatibility
                format!("{}_index", variable)
            };
            
            state.variable_context.add_string_variable(
                index_var_name,
                (index + 1).to_string(),
                state.current_file_path.clone(),
                0,
            )?;
            
            state.variable_context.add_string_variable(
                format!("{}_index0", variable),
                index.to_string(),
                state.current_file_path.clone(),
                0,
            )?;
            
            // Clone and process the body for this iteration
            for body_element in body {
                let mut element_clone = body_element.clone();
                // Apply variable substitution for all variables (component + iteration)
                self.apply_variable_substitution(&mut element_clone, state)?;
                // Don't recursively resolve components yet - just create the elements
                expanded_elements.push(element_clone);
            }
            
            // Pop the iteration scope
            state.variable_context.pop_scope()?;
        }
        
        // Replace the @for node with a container holding all expanded elements
        *element = AstNode::Element {
            element_type: "Container".to_string(),
            properties: vec![],
            pseudo_selectors: vec![],
            children: expanded_elements,
        };
        
        Ok(())
    }
    
    /// Expand @if conditional into appropriate elements
    fn expand_if_conditional(
        &mut self,
        element: &mut AstNode,
        condition: &str,
        then_body: &Vec<AstNode>,
        elif_branches: &Vec<(String, Vec<AstNode>)>,
        else_body: &Option<Vec<AstNode>>,
        state: &mut CompilerState,
    ) -> Result<()> {
        // Evaluate the main condition
        let condition_result = self.evaluate_condition(condition, state)?;
        
        let chosen_body = if condition_result {
            // Main condition is true
            Some(then_body.as_slice())
        } else {
            // Check elif branches
            let mut found_true_elif = false;
            let mut elif_body = None;
            
            for (elif_condition, elif_body_ref) in elif_branches {
                if self.evaluate_condition(elif_condition, state)? {
                    elif_body = Some(elif_body_ref.as_slice());
                    found_true_elif = true;
                    break;
                }
            }
            
            if found_true_elif {
                elif_body
            } else {
                // Use else body if available
                else_body.as_ref().map(|v| v.as_slice())
            }
        };
        
        // Generate elements based on the chosen body
        let expanded_elements = if let Some(body) = chosen_body {
            let mut elements = Vec::new();
            for body_element in body {
                let mut element_clone = body_element.clone();
                self.apply_variable_substitution(&mut element_clone, state)?;
                self.resolve_element_components(&mut element_clone, state)?;
                elements.push(element_clone);
            }
            elements
        } else {
            // No condition matched and no else body - return empty
            Vec::new()
        };
        
        // Replace the @if node with a container holding the chosen elements
        *element = AstNode::Element {
            element_type: "Container".to_string(),
            properties: vec![],
            pseudo_selectors: vec![],
            children: expanded_elements,
        };
        
        Ok(())
    }
    
    /// Evaluate a condition string - for now, just basic variable truthiness
    fn evaluate_condition(&self, condition: &str, state: &CompilerState) -> Result<bool> {
        // Simple condition evaluation - check if variable exists and is truthy
        if let Some(var_entry) = state.variable_context.get_variable(condition) {
            // Check for common falsy values
            let result = match var_entry.value.to_lowercase().as_str() {
                "false" | "0" | "" | "null" | "undefined" => false,
                _ => true,
            };
            Ok(result)
        } else {
            // Variable doesn't exist - treat as false
            Ok(false)
        }
    }
}

#[derive(Debug, Clone)]
pub struct ComponentStats {
    pub total_definitions: usize,
    pub total_instantiations: usize,
    pub max_instantiation_depth: usize,
    pub definitions_by_complexity: HashMap<String, ComponentComplexity>,
}

#[derive(Debug, Clone)]
pub struct ComponentComplexity {
    pub property_count: usize,
    pub template_element_count: usize,
    pub max_nesting_depth: usize,
    pub has_slot_content: bool,
}

impl ComponentStats {
    pub fn print_summary(&self) {
        println!("Component Statistics:");
        println!("  Definitions: {}", self.total_definitions);
        println!("  Instantiations: {}", self.total_instantiations);
        println!("  Max depth: {}", self.max_instantiation_depth);
        
        if !self.definitions_by_complexity.is_empty() {
            println!("  Complexity breakdown:");
            for (name, complexity) in &self.definitions_by_complexity {
                println!("    {}: {} props, {} elements, depth {}{}",
                        name,
                        complexity.property_count,
                        complexity.template_element_count,
                        complexity.max_nesting_depth,
                        if complexity.has_slot_content { " (slotted)" } else { "" });
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_variable_substitution() {
        let resolver = ComponentResolver::new();
        let mut mapping = HashMap::new();
        mapping.insert("title".to_string(), "\"Hello World\"".to_string());
        mapping.insert("count".to_string(), "42".to_string());
        
        let result = resolver.substitute_variables("text: $title, value: $count", &mapping).unwrap();
        assert_eq!(result, "text: \"Hello World\", value: 42");
    }
    
    #[test]
    fn test_undefined_variable_error() {
        let resolver = ComponentResolver::new();
        let mapping = HashMap::new();
        
        let result = resolver.substitute_variables("text: $undefined", &mapping);
        assert!(result.is_err());
    }
    
    #[test]
    fn test_component_complexity_calculation() {
        let resolver = ComponentResolver::new();
        let component_def = ComponentDefinition {
            name: "TestComponent".to_string(),
            properties: vec![
                ComponentPropertyDef {
                    name: "title".to_string(),
                    value_type_hint: ValueType::String,
                    default_value: "Default".to_string(),
                },
                ComponentPropertyDef {
                    name: "count".to_string(),
                    value_type_hint: ValueType::Int,
                    default_value: "0".to_string(),
                },
            ],
            definition_start_line: 1,
            definition_root_element_index: None,
            calculated_size: 0,
            internal_template_element_offsets: HashMap::new(),
        };
        
        let state = CompilerState::new();
        let complexity = resolver.calculate_component_complexity(&component_def, &state);
        
        assert_eq!(complexity.property_count, 2);
        assert_eq!(complexity.template_element_count, 0); // No template
    }
}
