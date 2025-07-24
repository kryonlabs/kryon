//! Error types for the Kryon compiler

use thiserror::Error;
use std::collections::HashMap;

/// Maps combined content line numbers to original source locations
#[derive(Debug, Clone, Default)]
pub struct SourceMap {
    /// Map of combined line number -> (original file, original line number)
    line_map: HashMap<usize, (String, usize)>,
}

impl SourceMap {
    pub fn new() -> Self {
        Self::default()
    }

    /// Add a mapping for a range of lines from an included file
    pub fn add_file_mapping(&mut self, start_line: usize, file_path: &str, original_lines: &[&str]) {
        for (i, _) in original_lines.iter().enumerate() {
            self.line_map.insert(start_line + i, (file_path.to_string(), i + 1));
        }
    }

    /// Add a mapping for a single line
    pub fn add_line_mapping(&mut self, combined_line: usize, file_path: &str, original_line: usize) {
        self.line_map.insert(combined_line, (file_path.to_string(), original_line));
    }

    /// Get the original source location for a combined line number
    pub fn get_original_location(&self, combined_line: usize) -> Option<(String, usize)> {
        self.line_map.get(&combined_line).cloned()
    }

    /// Convert a line number using the source map, return (file, line) or default
    pub fn resolve_location(&self, combined_line: usize, default_file: &str) -> (String, usize) {
        self.get_original_location(combined_line)
            .unwrap_or_else(|| (default_file.to_string(), combined_line))
    }

    /// Get the number of line mappings
    pub fn mapping_count(&self) -> usize {
        self.line_map.len()
    }
}

#[derive(Error, Debug)]
pub enum CompilerError {
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
    
    #[error("Script compilation error: {0}")]
    ScriptCompilation(#[from] crate::compiler::middle_end::script_compiler::ScriptCompilationError),

    #[error("Parse error in {file} at line {line}: {message}")]
    Parse { file: String, line: usize, message: String },

    #[error("Semantic error in {file} at line {line}: {message}")]
    Semantic { file: String, line: usize, message: String },

    #[error("Include error: {message}")]
    Include { message: String },

    #[error("Variable error in {file} at line {line}: {message}")]
    Variable { file: String, line: usize, message: String },

    #[error("Style error: {message}")]
    Style { message: String },

    #[error("Component error in {file} at line {line}: {message}")]
    Component { file: String, line: usize, message: String },

    #[error("Script error in {file} at line {line}: {message}")]
    Script { file: String, line: usize, message: String },

    #[error("Code generation error: {message}")]
    CodeGen { message: String },

    #[error("Resource error: {message}")]
    Resource { message: String },

    #[error("Maximum limit exceeded: {limit_type} (limit: {limit})")]
    LimitExceeded { limit_type: String, limit: usize },

    #[error("File not found: {path}")]
    FileNotFound { path: String },

    #[error("Invalid format: {message}")]
    InvalidFormat { message: String },
}

pub type Result<T> = std::result::Result<T, CompilerError>;

impl CompilerError {
    pub fn parse(file: impl Into<String>, line: usize, message: impl Into<String>) -> Self {
        Self::Parse {
            file: file.into(),
            line,
            message: message.into(),
        }
    }

    pub fn semantic(file: impl Into<String>, line: usize, message: impl Into<String>) -> Self {
        Self::Semantic {
            file: file.into(),
            line,
            message: message.into(),
        }
    }

    pub fn component(file: impl Into<String>, line: usize, message: impl Into<String>) -> Self {
        Self::Component {
            file: file.into(),
            line,
            message: message.into(),
        }
    }

    pub fn script(file: impl Into<String>, line: usize, message: impl Into<String>) -> Self {
        Self::Script {
            file: file.into(),
            line,
            message: message.into(),
        }
    }

    pub fn variable(file: impl Into<String>, line: usize, message: impl Into<String>) -> Self {
        Self::Variable {
            file: file.into(),
            line,
            message: message.into(),
        }
    }

    // Legacy methods for backward compatibility (when file info is not available)
    pub fn parse_legacy(line: usize, message: impl Into<String>) -> Self {
        Self::Parse {
            file: "<unknown>".to_string(),
            line,
            message: message.into(),
        }
    }

    pub fn semantic_legacy(line: usize, message: impl Into<String>) -> Self {
        Self::Semantic {
            file: "<unknown>".to_string(),
            line,
            message: message.into(),
        }
    }

    pub fn component_legacy(line: usize, message: impl Into<String>) -> Self {
        Self::Component {
            file: "<unknown>".to_string(),
            line,
            message: message.into(),
        }
    }

    pub fn script_legacy(line: usize, message: impl Into<String>) -> Self {
        Self::Script {
            file: "<unknown>".to_string(),
            line,
            message: message.into(),
        }
    }

    pub fn variable_legacy(line: usize, message: impl Into<String>) -> Self {
        Self::Variable {
            file: "<unknown>".to_string(),
            line,
            message: message.into(),
        }
    }
}