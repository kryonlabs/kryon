//! Script bytecode compilation and validation
//! 
//! This module provides comprehensive script compilation capabilities including:
//! - Lua bytecode compilation using mlua
//! - Advanced syntax validation with detailed error reporting
//! - Multi-language compilation framework
//! - Source map generation for debugging
//! - Performance optimizations for embedded systems

use crate::error::{CompilerError, Result};
use crate::core::*;
use crate::core::types::ScriptLanguage;
use std::collections::HashSet;
use regex::Regex;

#[cfg(feature = "lua")]
use mlua::{Lua, Error as LuaError};

/// Comprehensive script compilation error types
#[derive(Debug, thiserror::Error)]
pub enum ScriptCompilationError {
    #[error("Syntax error in {file} at line {line}, column {column}: {message}\n{context}")]
    SyntaxError {
        file: String,
        line: usize,
        column: usize,
        message: String,
        context: String,
    },
    
    #[error("Semantic error in {file} at line {line} in function '{function}': {message}")]
    SemanticError {
        file: String,
        line: usize,
        function: String,
        message: String,
    },
    
    #[error("Unsupported script language '{language}'. Supported languages: {supported:?}")]
    UnsupportedLanguage {
        language: String,
        supported: Vec<String>,
    },
    
    #[error("Bytecode generation failed for {file}: {error}")]
    BytecodeGenerationFailed {
        file: String,
        error: String,
    },
    
    #[error("Function extraction failed for {file}: {error}")]
    FunctionExtractionFailed {
        file: String,
        error: String,
    },
    
    #[error("Script feature '{feature}' not available (missing dependency or feature flag)")]
    FeatureNotAvailable {
        feature: String,
    },
}

/// Script compilation metadata
#[derive(Debug, Clone)]
pub struct CompilationMetadata {
    pub compiler_version: String,
    pub compilation_time: u64,
    pub optimization_level: u8,
    pub features_used: Vec<String>,
    pub source_hash: String,
    pub file_path: String,
}

/// Source map entry for debugging support
#[derive(Debug, Clone)]
pub struct SourceMapEntry {
    pub bytecode_offset: u32,
    pub source_line: usize,
    pub source_column: usize,
    pub function_name: Option<String>,
}

/// Compiled script with bytecode and metadata
#[derive(Debug, Clone)]
pub struct CompiledScript {
    pub name: String,
    pub language: ScriptLanguage,
    pub bytecode: Vec<u8>,
    pub entry_points: Vec<String>,
    pub source_map: Option<Vec<SourceMapEntry>>,
    pub compilation_metadata: CompilationMetadata,
    pub size_statistics: CompilationStatistics,
}

/// Compilation statistics for performance analysis
#[derive(Debug, Clone)]
pub struct CompilationStatistics {
    pub source_size: usize,
    pub bytecode_size: usize,
    pub compression_ratio: f64,
    pub compilation_time_ms: u64,
    pub function_count: usize,
    pub complexity_score: u32,
}

/// Main script compiler with multi-language support
pub struct ScriptCompiler {
    #[cfg(feature = "lua")]
    lua_compiler: LuaCompiler,
    supported_languages: Vec<ScriptLanguage>,
    optimization_level: u8,
    debug_mode: bool,
    validate_syntax: bool,
}

impl ScriptCompiler {
    /// Create a new script compiler with default settings
    pub fn new() -> Result<Self> {
        let mut supported_languages = Vec::new();
        
        #[cfg(feature = "lua")]
        supported_languages.push(ScriptLanguage::Lua);
        
        Ok(Self {
            #[cfg(feature = "lua")]
            lua_compiler: LuaCompiler::new()?,
            supported_languages,
            optimization_level: 1,
            debug_mode: false,
            validate_syntax: true,
        })
    }
    
    /// Create compiler with custom optimization settings
    pub fn with_optimization(mut self, level: u8, debug: bool) -> Self {
        self.optimization_level = level;
        self.debug_mode = debug;
        self
    }
    
    /// Enable or disable syntax validation
    pub fn with_syntax_validation(mut self, validate: bool) -> Self {
        self.validate_syntax = validate;
        self
    }
    
