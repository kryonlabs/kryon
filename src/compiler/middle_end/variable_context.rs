//! Unified variable tracking and substitution system
//! 
//! This module provides a centralized variable context that can track variables from:
//! - @variables blocks (global variables)
//! - Component properties (component-local variables) 
//! - Function parameters (function-local variables)
//! - Style calculations (computed variables)

use crate::error::{CompilerError, Result};
use crate::core::*;
use crate::compiler::middle_end::module_context::ModuleContext;
use crate::compiler::frontend::ast::{Expression, AstProperty};
use crate::types::*;
use std::collections::{HashMap, HashSet};
use std::path::PathBuf;
use regex::Regex;

/// Variable scope levels
#[derive(Debug, Clone, PartialEq)]
pub enum VariableScope {
    Global,     // @variables blocks
    Module,     // Module-level variables (with import priority)
    Component,  // Component property substitution
    Function,   // Function parameter substitution
    Style,      // Style inheritance and calculations
}

/// A variable entry with scope and metadata
#[derive(Debug, Clone)]
pub struct VariableEntry {
    pub name: String,
    pub value: String,
    pub scope: VariableScope,
    pub source_file: String,
    pub source_line: usize,
    pub value_type: ValueType,
    pub module_path: Option<PathBuf>,
    pub import_order: Option<usize>,
    pub is_private: bool,
}

/// Unified variable context for all substitution operations
#[derive(Debug, Clone)]
pub struct VariableContext {
    // Variables by scope - inner scopes shadow outer scopes
    variables: Vec<HashMap<String, VariableEntry>>,
    current_scope: VariableScope,
    
    // Stack for scope management
    scope_stack: Vec<VariableScope>,
    
    // Module-level variable management
    module_variables: HashMap<PathBuf, HashMap<String, VariableEntry>>,
    current_module: Option<PathBuf>,
    import_order_counter: usize,
}

impl VariableContext {
    pub fn new() -> Self {
        let mut context = Self {
            variables: vec![HashMap::new()], // Start with global scope
            current_scope: VariableScope::Global,
            scope_stack: Vec::new(),
            module_variables: HashMap::new(),
            current_module: None,
            import_order_counter: 0,
        };
        
        // Initialize global scope
        context.variables.push(HashMap::new());
        context
    }
    
    /// Push a new variable scope (component, function, etc.)
    pub fn push_scope(&mut self, scope: VariableScope) {
        self.scope_stack.push(self.current_scope.clone());
        self.current_scope = scope;
        self.variables.push(HashMap::new());
    }
    
    /// Pop the current scope, returning to the previous one
    pub fn pop_scope(&mut self) -> Result<()> {
        if self.variables.len() <= 1 {
            return Err(CompilerError::variable_legacy(
                0,
                "Cannot pop global variable scope".to_string()
            ));
        }
        
        self.variables.pop();
        self.current_scope = self.scope_stack.pop()
            .unwrap_or(VariableScope::Global);
        
        Ok(())
    }
    
    /// Add a variable to the current scope
    pub fn add_variable(&mut self, entry: VariableEntry) -> Result<()> {
        if let Some(current_scope_vars) = self.variables.last_mut() {
            current_scope_vars.insert(entry.name.clone(), entry);
            Ok(())
        } else {
            Err(CompilerError::variable_legacy(
                0,
                "No variable scope available".to_string()
            ))
        }
    }
    
    /// Add a simple string variable to current scope
    pub fn add_string_variable(&mut self, name: String, value: String, source_file: String, source_line: usize) -> Result<()> {
        let entry = VariableEntry {
            name: name.clone(),
            value,
            scope: self.current_scope.clone(),
            source_file,
            source_line,
            value_type: ValueType::String,
            module_path: self.current_module.clone(),
            import_order: None,
            is_private: name.starts_with('_'),
        };
        self.add_variable(entry)
    }
    
