//! Kryon UI Language Compiler
//!
//! A complete compiler for the KRY declarative UI language that produces
//! optimized KRB binary files for cross-platform execution.
//!
//! # Basic Usage
//!
//! ```no_run
//! use kryc::{compile_file, CompilerOptions, Result};
//!
//! fn main() -> Result<()> {
//!     let options = CompilerOptions::default();
//!     compile_file("app.kry", "app.krb", options)?;
//!     Ok(())
//! }
//! ```

// 1. Declare all the top-level library modules.
//    All other files have been moved inside these.
pub mod cli;
pub mod compiler;
pub mod core;
pub mod error;

// 2. Define the public API by re-exporting the most important types and functions.
//    This allows users to `use kryc::CompilerError` instead of `use kryc::error::CompilerError`.
pub use cli::EnhancedCli;
pub use core::*; // Re-exports Element, CompilerState, PropertyId, KrbFileInfo, etc.
pub use error::{CompilerError, Result};

// --- Public-Facing Structs, Enums, and Constants for Library Users ---

/// Compiler version information.
pub const VERSION: &str = env!("CARGO_PKG_VERSION");
pub const NAME: &str = env!("CARGO_PKG_NAME");
pub const DESCRIPTION: &str = env!("CARGO_PKG_DESCRIPTION");

/// Compiler build information, useful for tools integrating with the compiler.
#[derive(Debug, Clone)]
pub struct CompilerInfo {
    pub version: &'static str,
    pub name: &'static str,
    pub description: &'static str,
    pub target_krb_version: (u8, u8),
    pub supported_features: &'static [&'static str],
}

pub const BUILD_INFO: CompilerInfo = CompilerInfo {
    version: VERSION,
    name: NAME,
    description: DESCRIPTION,
    target_krb_version: (core::constants::KRB_VERSION_MAJOR, core::constants::KRB_VERSION_MINOR),
    supported_features: &[
        "includes", "variables", "styles", "components", "scripting",
        "pseudo-selectors", "animations", "resources",
    ],
};

/// Target platform for compilation, affecting platform-specific optimizations.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum TargetPlatform {
    Desktop,
    Mobile,
    Web,
    Embedded,
    #[default]
    Universal,
}

/// Compilation options and settings to control the compiler's behavior.
#[derive(Debug, Clone, Default)]
pub struct CompilerOptions {
    pub debug_mode: bool,
    pub optimization_level: u8,
    pub target_platform: TargetPlatform,
    pub embed_scripts: bool,
    pub compress_output: bool,
    pub max_file_size: u64,
    pub include_directories: Vec<String>,
    pub generate_debug_info: bool,
    pub custom_variables: std::collections::HashMap<String, String>,
}

/// Compilation statistics and metrics returned after a successful compilation.
#[derive(Debug, Clone, Default, serde::Serialize)]
pub struct CompilationStats {
    pub source_size: u64,
    pub output_size: u64,
    pub compression_ratio: f64,
    pub element_count: usize,
    pub style_count: usize,
    pub component_count: usize,
    pub script_count: usize,
    pub resource_count: usize,
    pub string_count: usize,
    pub include_count: usize,
    pub variable_count: usize,
    pub compile_time_ms: u64,
    pub peak_memory_usage: u64,
}

// --- Public-Facing Functions (The Library's API) ---

/// Main compiler entry point with custom options.
///
/// This function orchestrates the entire compilation pipeline and is the
/// primary way to use `kryc` as a library.
pub fn compile_file_with_options(
    input_path: &str,
    output_path: &str,
    options: CompilerOptions,
) -> Result<CompilationStats> {
    // The implementation is now fully delegated to the `compiler` module.
    compiler::compile_with_options(input_path, output_path, options)
}

/// A convenience function to compile a file with default options.
pub fn compile_file(input_path: &str, output_path: &str) -> Result<CompilationStats> {
    compile_file_with_options(input_path, output_path, CompilerOptions::default())
}

/// Utility function to analyze a KRB file and return detailed information.
pub fn analyze_krb_file(file_path: &str) -> Result<KrbFileInfo> {
    let data = std::fs::read(file_path).map_err(|e| CompilerError::FileNotFound {
        path: format!("{}: {}", file_path, e),
    })?;
    // The implementation was moved to the `core` module.
    core::validate_krb_file(&data)
}

/// Checks if the compiler build supports a specific feature.
pub fn supports_feature(feature: &str) -> bool {
    BUILD_INFO.supported_features.contains(&feature)
}

/// Gets the compiler's build information.
pub fn build_info() -> &'static CompilerInfo {
    &BUILD_INFO
}