    /// Get list of supported script languages
    pub fn supported_languages(&self) -> &[ScriptLanguage] {
        &self.supported_languages
    }
    
    /// Compile source code directly to bytecode
    pub fn compile_source(
        &self,
        language: ScriptLanguage,
        source_code: &str,
        script_name: &str,
        file_path: &str,
    ) -> Result<CompiledScript> {
        let start_time = std::time::Instant::now();
        
        // Validate language support
        if !self.supported_languages.contains(&language) {
            return Err(ScriptCompilationError::UnsupportedLanguage {
                language: format!("{:?}", language),
                supported: self.supported_languages.iter().map(|l| format!("{:?}", l)).collect(),
            }.into());
        }
        
        // Compile based on language
        let result = match language {
            #[cfg(feature = "lua")]
            ScriptLanguage::Lua => self.lua_compiler.compile(source_code, file_path, self.optimization_level),
            _ => Err(ScriptCompilationError::UnsupportedLanguage {
                language: format!("{:?}", language),
                supported: self.supported_languages.iter().map(|l| format!("{:?}", l)).collect(),
            }.into()),
        }?;
        
        let compilation_time = start_time.elapsed().as_millis() as u64;
        
        // Create compilation statistics
        let size_statistics = CompilationStatistics {
            source_size: source_code.len(),
            bytecode_size: result.bytecode.len(),
            compression_ratio: result.bytecode.len() as f64 / source_code.len() as f64,
            compilation_time_ms: compilation_time,
            function_count: result.entry_points.len(),
            complexity_score: self.calculate_complexity(source_code),
        };
        
        // Create metadata
        let metadata = CompilationMetadata {
            compiler_version: env!("CARGO_PKG_VERSION").to_string(),
            compilation_time: std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap_or_default()
                .as_secs(),
            optimization_level: self.optimization_level,
            features_used: vec!["lua".to_string()],
            source_hash: format!("{:x}", md5::compute(source_code)),
            file_path: file_path.to_string(),
        };
        
        Ok(CompiledScript {
            name: script_name.to_string(),
            language: language,
            bytecode: result.bytecode,
            entry_points: result.entry_points,
            source_map: result.source_map,
            compilation_metadata: metadata,
            size_statistics: size_statistics,
        })
    }
    
    /// Compile a script entry to bytecode
    pub fn compile_script(
        &self,
        script_entry: &ScriptEntry,
        file_path: &str,
    ) -> Result<CompiledScript> {
        let start_time = std::time::Instant::now();
        
        // Validate language support
        if !self.supported_languages.contains(&script_entry.language_id) {
            return Err(ScriptCompilationError::UnsupportedLanguage {
                language: format!("{:?}", script_entry.language_id),
                supported: self.supported_languages.iter().map(|l| format!("{:?}", l)).collect(),
            }.into());
        }
        
        // Extract source code
        let source_code = String::from_utf8_lossy(&script_entry.code_data);
        
        // Compile based on language
        let result = match script_entry.language_id {
            #[cfg(feature = "lua")]
            ScriptLanguage::Lua => self.lua_compiler.compile(&source_code, file_path, self.optimization_level),
            _ => Err(ScriptCompilationError::UnsupportedLanguage {
                language: format!("{:?}", script_entry.language_id),
                supported: self.supported_languages.iter().map(|l| format!("{:?}", l)).collect(),
            }.into()),
        }?;
        
        let compilation_time = start_time.elapsed().as_millis() as u64;
        
        // Create compilation statistics
        let size_statistics = CompilationStatistics {
            source_size: source_code.len(),
            bytecode_size: result.bytecode.len(),
            compression_ratio: result.bytecode.len() as f64 / source_code.len() as f64,
            compilation_time_ms: compilation_time,
            function_count: result.entry_points.len(),
            complexity_score: self.calculate_complexity(&source_code),
        };
        
        // Create metadata
        let metadata = CompilationMetadata {
            compiler_version: env!("CARGO_PKG_VERSION").to_string(),
            compilation_time: std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap_or_default()
                .as_secs(),
            optimization_level: self.optimization_level,
            features_used: self.detect_features_used(&source_code),
            source_hash: self.calculate_source_hash(&source_code),
            file_path: file_path.to_string(),
        };
        
        Ok(CompiledScript {
            name: script_entry.name.clone(),
            language: script_entry.language_id,
            bytecode: result.bytecode,
            entry_points: result.entry_points,
            source_map: result.source_map,
            compilation_metadata: metadata,
            size_statistics,
        })
    }
    