    /// Look up a variable by name, searching from current scope to global with module priority
    pub fn get_variable(&self, name: &str) -> Option<&VariableEntry> {
        // First, search in current scopes (Component, Function, Style)
        for scope_vars in self.variables.iter().rev() {
            if let Some(var) = scope_vars.get(name) {
                // Local definitions always override imports
                if var.scope != VariableScope::Module {
                    return Some(var);
                }
            }
        }
        
        // Then search in module variables with import priority
        // Current module variables override imported ones
        if let Some(current_module) = &self.current_module {
            if let Some(module_vars) = self.module_variables.get(current_module) {
                if let Some(var) = module_vars.get(name) {
                    return Some(var);
                }
            }
        }
        
        // Finally, search in imported module variables by import order (later imports override earlier ones)
        let mut best_match: Option<&VariableEntry> = None;
        let mut best_import_order = 0;
        
        for module_vars in self.module_variables.values() {
            if let Some(var) = module_vars.get(name) {
                if let Some(import_order) = var.import_order {
                    if best_match.is_none() || import_order >= best_import_order {
                        best_match = Some(var);
                        best_import_order = import_order;
                    }
                }
            }
        }
        
        if best_match.is_some() {
            return best_match;
        }
        
        // Finally, search in regular scopes for any remaining variables
        for scope_vars in self.variables.iter().rev() {
            if let Some(var) = scope_vars.get(name) {
                return Some(var);
            }
        }
        
        None
    }
    
    /// Check if a variable exists in any scope
    pub fn has_variable(&self, name: &str) -> bool {
        self.get_variable(name).is_some()
    }
    
    /// Substitute all $variable and ${variable} references in a string
    pub fn substitute_variables(&self, input: &str) -> Result<String> {
        self.substitute_variables_with_context(input, false)
    }
    
    /// Substitute variables with script context awareness
    pub fn substitute_variables_for_script(&self, input: &str) -> Result<String> {
        self.substitute_variables_with_context(input, true)
    }
    
