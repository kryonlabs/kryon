//! Common imports and utilities for Kryon tests

// Re-export essential testing types
pub use crate::{
    utils::*,
    fixtures::*,
    runners::*,
    assertions::*,
    snapshots::*,
    benchmarks::*,
    properties::*,
    integration::*,
    visual::*,
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