    /// Calculate script complexity score for optimization decisions
    fn calculate_complexity(&self, source: &str) -> u32 {
        let mut score = 0u32;
        
        // Basic complexity metrics
        score += source.lines().count() as u32; // Line count
        score += source.matches("function").count() as u32 * 10; // Function definitions
        score += source.matches("if").count() as u32 * 3; // Conditional branches
        score += source.matches("for").count() as u32 * 5; // Loops
        score += source.matches("while").count() as u32 * 5; // Loops
        score += source.matches("local").count() as u32 * 2; // Variable declarations
        
        score
    }
    
    /// Detect language features used in the source code
    fn detect_features_used(&self, source: &str) -> Vec<String> {
        let mut features = Vec::new();
        
        // Common features to detect
        if source.contains("coroutine") {
            features.push("coroutines".to_string());
        }
        if source.contains("require") || source.contains("module") {
            features.push("modules".to_string());
        }
        if source.contains("__index") || source.contains("__newindex") {
            features.push("metamethods".to_string());
        }
        if source.contains("pcall") || source.contains("xpcall") {
            features.push("error_handling".to_string());
        }
        if source.contains("table.") {
            features.push("table_operations".to_string());
        }
        if source.contains("string.") {
            features.push("string_operations".to_string());
        }
        if source.contains("math.") {
            features.push("math_operations".to_string());
        }
        
        features
    }
    
    /// Calculate hash of source code for change detection
    fn calculate_source_hash(&self, source: &str) -> String {
        use std::collections::hash_map::DefaultHasher;
        use std::hash::{Hash, Hasher};
        
        let mut hasher = DefaultHasher::new();
        source.hash(&mut hasher);
        format!("{:x}", hasher.finish())
    }
}

/// Lua-specific compiler implementation
#[cfg(feature = "lua")]
pub struct LuaCompiler {
    lua: Lua,
    function_regex: Regex,
}

#[cfg(feature = "lua")]
impl LuaCompiler {
    /// Create a new Lua compiler
    pub fn new() -> Result<Self> {
        let lua = Lua::new();
        
        // Set up a minimal, secure environment for compilation
        lua.load(r#"
            -- Disable potentially dangerous functions for compilation security
            os = nil
            io = nil
            debug = nil
            package = nil
            loadfile = nil
            dofile = nil
        "#).exec().map_err(|e| {
            ScriptCompilationError::FeatureNotAvailable {
                feature: format!("Lua compiler initialization failed: {}", e),
            }
        })?;
        
        let function_regex = Regex::new(r"function\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(")
            .map_err(|e| CompilerError::script_legacy(0, format!("Regex compilation failed: {}", e)))?;
        
