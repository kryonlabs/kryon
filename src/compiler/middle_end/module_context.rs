//! Module context for isolated compilation
//! 
//! This module provides the core data structures for module-level isolation,
//! where each @include creates its own isolated context with its own variables,
//! styles, and components.
//! 
use crate::core::{ComponentPropertyDef, VariableDef}; 
use crate::error::{CompilerError, Result};
use std::collections::{HashMap, HashSet};
use std::path::PathBuf;

/// Context for a single module (included file)
/// Contains all the isolated state for one module
#[derive(Debug, Clone)]
pub struct ModuleContext {
    /// The processed content of the module (after includes)
    pub content: String,
    
    /// File path of this module
    pub file_path: PathBuf,
    
    /// Variables defined in this module (public and private)
    pub variables: HashMap<String, VariableDef>,
    
    /// Styles defined in this module
    pub styles: HashMap<String, StyleDef>,
    
    /// Components defined in this module
    pub components: HashMap<String, ComponentDef>,
    
    /// Scripts defined in this module
    pub scripts: Vec<ScriptDef>,
    
    /// Names of items that are private (start with _)
    pub private_items: HashSet<String>,
    
    /// Modules that this module imports
    pub imports: Vec<ModuleImport>,
    
    /// Module dependency graph position
    pub dependency_order: Option<usize>,
}

/// Information about a module import
#[derive(Debug, Clone)]
pub struct ModuleImport {
    /// Path to the imported module
    pub module_path: PathBuf,
    
    /// Items actually imported and accessible
    pub accessible_items: Vec<String>,
    
    /// Order this import was processed (for override priority)
    pub import_order: usize,
}

/// Temporary definitions for module compilation
#[derive(Debug, Clone)]
pub struct StyleDef {
    pub name: String,
    pub properties: HashMap<String, String>,
    pub extends: Option<String>,
}

#[derive(Debug, Clone)]
pub struct ComponentDef {
    pub name: String,
    pub properties: Vec<ComponentPropertyDef>,
    pub template: String,
}

#[derive(Debug, Clone)]
pub struct ScriptDef {
    pub language: String,
    pub content: String,
    pub name: Option<String>,
}

/// Module graph for dependency resolution
#[derive(Debug)]
pub struct ModuleGraph {
    /// All modules in the graph
    pub modules: HashMap<PathBuf, ModuleContext>,
    
    /// Module compilation order (resolved dependencies)
    pub compilation_order: Vec<PathBuf>,
    
    /// Root module (the main file being compiled)
    pub root_module: PathBuf,
}

impl ModuleContext {
    /// Create a new empty module context
    pub fn new(file_path: PathBuf) -> Self {
        Self {
            content: String::new(),
            file_path,
            variables: HashMap::new(),
            styles: HashMap::new(),
            components: HashMap::new(),
            scripts: Vec::new(),
            private_items: HashSet::new(),
            imports: Vec::new(),
            dependency_order: None,
        }
    }
    
    /// Add a variable to this module, checking for privacy
    pub fn add_variable(&mut self, name: String, value: VariableDef) {
        if name.starts_with('_') {
            self.private_items.insert(name.clone());
        }
        self.variables.insert(name, value);
    }
    
    /// Add a style to this module, checking for privacy
    pub fn add_style(&mut self, name: String, style: StyleDef) {
        if name.starts_with('_') {
            self.private_items.insert(name.clone());
        }
        self.styles.insert(name, style);
    }
    
    /// Add a component to this module, checking for privacy
    pub fn add_component(&mut self, name: String, component: ComponentDef) {
        if name.starts_with('_') {
            self.private_items.insert(name.clone());
        }
        self.components.insert(name, component);
    }
    
    /// Check if an item is private to this module
    pub fn is_private(&self, item_name: &str) -> bool {
        self.private_items.contains(item_name)
    }
    
    /// Get all public items from this module
    pub fn get_public_variables(&self) -> HashMap<String, VariableDef> {
        self.variables.iter()
            .filter(|(name, _)| !self.is_private(name))
            .map(|(name, value)| (name.clone(), value.clone()))
            .collect()
    }
    
    /// Get all public styles from this module
    pub fn get_public_styles(&self) -> HashMap<String, StyleDef> {
        self.styles.iter()
            .filter(|(name, _)| !self.is_private(name))
            .map(|(name, style)| (name.clone(), style.clone()))
            .collect()
    }
    
    /// Get all public components from this module
    pub fn get_public_components(&self) -> HashMap<String, ComponentDef> {
        self.components.iter()
            .filter(|(name, _)| !self.is_private(name))
            .map(|(name, comp)| (name.clone(), comp.clone()))
            .collect()
    }
    
