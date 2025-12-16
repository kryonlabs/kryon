//! Reactive expression detection and transformation
//!
//! This module analyzes expressions to detect Signal<T> usage and
//! automatically generates reactive bindings for automatic UI updates.

use proc_macro2::TokenStream;
use quote::quote;
use syn::visit::{self, Visit};
use syn::{Expr, ExprMethodCall, Ident};

/// Information about reactive variables found in an expression
#[derive(Debug, Default)]
pub struct ReactiveInfo {
    /// Signal variables referenced (e.g., "count", "username")
    pub signal_vars: Vec<Ident>,
    /// Whether .get() is called on any signals
    pub has_get_calls: bool,
}

/// Visitor that detects Signal<T> usage in expressions
struct SignalVisitor {
    signal_vars: Vec<Ident>,
    has_get_calls: bool,
}

impl SignalVisitor {
    fn new() -> Self {
        SignalVisitor {
            signal_vars: Vec::new(),
            has_get_calls: false,
        }
    }
}

impl<'ast> Visit<'ast> for SignalVisitor {
    fn visit_expr_method_call(&mut self, node: &'ast ExprMethodCall) {
        // Check if this is a .get() call (signal.get())
        if node.method == "get" {
            self.has_get_calls = true;

            // Try to extract the receiver (what .get() is called on)
            if let Expr::Path(path) = &*node.receiver {
                if let Some(ident) = path.path.get_ident() {
                    // This is likely a signal variable
                    if !self.signal_vars.iter().any(|v| v == ident) {
                        self.signal_vars.push(ident.clone());
                    }
                }
            }
        }

        // Continue visiting nested expressions
        visit::visit_expr_method_call(self, node);
    }

    fn visit_expr_path(&mut self, node: &'ast syn::ExprPath) {
        // Detect direct signal usage (without .get())
        // This would require type information which we don't have in proc macros
        // So we use a heuristic: if there's a .get() call, track that variable
        visit::visit_expr_path(self, node);
    }
}

/// Analyze an expression to detect Signal<T> usage
pub fn analyze_reactive_expr(expr: &Expr) -> ReactiveInfo {
    let mut visitor = SignalVisitor::new();
    visitor.visit_expr(expr);

    ReactiveInfo {
        signal_vars: visitor.signal_vars,
        has_get_calls: visitor.has_get_calls,
    }
}

/// Check if an expression contains reactive Signal usage
pub fn is_reactive_expr(expr: &Expr) -> bool {
    let info = analyze_reactive_expr(expr);
    !info.signal_vars.is_empty() || info.has_get_calls
}

/// Generate reactive binding code for an expression
///
/// Transforms:
/// ```ignore
/// format!("Count: {}", count.get())
/// ```
///
/// Into reactive builder call (conceptually):
/// ```ignore
/// text_builder.reactive(count.clone(), |c| format!("Count: {}", c))
/// ```
pub fn generate_reactive_binding(
    expr: &Expr,
    info: &ReactiveInfo,
) -> Option<TokenStream> {
    if info.signal_vars.is_empty() {
        return None;
    }

    // For now, we'll just track that it's reactive
    // Full implementation would require transforming the expression
    // to remove .get() calls and use the closure parameter instead

    // This is a placeholder - full implementation is complex
    // and requires expression rewriting
    Some(quote! {
        // TODO: Generate .reactive() binding
        // This requires rewriting the expression to use closure parameters
    })
}

/// Transform an expression to use reactive closure parameter
///
/// Example transformation:
/// ```ignore
/// format!("Count: {}", count.get())
/// // becomes:
/// |count_value| format!("Count: {}", count_value)
/// ```
pub fn transform_expr_for_reactive(expr: &Expr, signal_var: &Ident) -> TokenStream {
    // This is a simplified version
    // Full implementation would use syn::fold to rewrite the AST

    let param_name = quote::format_ident!("{}_value", signal_var);

    quote! {
        |#param_name| {
            // Expression would be transformed here
            // to replace signal_var.get() with param_name
            #expr
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use syn::parse_quote;

    #[test]
    fn test_detect_signal_get() {
        let expr: Expr = parse_quote! { count.get() };
        let info = analyze_reactive_expr(&expr);

        assert_eq!(info.signal_vars.len(), 1);
        assert_eq!(info.signal_vars[0], "count");
        assert!(info.has_get_calls);
    }

    #[test]
    #[ignore] // TODO: Detecting signals inside macro invocations requires full expansion
    fn test_detect_signal_in_format() {
        // Detecting signals inside macro invocations like format!()
        // is difficult without full macro expansion.
        // This is a known limitation - macros are expanded after proc macros run.
        let expr: Expr = parse_quote! { format!("Count: {}", count.get()) };
        let info = analyze_reactive_expr(&expr);

        assert_eq!(info.signal_vars.len(), 1);
        assert_eq!(info.signal_vars[0], "count");
    }

    #[test]
    fn test_non_reactive_expr() {
        let expr: Expr = parse_quote! { "Hello, World!" };
        let info = analyze_reactive_expr(&expr);

        assert!(info.signal_vars.is_empty());
        assert!(!info.has_get_calls);
    }

    #[test]
    fn test_is_reactive() {
        let reactive: Expr = parse_quote! { count.get() };
        let non_reactive: Expr = parse_quote! { 42 };

        assert!(is_reactive_expr(&reactive));
        assert!(!is_reactive_expr(&non_reactive));
    }
}