        Ok(Self {
            lua,
            function_regex,
        })
    }
    
    /// Compile Lua source to bytecode with comprehensive validation
    pub fn compile(
        &self,
        source: &str,
        file_path: &str,
        optimization_level: u8,
    ) -> Result<CompilationResult> {
        // Step 1: Comprehensive syntax validation
        self.validate_syntax_comprehensive(source, file_path)?;
        
        // Step 2: Extract entry points before compilation
        let entry_points = self.extract_entry_points(source, file_path)?;
        
        // Step 3: Compile to bytecode
        let bytecode = self.compile_to_bytecode(source, optimization_level, file_path)?;
        
        // Step 4: Generate source map if in debug mode
        let source_map = if optimization_level == 0 {
            Some(self.generate_source_map(source, &bytecode)?)
        } else {
            None
        };
        
        Ok(CompilationResult {
            bytecode,
            entry_points,
            source_map,
        })
    }
    
    /// Comprehensive Lua syntax validation with detailed error reporting
    fn validate_syntax_comprehensive(&self, source: &str, file_path: &str) -> Result<()> {
        // Method 1: Use mlua's built-in syntax checking
        match self.lua.load(source).into_function() {
            Ok(_) => {
                // Basic syntax is valid, now do semantic validation
                self.validate_semantic_rules(source, file_path)?;
                Ok(())
            }
            Err(lua_error) => {
                let (line, column, message) = self.parse_lua_error(&lua_error);
                let context = self.get_source_context(source, line, 3);
                
                Err(ScriptCompilationError::SyntaxError {
                    file: file_path.to_string(),
                    line,
                    column,
                    message,
                    context,
                }.into())
            }
        }
    }
    
    /// Validate semantic rules beyond basic syntax
    fn validate_semantic_rules(&self, source: &str, file_path: &str) -> Result<()> {
        let mut errors = Vec::new();
        
        // Check for common Lua pitfalls and best practices
        for (line_num, line) in source.lines().enumerate() {
            let line_num = line_num + 1;
            
            // Check for global variable assignments without local
            if let Some(var_name) = self.detect_implicit_global(line) {
                if !self.is_function_name(&var_name) {
                    errors.push(format!(
                        "Line {}: Implicit global variable '{}'. Consider using 'local {}' for better performance.",
                        line_num, var_name, var_name
                    ));
                }
            }
            
            // Check for deprecated patterns
            if line.contains("table.getn") {
                errors.push(format!(
                    "Line {}: 'table.getn' is deprecated. Use '#table' instead.",
                    line_num
                ));
            }
            
            // Check for potential nil access patterns
            if line.contains(".") && !line.contains("local") && !line.contains("function") {
                if let Some(potential_nil) = self.detect_potential_nil_access(line) {
                    errors.push(format!(
                        "Line {}: Potential nil access on '{}'. Consider checking with 'if {} then' first.",
                        line_num, potential_nil, potential_nil
                    ));
                }
            }
        }
        
        // Report warnings if any (don't fail compilation, just warn)
        if !errors.is_empty() {
            eprintln!("Lua semantic warnings in {}:", file_path);
            for error in errors {
                eprintln!("  Warning: {}", error);
            }
        }
        
        Ok(())
    }
    
    /// Detect implicit global variable assignments
    fn detect_implicit_global(&self, line: &str) -> Option<String> {
        let trimmed = line.trim();
        
        // Skip comments and empty lines
        if trimmed.starts_with("--") || trimmed.is_empty() {
            return None;
        }
        
        // Skip local declarations
        if trimmed.starts_with("local ") {
            return None;
        }
        
        // Look for assignment pattern
        if let Some(equals_pos) = trimmed.find('=') {
            let left_side = trimmed[..equals_pos].trim();
            
            // Simple variable assignment (not table access)
            if !left_side.contains('.') && !left_side.contains('[') {
                // Extract variable name
                let var_name = left_side.split_whitespace().last()?.to_string();
                
                // Check if it's a valid identifier
                if var_name.chars().all(|c| c.is_alphanumeric() || c == '_') 
                   && var_name.chars().next()?.is_alphabetic() {
                    return Some(var_name);
                }
            }
        }
        
        None
    }
    
    /// Check if a name is likely a function name
    fn is_function_name(&self, name: &str) -> bool {
        // Common function naming patterns
        name.ends_with("Callback") || 
        name.ends_with("Handler") || 
        name.starts_with("on") ||
        name.starts_with("handle") ||
        self.function_regex.is_match(&format!("function {}", name))
    }
    
    /// Detect potential nil access patterns
    fn detect_potential_nil_access(&self, line: &str) -> Option<String> {
        // Look for patterns like variable.property without nil checking
        let trimmed = line.trim();
        
        if trimmed.contains('.') && !trimmed.contains("if ") && !trimmed.contains("and ") {
            if let Some(dot_pos) = trimmed.find('.') {
                let var_part = &trimmed[..dot_pos];
                let var_name = var_part.split_whitespace().last()?;
                
                // Skip known safe patterns
                if var_name == "string" || var_name == "table" || var_name == "math" {
                    return None;
                }
                
                return Some(var_name.to_string());
            }
        }
        
        None
    }
    
    /// Extract function entry points from Lua source
    fn extract_entry_points(&self, source: &str, file_path: &str) -> Result<Vec<String>> {
        let mut functions = Vec::new();
        let mut seen = HashSet::new();
        
        for captures in self.function_regex.captures_iter(source) {
            if let Some(func_name) = captures.get(1) {
                let name = func_name.as_str().to_string();
                if seen.insert(name.clone()) {
                    functions.push(name);
                }
            }
        }
        
        if functions.is_empty() {
            // Check if this might be a script without explicit functions
            if source.trim().is_empty() {
                return Err(ScriptCompilationError::FunctionExtractionFailed {
                    file: file_path.to_string(),
                    error: "Script is empty".to_string(),
                }.into());
            }
            
            // Allow scripts without functions (they might just have top-level code)
            functions.push("__main__".to_string());
        }
        
        Ok(functions)
    }
    
    /// Compile Lua source to bytecode
    fn compile_to_bytecode(
        &self,
        source: &str,
        optimization_level: u8,
        file_path: &str,
    ) -> Result<Vec<u8>> {
        // Create a function from the source - use just filename to reduce bytecode size
        let filename = std::path::Path::new(file_path)
            .file_name()
            .and_then(|name| name.to_str())
            .unwrap_or("script");
        
        let function = self.lua.load(source)
            .set_name(filename)
            .into_function()
            .map_err(|e| {
                ScriptCompilationError::BytecodeGenerationFailed {
                    file: file_path.to_string(),
                    error: format!("Failed to create function: {}", e),
                }
            })?;
        
        // Dump the function to bytecode
        // The strip parameter controls debug information inclusion
        let strip_debug = optimization_level > 0;
        
        let bytecode = function.dump(strip_debug);
        
        Ok(bytecode)
    }
    
    /// Generate source map for debugging (simplified implementation)
    fn generate_source_map(
        &self,
        source: &str,
        _bytecode: &[u8],
    ) -> Result<Vec<SourceMapEntry>> {
        let mut source_map = Vec::new();
        
        // For now, create a simple mapping of line numbers
        // In a full implementation, you'd analyze the bytecode structure
        for (line_num, line) in source.lines().enumerate() {
            if line.trim().starts_with("function ") {
                if let Some(captures) = self.function_regex.captures(line) {
                    if let Some(func_name) = captures.get(1) {
                        source_map.push(SourceMapEntry {
                            bytecode_offset: 0, // Would need bytecode analysis
                            source_line: line_num + 1,
                            source_column: line.find("function").unwrap_or(0),
                            function_name: Some(func_name.as_str().to_string()),
                        });
                    }
                }
            }
        }
        
        Ok(source_map)
    }
    
    /// Parse Lua error to extract line, column, and message
    fn parse_lua_error(&self, error: &LuaError) -> (usize, usize, String) {
        let error_str = error.to_string();
        
        // Try to extract line number from Lua error messages
        // Common patterns: "[string \"...\"]:5: message" or "line 5: message"
        if let Some(captures) = Regex::new(r":(\d+):\s*(.+)").unwrap().captures(&error_str) {
            if let (Some(line_match), Some(msg_match)) = (captures.get(1), captures.get(2)) {
                if let Ok(line_num) = line_match.as_str().parse::<usize>() {
                    return (line_num, 1, msg_match.as_str().to_string());
                }
            }
        }
        
        // Fallback if we can't parse the error format
        (1, 1, error_str)
    }
    
    /// Get source code context around an error line
    fn get_source_context(&self, source: &str, error_line: usize, context_lines: usize) -> String {
        let lines: Vec<&str> = source.lines().collect();
        let start = error_line.saturating_sub(context_lines + 1);
        let end = (error_line + context_lines).min(lines.len());
        
        let mut context = String::new();
        for (i, line) in lines[start..end].iter().enumerate() {
            let line_num = start + i + 1;
            let marker = if line_num == error_line { ">>> " } else { "    " };
            context.push_str(&format!("{}{:3}: {}\n", marker, line_num, line));
        }
        
        context
    }
}