    /// Context-aware variable substitution
    fn substitute_variables_with_context(&self, input: &str, is_script_context: bool) -> Result<String> {
        let mut result = input.to_string();
        
        // Support both $variable and ${variable} syntax
        let var_regex = Regex::new(r"\$\{([a-zA-Z_][a-zA-Z0-9_]*)\}|\$([a-zA-Z_][a-zA-Z0-9_]*)")
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
                
                if let Some(var_entry) = self.get_variable(var_name) {
                    let replacement = if is_script_context && self.is_template_variable_in_assignment_context(input, var_match.start()) {
                        // In script context, for template variable assignments, generate proper function calls
                        if self.is_template_variable_assignment(input, var_match.start()) {
                            // For assignment, we need to handle the entire assignment statement differently
                            // Return a placeholder for now - we'll handle the full assignment replacement later
                            format!("__TEMPLATE_WRITE__{}__", var_name)
                        } else {
                            // This is a template variable read (right side of assignment, in expressions, etc.)
                            // No $ prefix in scripts - direct variable access
                            format!("{}", var_name)
                        }
                    } else {
                        // Regular substitution - replace with literal value
                        var_entry.value.clone()
                    };
                    
                    result = result.replace(var_match.as_str(), &replacement);
                } else {
                    println!("DEBUG: Variable substitution failed for '{}' in context:", var_name);
                    println!("  Input string: {}", input);
                    println!("  Current scope: {:?}", self.current_scope);
                    println!("  Available variables in all scopes:");
                    for var in self.get_all_variables() {
                        println!("    {} = {} (scope: {:?})", var.name, var.value, var.scope);
                    }
                    return Err(CompilerError::variable_legacy(
                        0,
                        format!("Undefined variable: {}", var_name)
                    ));
                }
            }
        }
        
        // Second pass: handle template assignment placeholders
        if is_script_context {
            result = self.process_template_assignments(result)?;
        }
        
        Ok(result)
    }
    
    /// Process template assignment placeholders and convert them to direct variable access
    fn process_template_assignments(&self, input: String) -> Result<String> {
        let mut result = input;
        
        // Look for assignment patterns with our write placeholders
        let assignment_regex = Regex::new(r"__TEMPLATE_WRITE__([a-zA-Z_][a-zA-Z0-9_]*)__\s*=\s*([^\n\r]+)")
            .map_err(|e| CompilerError::variable_legacy(0, format!("Regex error: {}", e)))?;
        
        result = assignment_regex.replace_all(&result, |caps: &regex::Captures| {
            let var_name = &caps[1];
            let value_expr = caps[2].trim();
            // Direct assignment to template variable (no $ prefix in scripts)
            format!("{} = {}", var_name, value_expr)
        }).to_string();
        
        // Replace any remaining read placeholders with direct variable access
        let read_placeholder_regex = Regex::new(r"__TEMPLATE_WRITE__([a-zA-Z_][a-zA-Z0-9_]*)__")
            .map_err(|e| CompilerError::variable_legacy(0, format!("Regex error: {}", e)))?;
        
        result = read_placeholder_regex.replace_all(&result, |caps: &regex::Captures| {
            let var_name = &caps[1];
            // Direct variable access (no $ prefix in scripts)
            format!("{}", var_name)
        }).to_string();
        
        Ok(result)
    }
    
    /// Check if this is a template variable context that needs special handling
    fn is_template_variable_in_assignment_context(&self, input: &str, var_pos: usize) -> bool {
        // Look for patterns that suggest this is a template variable being used in a script
        
        // Check if we're in a function context
        if input.contains("function ") {
            return true;
        }
        
        // Check for Lua-style script patterns
        if input.contains(" = ") || input.contains("not ") || 
           input.contains("if ") || input.contains("then") ||
           input.contains("print(") || input.contains("local ") {
            return true;
        }
        
        false
    }
    
    /// Check if this is a template variable assignment (left side of =)
    fn is_template_variable_assignment(&self, input: &str, var_pos: usize) -> bool {
        // Find the end of the variable name
        let var_start = var_pos;
        let var_end = input[var_start..].find(|c: char| !c.is_alphanumeric() && c != '_' && c != '$')
            .map(|pos| var_start + pos)
            .unwrap_or(input.len());
        
        let after_var = &input[var_end..];
        
        // Check if there's an assignment operator after the variable
        let trimmed_after = after_var.trim_start();
        trimmed_after.starts_with("=") && !trimmed_after.starts_with("==") && !trimmed_after.starts_with("!=")
    }
    
    /// Check if this is a template variable read (right side of =, or in expressions)
    fn is_template_variable_read(&self, input: &str, var_pos: usize) -> bool {
        let before_var = &input[..var_pos];
        
        // Check for read contexts like "= not $var" or "if $var" etc.
        if before_var.contains("= ") || before_var.contains("not ") || 
           before_var.contains("if ") || before_var.contains("(") {
            return true;
        }
        
        true // Default to read context
    }
    
    /// Evaluate an expression with variable substitution
    pub fn evaluate_expression(&self, expr: &Expression) -> Result<String> {
        match expr {
            Expression::String(s) => Ok(s.clone()),
            Expression::Number(n) => Ok(n.to_string()),
            Expression::Integer(i) => Ok(i.to_string()),
            Expression::Boolean(b) => Ok(b.to_string()),
            Expression::Variable(var_name) => {
                if let Some(var_entry) = self.get_variable(var_name) {
                    Ok(var_entry.value.clone())
                } else {
                    Err(CompilerError::variable_legacy(
                        0,
                        format!("Undefined variable: ${}", var_name)
                    ))
                }
            }
            Expression::NotEquals(left, right) => {
                let left_val = self.evaluate_expression(left)?;
                let right_val = self.evaluate_expression(right)?;
                Ok((left_val != right_val).to_string())
            }
            Expression::EqualEquals(left, right) => {
                let left_val = self.evaluate_expression(left)?;
                let right_val = self.evaluate_expression(right)?;
                Ok((left_val == right_val).to_string())
            }
            Expression::LessThan(left, right) => {
                let left_val = self.evaluate_expression(left)?;
                let right_val = self.evaluate_expression(right)?;
                // Try to parse as numbers for comparison
                if let (Ok(left_num), Ok(right_num)) = (left_val.parse::<f64>(), right_val.parse::<f64>()) {
                    Ok((left_num < right_num).to_string())
                } else {
                    Ok((left_val < right_val).to_string())
                }
            }
            Expression::LessThanOrEqual(left, right) => {
                let left_val = self.evaluate_expression(left)?;
                let right_val = self.evaluate_expression(right)?;
                if let (Ok(left_num), Ok(right_num)) = (left_val.parse::<f64>(), right_val.parse::<f64>()) {
                    Ok((left_num <= right_num).to_string())
                } else {
                    Ok((left_val <= right_val).to_string())
                }
            }
            Expression::GreaterThan(left, right) => {
                let left_val = self.evaluate_expression(left)?;
                let right_val = self.evaluate_expression(right)?;
                if let (Ok(left_num), Ok(right_num)) = (left_val.parse::<f64>(), right_val.parse::<f64>()) {
                    Ok((left_num > right_num).to_string())
                } else {
                    Ok((left_val > right_val).to_string())
                }
            }
            Expression::GreaterThanOrEqual(left, right) => {
                let left_val = self.evaluate_expression(left)?;
                let right_val = self.evaluate_expression(right)?;
                if let (Ok(left_num), Ok(right_num)) = (left_val.parse::<f64>(), right_val.parse::<f64>()) {
                    Ok((left_num >= right_num).to_string())
                } else {
                    Ok((left_val >= right_val).to_string())
                }
            }
            Expression::Ternary { condition, true_value, false_value } => {
                let condition_result = self.evaluate_expression(condition)?;
                let is_true = match condition_result.as_str() {
                    "true" => true,
                    "false" => false,
                    "" => false, // Empty string is falsy
                    _ => true,   // Non-empty string is truthy
                };
                
                if is_true {
                    self.evaluate_expression(true_value)
                } else {
                    self.evaluate_expression(false_value)
                }
            }
        }
    }
    
    /// Get all variables in current scope (for debugging)
    pub fn get_current_scope_variables(&self) -> Vec<String> {
        if let Some(current_vars) = self.variables.last() {
            current_vars.keys().cloned().collect()
        } else {
            Vec::new()
        }
    }
    
    /// Get all variables in all scopes (for debugging)
    pub fn get_all_variables(&self) -> Vec<&VariableEntry> {
        let mut all_vars = Vec::new();
        for scope_vars in &self.variables {
            for var in scope_vars.values() {
                all_vars.push(var);
            }
        }
        all_vars
    }
    
    /// Create a property mapping for component instantiation
    pub fn create_component_property_mapping(&self, component_props: &[ComponentPropertyDef], instance_props: &[AstProperty]) -> Result<HashMap<String, String>> {
        let mut mapping = HashMap::new();
        
        // Start with default values from component definition
        for prop_def in component_props {
            mapping.insert(prop_def.name.clone(), prop_def.default_value.clone());
        }
        
        // Override with instance-provided values
        for instance_prop in instance_props {
            if component_props.iter().any(|p| p.name == instance_prop.key) {
                // Substitute any variables in the instance property value
                let substituted_value = self.substitute_variables(&instance_prop.value.to_string())?;
                mapping.insert(instance_prop.key.clone(), substituted_value);
            } else {
                // Allow unknown properties to pass through but warn
                log::warn!("Component property '{}' not defined in component schema", instance_prop.key);
                let substituted_value = self.substitute_variables(&instance_prop.value.to_string())?;
                mapping.insert(instance_prop.key.clone(), substituted_value);
            }
        }
        
        Ok(mapping)
    }
    
    /// Set the current module context for variable resolution
    pub fn set_current_module(&mut self, module_path: PathBuf) {
        self.current_module = Some(module_path);
    }
    
    /// Import variables from a module with override priority
    pub fn import_module_variables(&mut self, module: &ModuleContext, import_order: usize) -> Result<()> {
        let module_path = module.file_path.clone();
        
        // Get public variables from the module
        let public_variables = module.get_public_variables();
        
        // Convert to VariableEntry format
        let mut variable_entries = HashMap::new();
        for (name, var_def) in public_variables {
            let entry = VariableEntry {
                name: name.clone(),
                value: var_def.value.clone(),
                scope: VariableScope::Module,
                source_file: module_path.to_string_lossy().to_string(),
                source_line: var_def.def_line,
                value_type: ValueType::String, // Default to string for now
                module_path: Some(module_path.clone()),
                import_order: Some(import_order),
                is_private: false, // Only public variables are imported
            };
            variable_entries.insert(name, entry);
        }
        
        // Store the module variables
        self.module_variables.insert(module_path, variable_entries);
        
        Ok(())
    }
    
    /// Add variables from a module context to the current context
    pub fn add_module_variables(&mut self, module: &ModuleContext) -> Result<()> {
        for (name, var_def) in &module.variables {
            // Skip private variables unless we're in the same module
            if module.is_private(name) && self.current_module.as_ref() != Some(&module.file_path) {
                continue;
            }
            
            let entry = VariableEntry {
                name: name.clone(),
                value: var_def.value.clone(),
                scope: VariableScope::Module,
                source_file: module.file_path.to_string_lossy().to_string(),
                source_line: var_def.def_line,
                value_type: ValueType::String,
                module_path: Some(module.file_path.clone()),
                import_order: module.dependency_order,
                is_private: module.is_private(name),
            };
            
            self.add_variable(entry)?;
        }
        
        Ok(())
    }
}

