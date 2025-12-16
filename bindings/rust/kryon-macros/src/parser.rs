//! Parser for declarative component syntax

use proc_macro2::Span;
use syn::parse::{Parse, ParseStream, Result};
use syn::{braced, Expr, Ident, Token};

/// A parsed component block with properties and children
///
/// Example:
/// ```ignore
/// Column {
///     width: "100%",
///     gap: 20,
///     Text { content: "Hello" },
/// }
/// ```
pub struct ComponentBlock {
    pub component_type: Ident,
    pub properties: Vec<Property>,
    pub children: Vec<Child>,
}

/// A property assignment in a component block
///
/// Examples:
/// - `width: 800`
/// - `color: "#fff"`
/// - `on_click: || println!("clicked")`
pub struct Property {
    pub name: Ident,
    pub value: Expr,
}

/// A child element (either a component block or an expression)
pub enum Child {
    Component(ComponentBlock),
    Expr(Expr),
}

impl Parse for ComponentBlock {
    fn parse(input: ParseStream) -> Result<Self> {
        // Parse component type name (e.g., "Column", "Button")
        let component_type: Ident = input.parse()?;

        // Parse the braced block
        let content;
        braced!(content in input);

        let mut properties = Vec::new();
        let mut children = Vec::new();

        // Parse properties and children
        while !content.is_empty() {
            // Try to parse as property assignment first
            if content.peek2(Token![:]) {
                properties.push(content.parse()?);
            } else {
                // Otherwise, parse as child
                children.push(content.parse()?);
            }

            // Skip optional trailing comma
            let _ = content.parse::<Token![,]>();
        }

        Ok(ComponentBlock {
            component_type,
            properties,
            children,
        })
    }
}

impl Parse for Property {
    fn parse(input: ParseStream) -> Result<Self> {
        let name: Ident = input.parse()?;
        input.parse::<Token![:]>()?;
        let value: Expr = input.parse()?;

        Ok(Property { name, value })
    }
}

impl Parse for Child {
    fn parse(input: ParseStream) -> Result<Self> {
        // Check if this is a component block (starts with an identifier followed by '{')
        if input.peek(Ident) && input.peek2(syn::token::Brace) {
            Ok(Child::Component(input.parse()?))
        } else {
            // Otherwise, parse as an expression (for conditionals, loops, etc.)
            Ok(Child::Expr(input.parse()?))
        }
    }
}

/// Helper to check if the next two tokens match a pattern
trait Peek2 {
    fn peek2<T: syn::parse::Peek>(&self, token: T) -> bool;
}

impl Peek2 for ParseStream<'_> {
    fn peek2<T: syn::parse::Peek>(&self, token: T) -> bool {
        let ahead = self.fork();
        ahead.parse::<Ident>().is_ok() && ahead.peek(token)
    }
}
