//! Preprocessor for handling @include directives and file inclusion
//! 
//! Now supports module-level isolation where each @include creates
//! an isolated context with its own variables, styles, and components.

use crate::error::{CompilerError, Result, SourceMap};
use crate::core::MAX_INCLUDE_DEPTH;

use crate::compiler::middle_end::module_context::{ModuleContext, ModuleGraph, ModuleImport};
use std::collections::{HashSet, HashMap};
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug)]
pub struct Preprocessor {
    included_files: HashSet<PathBuf>,
    current_depth: usize,
    source_map: SourceMap,
    combined_line_count: usize,
    
    // New: Global cache of processed files to prevent duplicate inclusion
    processed_file_cache: HashMap<PathBuf, String>,
    
    // New: Module isolation support
    module_graph: Option<ModuleGraph>,
    processed_modules: HashMap<PathBuf, ModuleContext>,
}

impl Preprocessor {
    pub fn new() -> Self {
        Self {
            included_files: HashSet::new(),
            current_depth: 0,
            source_map: SourceMap::new(),
            combined_line_count: 1,
            processed_file_cache: HashMap::new(),
            module_graph: None,
            processed_modules: HashMap::new(),
        }
    }

    
    /// NEW: Process includes with module isolation
    /// Each @include creates an isolated module context
    pub fn process_includes_isolated(&mut self, file_path: &str) -> Result<ModuleGraph> {
        let root_path = fs::canonicalize(file_path).map_err(|e| {
            CompilerError::FileNotFound {
                path: format!("{}: {}", file_path, e),
            }
        })?;
        
        // Initialize module graph
        let mut graph = ModuleGraph::new(root_path.clone());
        
        // Process the root module and all its dependencies
        self.process_module_recursive(&root_path, &mut graph, 0)?;
        
        // Resolve module dependencies and import order
        graph.resolve_dependencies()?;
        
        // Apply imports with override priority
        self.apply_module_imports(&mut graph)?;
        
        Ok(graph)
    }
    
    fn process_module_recursive(&mut self, module_path: &PathBuf, graph: &mut ModuleGraph, depth: usize) -> Result<()> {
        if depth > MAX_INCLUDE_DEPTH {
            return Err(CompilerError::Include {
                message: format!("Maximum include depth ({}) exceeded processing '{}'", 
                                MAX_INCLUDE_DEPTH, module_path.display()),
            });
        }
        
        // Check if we've already processed this module
        if graph.modules.contains_key(module_path) {
            return Ok(());
        }
        
        // Check for circular includes in current processing stack
        if self.included_files.contains(module_path) {
            return Err(CompilerError::Include {
                message: format!("Circular include detected: '{}'", module_path.display()),
            });
        }
        
        self.included_files.insert(module_path.clone());
        
        // Read and process the module file
        let content = fs::read_to_string(module_path).map_err(|e| {
            CompilerError::FileNotFound {
                path: format!("{}: {}", module_path.display(), e),
            }
        })?;
        
        // Create module context
        let mut module = ModuleContext::new(module_path.clone());
        
        // Process the content and extract includes
        let (processed_content, include_paths) = self.process_module_content(&content, module_path, depth)?;
        module.content = processed_content;
        
        // Process all included modules first (dependencies)
        let mut import_order = 0;
        for include_path in include_paths {
            self.process_module_recursive(&include_path, graph, depth + 1)?;
            
            // Record the import in the module
            let import = ModuleImport {
                module_path: include_path,
                accessible_items: Vec::new(), // Will be filled during import resolution
                import_order,
            };
            module.imports.push(import);
            import_order += 1;
        }
        
        // Add this module to the graph
        graph.add_module(module);
        
        self.included_files.remove(module_path);
        Ok(())
    }
    
    fn process_module_content(&mut self, content: &str, module_path: &PathBuf, _depth: usize) -> Result<(String, Vec<PathBuf>)> {
        let mut result = String::new();
        let mut include_paths = Vec::new();
        let mut line_num = 0;
        let lines: Vec<&str> = content.lines().collect();
        let base_dir = module_path.parent().unwrap_or(Path::new(""));
        
        for line in &lines {
            line_num += 1;
            let trimmed = line.trim_start();
            
            if let Some(include_path) = self.parse_include_line(trimmed, line_num)? {
                // Store include path for comment before moving it
                let include_comment = include_path.clone();
                
                // Resolve include path relative to current module
                let full_include_path = if Path::new(&include_path).is_absolute() {
                    PathBuf::from(include_path)
                } else {
                    base_dir.join(&include_path)
                };
                
                let canonical_include_path = fs::canonicalize(&full_include_path).map_err(|e| {
                    CompilerError::FileNotFound {
                        path: format!("{}: {}", full_include_path.display(), e),
                    }
                })?;
                
                include_paths.push(canonical_include_path);
                
                // In isolated mode, @include lines are removed from content
                // The module content will be merged later during compilation
                result.push_str(&format!("# @include processed: {}\n", include_comment));
            } else {
                result.push_str(line);
                result.push('\n');
            }
        }
        
        Ok((result, include_paths))
    }
    