    /// Merge imported module into this one, respecting privacy and override rules
    pub fn import_module(&mut self, imported_module: &ModuleContext, import_order: usize) -> Result<()> {
        let mut accessible_items = Vec::new();
        
        // Import public variables (local definitions override imported ones)
        for (name, value) in imported_module.get_public_variables() {
            if !self.variables.contains_key(&name) {
                self.variables.insert(name.clone(), value);
                accessible_items.push(name);
            }
            // If local definition exists, it overrides the imported one (no-op)
        }
        
        // Import public styles (local definitions override imported ones)
        for (name, style) in imported_module.get_public_styles() {
            if !self.styles.contains_key(&name) {
                self.styles.insert(name.clone(), style);
                accessible_items.push(name);
            }
        }
        
        // Import public components (local definitions override imported ones)
        for (name, component) in imported_module.get_public_components() {
            if !self.components.contains_key(&name) {
                self.components.insert(name.clone(), component);
                accessible_items.push(name);
            }
        }
        
        // Record the import
        let import = ModuleImport {
            module_path: imported_module.file_path.clone(),
            accessible_items,
            import_order,
        };
        self.imports.push(import);
        
        Ok(())
    }
}

impl ModuleGraph {
    /// Create a new module graph
    pub fn new(root_module: PathBuf) -> Self {
        Self {
            modules: HashMap::new(),
            compilation_order: Vec::new(),
            root_module,
        }
    }
    
    /// Add a module to the graph
    pub fn add_module(&mut self, module: ModuleContext) {
        self.modules.insert(module.file_path.clone(), module);
    }
    
    /// Resolve module dependencies and set compilation order
    pub fn resolve_dependencies(&mut self) -> Result<()> {
        let mut visited = HashSet::new();
        let mut visiting = HashSet::new();
        let mut order = Vec::new();
        
        // Start with root module
        self.resolve_module_recursive(&self.root_module.clone(), &mut visited, &mut visiting, &mut order)?;
        
        // Add any remaining modules that weren't visited
        let module_paths: Vec<PathBuf> = self.modules.keys().cloned().collect();
        for module_path in module_paths {
            if !visited.contains(&module_path) {
                self.resolve_module_recursive(&module_path, &mut visited, &mut visiting, &mut order)?;
            }
        }
        
        self.compilation_order = order;
        
        // Set dependency order in modules
        for (index, module_path) in self.compilation_order.iter().enumerate() {
            if let Some(module) = self.modules.get_mut(module_path) {
                module.dependency_order = Some(index);
            }
        }
        
        Ok(())
    }
    
    fn resolve_module_recursive(
        &mut self,
        module_path: &PathBuf,
        visited: &mut HashSet<PathBuf>,
        visiting: &mut HashSet<PathBuf>,
        order: &mut Vec<PathBuf>
    ) -> Result<()> {
        if visited.contains(module_path) {
            return Ok(());
        }
        
        if visiting.contains(module_path) {
            return Err(CompilerError::Include {
                message: format!("Circular dependency detected involving module: {}", module_path.display()),
            });
        }
        
        visiting.insert(module_path.clone());
        
        // Get dependencies of this module
        if let Some(module) = self.modules.get(module_path) {
            let dependencies: Vec<PathBuf> = module.imports.iter()
                .map(|import| import.module_path.clone())
                .collect();
            
            // Visit all dependencies first
            for dep_path in dependencies {
                self.resolve_module_recursive(&dep_path, visited, visiting, order)?;
            }
        }
        
        visiting.remove(module_path);
        visited.insert(module_path.clone());
        order.push(module_path.clone());
        
        Ok(())
    }
    
    /// Get the modules in dependency order
    pub fn get_ordered_modules(&self) -> Vec<&ModuleContext> {
        self.compilation_order.iter()
            .filter_map(|path| self.modules.get(path))
            .collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;
    
    #[test]
    fn test_module_privacy() {
        let mut module = ModuleContext::new(PathBuf::from("test.kry"));
        
        // Add public variable
        module.add_variable("public_var".to_string(), VariableDef {
            value: "value".to_string(),
            raw_value: "value".to_string(),
            def_line: 1,
            is_resolving: false,
            is_resolved: true,
        });
        
        // Add private variable
        module.add_variable("_private_var".to_string(), VariableDef {
            value: "private".to_string(),
            raw_value: "private".to_string(),
            def_line: 2,
            is_resolving: false,
            is_resolved: true,
        });
        
        assert!(!module.is_private("public_var"));
        assert!(module.is_private("_private_var"));
        
        let public_vars = module.get_public_variables();
        assert!(public_vars.contains_key("public_var"));
        assert!(!public_vars.contains_key("_private_var"));
    }
    
    #[test]
    fn test_module_import_override() {
        let mut base_module = ModuleContext::new(PathBuf::from("base.kry"));
        base_module.add_variable("color".to_string(), VariableDef {
            value: "blue".to_string(),
            raw_value: "blue".to_string(),
            def_line: 1,
            is_resolving: false,
            is_resolved: true,
        });
        
        let mut importing_module = ModuleContext::new(PathBuf::from("main.kry"));
        // Add local override
        importing_module.add_variable("color".to_string(), VariableDef {
            value: "red".to_string(),
            raw_value: "red".to_string(),
            def_line: 1,
            is_resolving: false,
            is_resolved: true,
        });
        
        // Import should not override local definition
        importing_module.import_module(&base_module, 0).unwrap();
        
        assert_eq!(importing_module.variables.get("color").unwrap().value, "red");
    }
}