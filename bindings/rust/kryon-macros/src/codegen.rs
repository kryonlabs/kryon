//! Code generation from parsed component blocks

use crate::parser::{Child, ComponentBlock, Property};
use crate::reactive;
use proc_macro2::TokenStream;
use quote::{format_ident, quote};
use syn::Ident;

/// Generate code from a parsed component block
pub fn generate_code(component: &ComponentBlock) -> TokenStream {
    // Generate the main component tree with ID management
    let component_code = generate_component(component, 0, &format_ident!("id_counter"));

    quote! {
        {
            use ::kryon::prelude::*;

            // ID counter for component tree
            let mut id_counter = 0u32;

            // Generate the component tree
            let root = #component_code;

            // Create IR document and run
            let doc = IRDocument::new(root);
            doc
        }
    }
}

/// Generate code for a single component
fn generate_component(
    component: &ComponentBlock,
    depth: usize,
    id_counter: &Ident,
) -> TokenStream {
    let component_type = &component.component_type;

    // Map component type to builder function
    let builder_fn = component_type_to_builder(&component.component_type);

    // Determine if this component takes initial content (e.g., text, button)
    let initial_content = get_initial_content(&component.properties);

    // Generate builder initialization
    let builder_init = if let Some(content_expr) = initial_content {
        quote! { #builder_fn(#content_expr) }
    } else {
        quote! { #builder_fn() }
    };

    // Generate property setter calls
    let property_calls = component
        .properties
        .iter()
        .filter_map(|prop| {
            // Skip properties used as initial content
            if is_initial_content_property(&prop.name) {
                return None;
            }

            let name = &prop.name;
            let value = &prop.value;

            // Convert snake_case property names to builder method names
            let method_name = format_ident!("{}", name);

            Some(quote! { .#method_name(#value) })
        })
        .collect::<Vec<_>>();

    // Generate child components
    let child_calls = component
        .children
        .iter()
        .map(|child| match child {
            Child::Component(child_component) => {
                let child_code = generate_component(child_component, depth + 1, id_counter);
                quote! { .child(#child_code) }
            }
            Child::Expr(expr) => {
                // For now, wrap expressions as-is (will handle conditionals/loops later)
                quote! { .child(#expr) }
            }
        })
        .collect::<Vec<_>>();

    // Increment ID counter and build component
    quote! {
        {
            #id_counter += 1;
            let component_id = #id_counter;

            #builder_init
                #(#property_calls)*
                #(#child_calls)*
                .build(component_id)
        }
    }
}

/// Map component type name to builder function name
fn component_type_to_builder(ident: &Ident) -> Ident {
    let name = ident.to_string();
    let builder_name = match name.as_str() {
        "App" => "container", // App is just a special container
        "Column" => "column",
        "Row" => "row",
        "Container" => "container",
        "Text" => "text",
        "Button" => "button",
        _ => {
            // Convert PascalCase to snake_case
            &to_snake_case(&name)
        }
    };

    format_ident!("{}", builder_name)
}

/// Check if a property should be used as initial content for the builder
fn is_initial_content_property(name: &Ident) -> bool {
    matches!(name.to_string().as_str(), "content" | "text" | "title")
}

/// Get the initial content expression for a component (if any)
fn get_initial_content(properties: &[Property]) -> Option<&syn::Expr> {
    properties.iter().find_map(|prop| {
        if is_initial_content_property(&prop.name) {
            Some(&prop.value)
        } else {
            None
        }
    })
}

/// Convert PascalCase to snake_case
fn to_snake_case(s: &str) -> String {
    let mut result = String::new();
    let mut chars = s.chars().peekable();

    while let Some(c) = chars.next() {
        if c.is_uppercase() {
            if !result.is_empty() {
                result.push('_');
            }
            result.push(c.to_lowercase().next().unwrap());
        } else {
            result.push(c);
        }
    }

    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_to_snake_case() {
        assert_eq!(to_snake_case("Column"), "column");
        assert_eq!(to_snake_case("TabGroup"), "tab_group");
        assert_eq!(to_snake_case("Text"), "text");
    }

    #[test]
    fn test_is_initial_content_property() {
        assert!(is_initial_content_property(&format_ident!("content")));
        assert!(is_initial_content_property(&format_ident!("text")));
        assert!(is_initial_content_property(&format_ident!("title")));
        assert!(!is_initial_content_property(&format_ident!("width")));
    }
}
