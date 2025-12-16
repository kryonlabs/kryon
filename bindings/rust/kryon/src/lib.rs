//! # Kryon
//!
//! Rust frontend for the Kryon UI framework.
//!
//! This crate provides a builder API for constructing Kryon UIs in Rust.
//!
//! ## Example
//!
//! ```rust
//! use kryon::prelude::*;
//!
//! let mut root = column()
//!     .width("100%")
//!     .height("100%")
//!     .background("#1a1a2e")
//!     .padding(30.0)
//!     .gap(20.0)
//!     .child(text("Hello, Kryon!").color("#fff").build(1))
//!     .build(0);
//!
//! let doc = IRDocument::new(root);
//! doc.write_to_file("output.kir").unwrap();
//! ```

pub mod builder;
pub mod reactive;

pub use kryon_core::{
    Alignment, AlignmentValue, Color, ComponentType, Dimension, IRComponent, IRDocument,
    Logic, ReactiveManifest,
};

pub use builder::{
    button, column, container, row, text, ButtonBuilder, ColumnBuilder, ContainerBuilder,
    RowBuilder, TextBuilder,
};

pub use reactive::{signal, Computed, Signal};

// Re-export procedural macros
pub use kryon_macros::kryon_app;

/// Prelude module for convenient imports
pub mod prelude {
    pub use crate::builder::{button, column, container, row, text};
    pub use crate::reactive::{signal, Computed, Signal};
    pub use crate::{
        Alignment, AlignmentValue, Color, ComponentType, Dimension, IRComponent, IRDocument,
    };
    pub use kryon_macros::kryon_app;
}
