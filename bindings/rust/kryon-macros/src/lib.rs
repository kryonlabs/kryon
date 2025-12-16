//! # Kryon Macros
//!
//! Procedural macros for the Kryon UI framework.
//!
//! This crate provides the `kryon_app!` macro for declarative UI syntax,
//! implementing Proposal 3 (Declarative Blocks) from the design document.

extern crate proc_macro;

mod parser;
mod codegen;
mod reactive;

use proc_macro::TokenStream;
use syn::parse_macro_input;

/// Declarative UI macro for Kryon applications
///
/// # Example
///
/// ```ignore
/// kryon_app! {
///     App {
///         title: "Counter",
///         width: 800,
///         height: 600,
///
///         Column {
///             width: "100%",
///             gap: 20,
///
///             Text {
///                 content: "Hello, Kryon!",
///                 font_size: 32,
///             },
///
///             Button {
///                 text: "Click me",
///                 on_click: || println!("Clicked!"),
///             },
///         },
///     }
/// }
/// ```
#[proc_macro]
pub fn kryon_app(input: TokenStream) -> TokenStream {
    let component_block = parse_macro_input!(input as parser::ComponentBlock);

    let generated = codegen::generate_code(&component_block);

    TokenStream::from(generated)
}