impl Default for VariableContext {
    fn default() -> Self {
        Self::new()
    }
}

/// Variable processor for handling @variables blocks
pub struct VariableProcessor {
    var_usage_regex: Regex,
    var_expression_regex: Regex,
}


impl VariableProcessor {
    pub fn new() -> Self {
        Self {
            var_usage_regex: Regex::new(r"\$([a-zA-Z_][a-zA-Z0-9_]*)").unwrap(),
            var_expression_regex: Regex::new(r#"\$([a-zA-Z_][a-zA-Z0-9_]*)\s*(==|!=|<=|>=|<|>)\s*([0-9]+|true|false|"[^"]*")"#).unwrap(),
        }
    }

    /// Process variables in source: collect, resolve, and substitute
    pub fn process_and_substitute_variables(
        &self,
        source: &str,
        state: &mut CompilerState,
    ) -> Result<String> {
        // Clear any existing variables
        state.variables.clear();

        // Phase 1: Collect raw variables
        self.collect_raw_variables(source, state)?;
        
        // Phase 2: Resolve inter-variable dependencies
        self.resolve_all_variables(state)?;
        
        // Phase 3: Substitute variables and remove @variables blocks
        self.perform_substitution_and_remove_blocks(source, state)
    }

    /// Scan source for @variables blocks and populate state.variables
    fn collect_raw_variables(&self, source: &str, state: &mut CompilerState) -> Result<()> {
        let mut in_variables_block = false;
        let mut line_num = 0;

        for line in source.lines() {
            line_num += 1;
            let trimmed = line.trim();

            // Skip full-line comments
            if trimmed.starts_with('#') {
                continue;
            }

            // Remove trailing comments
            let line_without_comment = self.strip_trailing_comment(trimmed);

            if line_without_comment == "@variables {" {
                if in_variables_block {
                    return Err(CompilerError::variable_legacy(
                        line_num,
                        "Nested @variables blocks are not allowed"
                    ));
                }
                in_variables_block = true;
                continue;
            }

            if line_without_comment == "}" && in_variables_block {
                in_variables_block = false;
                continue;
            }

            if in_variables_block && !line_without_comment.is_empty() {
                self.parse_variable_definition(line_without_comment, line_num, state)?;
            }
        }

        Ok(())
    }

    /// Parse a single variable definition line
    fn parse_variable_definition(
        &self,
        line: &str,
        line_num: usize,
        state: &mut CompilerState,
    ) -> Result<()> {
        let parts: Vec<&str> = line.splitn(2, ':').collect();
        if parts.len() != 2 {
            return Err(CompilerError::variable_legacy(
                line_num,
                format!("Invalid variable definition syntax: '{}'. Expected 'name: value'", line)
            ));
        }

        let var_name = parts[0].trim();
        let raw_value = parts[1].trim();

        if !is_valid_identifier(var_name) {
            return Err(CompilerError::variable_legacy(
                line_num,
                format!("Invalid variable name '{}'", var_name)
            ));
        }

        // Warn about redefinition
        if let Some(existing) = state.variables.get(var_name) {
            log::warn!(
                "Line {}: Variable '{}' redefined. Previous definition at line {}",
                line_num, var_name, existing.def_line
            );
        }

        state.variables.insert(var_name.to_string(), VariableDef {
            value: String::new(),
            raw_value: raw_value.to_string(),
            def_line: line_num,
            is_resolving: false,
            is_resolved: false,
        });

        Ok(())
    }

    /// Resolve all variables, handling dependencies and detecting cycles
    fn resolve_all_variables(&self, state: &mut CompilerState) -> Result<()> {
        let var_names: Vec<String> = state.variables.keys().cloned().collect();
        
        for name in var_names {
            if !state.variables[&name].is_resolved {
                self.resolve_variable(&name, state, &mut HashSet::new())?;
            }
        }

        Ok(())
    }

    /// Recursively resolve a single variable
    fn resolve_variable(
        &self,
        name: &str,
        state: &mut CompilerState,
        visited: &mut HashSet<String>,
    ) -> Result<String> {
        // Check if variable exists
        let var_def = state.variables.get(name).ok_or_else(|| {
            CompilerError::variable_legacy(0, format!("Undefined variable '{}'", name))
        })?.clone();

        if var_def.is_resolved {
            return Ok(var_def.value);
        }

        if var_def.is_resolving || visited.contains(name) {
            let cycle_path: Vec<_> = visited.iter().cloned().collect();
            return Err(CompilerError::variable_legacy(
                var_def.def_line,
                format!("Circular variable dependency detected: {} -> {}", 
                       cycle_path.join(" -> "), name)
            ));
        }

        // Mark as resolving
        visited.insert(name.to_string());
        if let Some(var_def) = state.variables.get_mut(name) {
            var_def.is_resolving = true;
        }

        // Resolve dependencies in raw_value
        let mut current_value = var_def.raw_value.clone();
        
        // Find all variable references
        for captures in self.var_usage_regex.captures_iter(&var_def.raw_value) {
            let var_ref = captures.get(0).unwrap().as_str(); // $var_name
            let ref_var_name = captures.get(1).unwrap().as_str(); // var_name
            
            // Recursively resolve the referenced variable
            let resolved_ref_value = self.resolve_variable(ref_var_name, state, visited)?;
            
            // Replace in current value
            current_value = current_value.replace(var_ref, &resolved_ref_value);
        }

        // Update the variable with resolved value
        if let Some(var_def) = state.variables.get_mut(name) {
            var_def.value = current_value.clone();
            var_def.is_resolved = true;
            var_def.is_resolving = false;
        }

        visited.remove(name);
        Ok(current_value)
    }

    /// Remove @variables blocks and substitute $varName references
    fn perform_substitution_and_remove_blocks(
        &self,
        source: &str,
        state: &CompilerState,
    ) -> Result<String> {
        let mut result = String::new();
        let mut in_variables_block = false;
        let mut in_component_block = false;
        let mut component_brace_depth = 0;
        let mut line_num = 0;
        let mut substitution_errors = Vec::new();

        for line in source.lines() {
            line_num += 1;
            let trimmed = line.trim();
            

            // Handle @variables block boundaries
            if trimmed.starts_with("@variables {") {
                in_variables_block = true;
                continue;
            }
            if in_variables_block && trimmed == "}" {
                in_variables_block = false;
                continue;
            }
            if in_variables_block {
                continue; // Skip lines inside @variables blocks
            }
            
            // Handle component Define block boundaries
            if trimmed.starts_with("Define ") && trimmed.contains("{") {
                in_component_block = true;
                component_brace_depth = 1;
                result.push_str(line);
                result.push('\n');
                continue;
            }
            if in_component_block {
                // Count braces to track nesting depth
                let open_braces = line.matches('{').count();
                let close_braces = line.matches('}').count();
                component_brace_depth += open_braces;
                component_brace_depth -= close_braces;
                
                
                result.push_str(line);
                result.push('\n');
                
                if component_brace_depth == 0 {
                    in_component_block = false;
                }
                continue;
            }

            // First, handle expressions (like $var == 0)
            let expr_substituted_line = self.var_expression_regex.replace_all(line, |caps: &regex::Captures| {
                let var_name = &caps[1];
                let operator = &caps[2];
                let value = &caps[3];
                
                match self.evaluate_expression(var_name, operator, value, state) {
                    Ok(result) => result,
                    Err(e) => {
                        substitution_errors.push(format!("Line {}: {}", line_num, e));
                        caps[0].to_string() // Return original if error
                    }
                }
            });

            // Then handle simple variable substitution on lines outside @variables blocks
            let substituted_line = self.var_usage_regex.replace_all(&expr_substituted_line, |caps: &regex::Captures| {

                let var_name = &caps[1];
                
                // Check if this is a template variable that should be preserved for runtime binding
                if self.is_template_variable_context(line, var_name) {
                    // Preserve template variables in element properties for runtime binding
                    caps[0].to_string() // Return original $variable_name
                } else {
                    match state.variables.get(var_name) {
                        Some(var_def) if var_def.is_resolved => var_def.value.clone(),
                        Some(_) => {
                            substitution_errors.push(format!(
                                "Line {}: Internal error: variable '{}' used but not resolved",
                                line_num, var_name
                            ));
                            caps[0].to_string() // Return original if error
                        }
                        None => {
                            substitution_errors.push(format!(
                                "Line {}: Undefined variable '${}'",
                                line_num, var_name
                            ));
                            caps[0].to_string() // Return original if error
                        }
                    }
                }
            });

            result.push_str(&substituted_line);
            result.push('\n');
        }

        if !substitution_errors.is_empty() {
            return Err(CompilerError::variable_legacy(0, substitution_errors.join("\n")));
        }

        Ok(result)
    }

    /// Evaluate expressions like $var == 0, $var != "test", etc.
    fn evaluate_expression(&self, var_name: &str, operator: &str, value: &str, state: &CompilerState) -> Result<String> {
        // Get the variable value
        let var_value = match state.variables.get(var_name) {
            Some(var_def) if var_def.is_resolved => &var_def.value,
            Some(_) => return Err(CompilerError::variable_legacy(0, format!("Variable '{}' not resolved", var_name))),
            None => return Err(CompilerError::variable_legacy(0, format!("Undefined variable '{}'", var_name))),
        };

        // Parse the comparison value
        let comparison_value = if value.starts_with('"') && value.ends_with('"') {
            // String value
            value[1..value.len()-1].to_string()
        } else if value == "true" || value == "false" {
            // Boolean value
            value.to_string()
        } else {
            // Numeric value
            value.to_string()
        };

        // Perform comparison
        let result = match operator {
            "==" => var_value == &comparison_value,
            "!=" => var_value != &comparison_value,
            "<" => {
                if let (Ok(var_num), Ok(comp_num)) = (var_value.parse::<f64>(), comparison_value.parse::<f64>()) {
                    var_num < comp_num
                } else {
                    var_value < &comparison_value
                }
            },
            ">" => {
                if let (Ok(var_num), Ok(comp_num)) = (var_value.parse::<f64>(), comparison_value.parse::<f64>()) {
                    var_num > comp_num
                } else {
                    var_value > &comparison_value
                }
            },
            "<=" => {
                if let (Ok(var_num), Ok(comp_num)) = (var_value.parse::<f64>(), comparison_value.parse::<f64>()) {
                    var_num <= comp_num
                } else {
                    var_value <= &comparison_value
                }
            },
            ">=" => {
                if let (Ok(var_num), Ok(comp_num)) = (var_value.parse::<f64>(), comparison_value.parse::<f64>()) {
                    var_num >= comp_num
                } else {
                    var_value >= &comparison_value
                }
            },
            _ => return Err(CompilerError::variable_legacy(0, format!("Unknown operator: {}", operator))),
        };

        Ok(result.to_string())
    }

    /// Strip trailing comments from a line, respecting quotes
    fn strip_trailing_comment<'a>(&self, line: &'a str) -> &'a str {
        let mut in_quotes = false;
        let mut comment_pos = None;

