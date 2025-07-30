//! Common imports and utilities for Kryon tests

// Re-export essential testing types
pub use crate::{
    config::*,
    utils::*,
    fixtures::*,
    runners::*,
    assertions::*,
    snapshots::*,
    benchmarks::*,
    properties::*,
    integration::*,
    visual::*,
    metrics::*,
    coverage::*,
    dependencies::*,
    orchestrator::*,
    generation::*,
    caching::*,
};

// External testing dependencies
pub use tokio;
pub use serde::{Serialize, Deserialize};
pub use serde_json;
pub use anyhow::{Result, Context, bail};
pub use tempfile;
pub use insta;
pub use criterion;
pub use proptest;
pub use assert_matches::assert_matches;
pub use pretty_assertions::{assert_eq, assert_ne};

// Kryon core types
pub use kryon_compiler::{CompilerOptions, compile_string};
pub use kryon_core::{Element, ElementId, PropertyValue, KrbFile};
pub use kryon_runtime::OptimizedApp;
pub use kryon_layout::OptimizedTaffyLayoutEngine;

// Standard library
pub use std::{
    collections::HashMap,
    path::{Path, PathBuf},
    fs,
    time::Duration,
};

// Common test result types and utilities
#[derive(Debug, Default, Clone)]
pub struct TestSummary {
    pub total: usize,
    pub passed: usize,
    pub failed: usize,
    pub total_time: Duration,
    pub average_time: Duration,
    pub slowest_test: Option<String>,
}

impl TestSummary {
    pub fn success_rate(&self) -> f64 {
        if self.total == 0 {
            0.0
        } else {
            (self.passed as f64 / self.total as f64) * 100.0
        }
    }
}

// Test execution result
#[derive(Debug, Clone)]
pub struct TestResult {
    pub name: String,
    pub success: bool,
    pub execution_time: Duration,
    pub error_message: Option<String>,
}

// Test configuration structure
#[derive(Debug, Clone)]
pub struct TestConfig {
    pub timeout_seconds: u64,
    pub enable_snapshots: bool,
    pub enable_benchmarks: bool,
    pub parallel_execution: bool,
    pub output_directory: PathBuf,
}

impl Default for TestConfig {
    fn default() -> Self {
        Self {
            timeout_seconds: 300,
            enable_snapshots: true,
            enable_benchmarks: false,
            parallel_execution: true,
            output_directory: PathBuf::from("target/test-output"),
        }
    }
}

// Error type for testing infrastructure
#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("Configuration error: {0}")]
    Config(String),
    
    #[error("Test execution error: {0}")]
    Execution(String),
    
    #[error("Assertion failed: {0}")]
    Assertion(String),
    
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
    
    #[error("Serialization error: {0}")]
    Serialization(#[from] serde_json::Error),
    
    #[error("TOML error: {0}")]
    Toml(#[from] toml::de::Error),
    
    #[error("TOML serialization error: {0}")]
    TomlSer(#[from] toml::ser::Error),
}

pub type Result<T> = std::result::Result<T, Error>;

// Setup function for test environment
pub fn setup_test_environment() {
    // Initialize logging if not already done
    if std::env::var("RUST_LOG").is_err() {
        std::env::set_var("RUST_LOG", "info");
    }
    
    // Set up test-specific environment variables
    std::env::set_var("KRYON_TEST_MODE", "1");
    
    // Ensure test output directory exists
    let output_dir = std::path::Path::new("target/test-output");
    if !output_dir.exists() {
        let _ = std::fs::create_dir_all(output_dir);
    }
}