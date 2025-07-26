//! Kryon Testing Infrastructure
//! 
//! This crate provides comprehensive testing utilities and infrastructure for the entire Kryon project.
//! 
//! # Features
//! 
//! - **Unit Tests**: Individual component testing
//! - **Integration Tests**: Cross-component interaction testing  
//! - **Snapshot Tests**: Visual regression testing using Ratatui backend
//! - **Performance Tests**: Benchmarking and performance regression detection
//! - **Property-based Tests**: Fuzzing and property verification
//! - **End-to-End Tests**: Complete pipeline testing from KRY to rendered output
//! 
//! # Usage
//! 
//! ```rust
//! use kryon_tests::prelude::*;
//! 
//! #[tokio::test]
//! async fn test_compiler_pipeline() {
//!     let test_case = TestCase::new("basic_app")
//!         .with_source(r#"
//!             App {
//!                 window_width: 800
//!                 window_height: 600
//!                 Text { text: "Hello World" }
//!             }
//!         "#)
//!         .expect_compilation_success()
//!         .expect_elements(2)
//!         .expect_text_content("Hello World");
//!     
//!     test_case.run().await.unwrap();
//! }
//! ```

pub mod prelude;
pub mod utils;
pub mod fixtures;
pub mod runners;
pub mod assertions;
pub mod snapshots;
pub mod benchmarks;
pub mod properties;
pub mod integration;
pub mod visual;

// Re-export commonly used types
pub use prelude::*;