        for (i, ch) in line.char_indices() {
            if ch == '"' {
                in_quotes = !in_quotes;
            }
            if ch == '#' && !in_quotes {
                comment_pos = Some(i);
                break;
            }
        }

        if let Some(pos) = comment_pos {
            line[..pos].trim_end()
        } else {
            line
        }
    }
    
    /// Check if a variable usage is in a template context that should be preserved for runtime binding
    fn is_template_variable_context(&self, line: &str, _var_name: &str) -> bool {
        // Preserve template variables in element properties that should be runtime-bound
        // Look for patterns like 'text: $variable' or 'text: "something $variable"'
        if line.trim().starts_with("text:") {
            return true;
        }
        
        // Check for other element properties that should support template variables
        let template_properties = [
            "text:", "value:", "placeholder:", "title:", "label:", "content:", "visible:"
        ];
        
        for prop in &template_properties {
            if line.trim().starts_with(prop) {
                return true;
            }
        }
        
        false
    }
}


/// Check if a string is a valid identifier
pub fn is_valid_identifier(name: &str) -> bool {
    if name.is_empty() {
        return false;
    }
    
    let mut chars = name.chars();
    let first = chars.next().unwrap();
    
    if !first.is_ascii_alphabetic() && first != '_' {
        return false;
    }
    
    chars.all(|c| c.is_ascii_alphanumeric() || c == '_')
}