    fn apply_module_imports(&mut self, graph: &mut ModuleGraph) -> Result<()> {
        // Apply imports in dependency order
        for module_path in graph.compilation_order.clone() {
            let import_list = if let Some(module) = graph.modules.get(&module_path) {
                module.imports.clone()
            } else {
                continue;
            };
            
            for import in import_list {
                if let Some(imported_module) = graph.modules.get(&import.module_path).cloned() {
                    if let Some(importing_module) = graph.modules.get_mut(&module_path) {
                        importing_module.import_module(&imported_module, import.import_order)?;
                    }
                }
            }
        }
        
        Ok(())
    }


    /// Parse an @include line and extract the file path
    /// Returns None if this isn't a valid include line
    fn parse_include_line(&self, line: &str, line_num: usize) -> Result<Option<String>> {
        if !line.starts_with("@include") {
            return Ok(None);
        }

        let after_include = line[8..].trim_start(); // Skip "@include"

        // Must start with quote
        if !after_include.starts_with('"') {
            return Err(CompilerError::Parse {
                file: "<unknown>".to_string(),
                line: line_num,
                message: "Invalid @include syntax: path must be quoted".to_string(),
            });
        }

        // Find closing quote
        let mut end_quote_pos = None;
        let mut escaped = false;
        
        for (i, ch) in after_include[1..].char_indices() {
            if escaped {
                escaped = false;
                continue;
            }
            
            if ch == '\\' {
                escaped = true;
            } else if ch == '"' {
                end_quote_pos = Some(i + 1);
                break;
            }
        }

        let end_quote_pos = end_quote_pos.ok_or_else(|| CompilerError::Parse {
            file: "<unknown>".to_string(),
            line: line_num,
            message: "Invalid @include syntax: missing closing quote".to_string(),
        })?;

        let path = &after_include[1..end_quote_pos];
        
        // Check what comes after the closing quote
        let after_quote = after_include[end_quote_pos + 1..].trim_start();
        
        // Should be empty or a comment
        if !after_quote.is_empty() && !after_quote.starts_with('#') {
            return Err(CompilerError::Parse {
                file: "<unknown>".to_string(),
                line: line_num,
                message: format!("Invalid @include syntax: unexpected content after path: '{}'", 
                               after_quote),
            });
        }

        if path.is_empty() {
            return Err(CompilerError::Parse {
                file: "<unknown>".to_string(),
                line: line_num,
                message: "Invalid @include syntax: path cannot be empty".to_string(),
            });
        }

        // Process escape sequences in the path
        let unescaped_path = self.unescape_string(path)?;
        
        Ok(Some(unescaped_path))
    }

    /// Unescape string literals in include paths
    fn unescape_string(&self, s: &str) -> Result<String> {
        let mut result = String::new();
        let mut chars = s.chars();
        
        while let Some(ch) = chars.next() {
            if ch == '\\' {
                match chars.next() {
                    Some('n') => result.push('\n'),
                    Some('t') => result.push('\t'),
                    Some('r') => result.push('\r'),
                    Some('\\') => result.push('\\'),
                    Some('"') => result.push('"'),
                    Some('\'') => result.push('\''),
                    Some(other) => {
                        result.push('\\');
                        result.push(other);
                    }
                    None => result.push('\\'),
                }
            } else {
                result.push(ch);
            }
        }
        
        Ok(result)
    }
}


fn merge_module_graph_content(graph: &ModuleGraph) -> String {
    let mut merged = String::new();
    
    // Add content from all modules in dependency order
    for module in graph.get_ordered_modules() {
        merged.push_str(&format!("# Module: {}\n", module.file_path.display()));
        merged.push_str(&module.content);
        merged.push_str("\n\n");
    }
    
    log::debug!("Merged module content:\n{}", merged);
    merged
}