/// Result of script compilation
#[derive(Debug)]
pub struct CompilationResult {
    pub bytecode: Vec<u8>,
    pub entry_points: Vec<String>,
    pub source_map: Option<Vec<SourceMapEntry>>,
}

/// Provide helpful compilation error messages for missing features
pub fn get_feature_help_message(language: &str) -> String {
    match language {
        "lua" => {
            "Lua compilation requires the 'lua' feature to be enabled.\n\
             Add to your Cargo.toml:\n\
             kryc = { features = [\"lua\"] }\n\
             \n\
             Or build with:\n\
             cargo build --features lua".to_string()
        }
        "javascript" => {
            "JavaScript compilation is planned but not yet implemented.\n\
             Currently supported languages: Lua".to_string()
        }
        "python" => {
            "Python compilation is planned but not yet implemented.\n\
             Currently supported languages: Lua".to_string()
        }
        "wren" => {
            "Wren compilation is planned but not yet implemented.\n\
             Currently supported languages: Lua".to_string()
        }
        _ => {
            format!("Unknown script language '{}'. Supported languages: Lua", language)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[cfg(feature = "lua")]
    #[test]
    fn test_lua_compilation() {
        let compiler = LuaCompiler::new().unwrap();
        
        let source = r#"
            function hello(name)
                print("Hello, " .. name)
                return "greeting sent"
            end
            
            function calculate(a, b)
                return a + b * 2
            end
        "#;
        
        let result = compiler.compile(source, "test.lua", 1).unwrap();
        
        assert!(!result.bytecode.is_empty());
        assert_eq!(result.entry_points.len(), 2);
        assert!(result.entry_points.contains(&"hello".to_string()));
        assert!(result.entry_points.contains(&"calculate".to_string()));
    }
    
    #[cfg(feature = "lua")]
    #[test]
    fn test_lua_syntax_validation() {
        let compiler = LuaCompiler::new().unwrap();
        
        // Valid syntax should pass
        let valid_source = r#"
            function test()
                local x = 10
                if x > 5 then
                    print("valid")
                end
            end
        "#;
        
        assert!(compiler.validate_syntax_comprehensive(valid_source, "test.lua").is_ok());
        
        // Invalid syntax should fail with detailed error
        let invalid_source = "function test( print('missing closing paren') end";
        
        let result = compiler.validate_syntax_comprehensive(invalid_source, "test.lua");
        assert!(result.is_err());
        
        if let Err(e) = result {
            let error_str = e.to_string();
            assert!(error_str.contains("Syntax error"));
            assert!(error_str.contains("test.lua"));
        }
    }
    
    #[test]
    fn test_script_compiler_creation() {
        let compiler = ScriptCompiler::new().unwrap();
        
        #[cfg(feature = "lua")]
        assert!(compiler.supported_languages().contains(&ScriptLanguage::Lua));
        
        assert_eq!(compiler.optimization_level, 1);
        assert!(!compiler.debug_mode);
        assert!(compiler.validate_syntax);
    }
    
    #[test]
    fn test_complexity_calculation() {
        let compiler = ScriptCompiler::new().unwrap();
        
        let simple_source = "print('hello')";
        let complex_source = r#"
            function complex_function()
                if condition then
                    for i = 1, 10 do
                        local x = calculate(i)
                        while x > 0 do
                            x = x - 1
                        end
                    end
                end
            end
        "#;
        
        let simple_score = compiler.calculate_complexity(simple_source);
        let complex_score = compiler.calculate_complexity(complex_source);
        
        assert!(complex_score > simple_score);
    }
    
    #[test]
    fn test_feature_detection() {
        let compiler = ScriptCompiler::new().unwrap();
        
        let source_with_features = r#"
            local co = coroutine.create(function() end)
            require("module")
            local meta = { __index = function() end }
            local success, result = pcall(risky_function)
            table.insert(list, value)
            string.find(text, pattern)
            math.sqrt(number)
        "#;
        
        let features = compiler.detect_features_used(source_with_features);
        
        assert!(features.contains(&"coroutines".to_string()));
        assert!(features.contains(&"modules".to_string()));
        assert!(features.contains(&"metamethods".to_string()));
        assert!(features.contains(&"error_handling".to_string()));
        assert!(features.contains(&"table_operations".to_string()));
        assert!(features.contains(&"string_operations".to_string()));
        assert!(features.contains(&"math_operations".to_string()));
    }
}