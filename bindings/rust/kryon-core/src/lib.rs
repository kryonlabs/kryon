//! # Kryon Core
//!
//! Core IR types for the Kryon UI framework.
//!
//! This crate provides the fundamental types used to represent UI components
//! in Kryon's intermediate representation (IR) format. All Kryon frontends
//! (Nim, Lua, C, Rust, etc.) serialize to this common IR format (.kir files),
//! which can then be rendered by any Kryon backend (SDL3, Terminal, Web).

pub mod color;
pub mod dimension;
pub mod ir;

pub use color::Color;
pub use dimension::Dimension;
pub use ir::{
    Alignment, AlignmentValue, ComponentType, IRComponent, IRDocument, Logic, ReactiveManifest,
};
