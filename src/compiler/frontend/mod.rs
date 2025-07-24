// FILE: src/compiler/frontend/mod.rs (New Content)

// Declare the files within this module
pub mod ast;
pub mod lexer;
pub mod parser;
pub mod semantic;

// This helper function was originally in lib.rs or compiler/mod.rs.
// It's part of the frontend's job to produce the AST, so it belongs here.
use compiler::middle_end::module_context::ModuleGraph;
use crate::{compiler, error::Result, CompilerOptions};
use ast::AstNode;
use lexer::Lexer;
use parser::Parser;

pub fn parse_module_graph(graph: &ModuleGraph, options: &crate::CompilerOptions) -> Result<AstNode> {
    let mut combined_styles = Vec::new();
    let mut combined_fonts = Vec::new();
    let mut combined_components = Vec::new();
    let mut combined_scripts = Vec::new();
    let mut combined_directives = Vec::new();
    let mut app_node = None;

    for module in graph.get_ordered_modules() {
        if options.debug_mode {
            log::debug!("Parsing module: {}", module.file_path.display());
        }

        let mut lexer = lexer::Lexer::new_with_source_map(
            &module.content,
            module.file_path.to_string_lossy().to_string(),
            crate::error::SourceMap::new(),
        );
        let tokens = lexer.tokenize()?;

        let mut parser = parser::Parser::new(tokens);
        let module_ast = parser.parse()?;

        if let AstNode::File { app, styles, fonts, components, scripts, directives } = module_ast {
            if module.file_path == graph.root_module {
                app_node = app;
            }
            combined_styles.extend(styles);
            combined_fonts.extend(fonts);
            combined_components.extend(components);
            combined_scripts.extend(scripts);
            combined_directives.extend(directives);
        }
    }

    Ok(AstNode::File {
        app: app_node,
        styles: combined_styles,
        fonts: combined_fonts,
        components: combined_components,
        scripts: combined_scripts,
        directives: combined_directives,
    })
}


/// Temporary helper to merge module graph back to single content
/// TODO: Replace this with proper module-aware compilation
/// Parse module graph to AST while preserving component template boundaries
pub fn parse_module_graph_to_ast(graph: &ModuleGraph, options: &CompilerOptions) -> Result<AstNode> {
    
    let mut _combined_elements: Vec<AstNode> = Vec::new();
    let mut combined_styles = Vec::new(); 
    let mut combined_fonts = Vec::new();
    let mut combined_components = Vec::new();
    let mut combined_scripts = Vec::new();
    let mut combined_directives = Vec::new();
    let mut app_node = None;
    
    // Parse each module separately to preserve component template context
    for module in graph.get_ordered_modules() {
        if options.debug_mode {
            log::debug!("Parsing module: {}", module.file_path.display());
        }
        
        let mut lexer = Lexer::new_with_source_map(
            &module.content, 
            module.file_path.to_string_lossy().to_string(), 
            crate::error::SourceMap::new()
        );
        let tokens = lexer.tokenize()?;
        
        let mut parser = Parser::new(tokens);
        let module_ast = parser.parse()?;
        
        // Extract components from this module
        if let AstNode::File { app, styles, fonts, components, scripts, directives } = module_ast {
            // Take the app from the main module only
            if module.file_path == graph.root_module {
                app_node = app;
            }
            
            // Collect all components, styles, etc.
            combined_styles.extend(styles);
            combined_fonts.extend(fonts);
            combined_components.extend(components);
            combined_scripts.extend(scripts);
            combined_directives.extend(directives);
        }
    }
    
    // Create final combined AST
    Ok(AstNode::File {
        app: app_node,
        styles: combined_styles,
        fonts: combined_fonts,
        components: combined_components,
        scripts: combined_scripts,
        directives: combined_directives,
    })
}



