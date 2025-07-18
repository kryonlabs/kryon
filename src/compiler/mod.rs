// FILE: src/compiler/mod.rs

// This file defines the main "compiler" module and orchestrates the pipeline.

// 1. Declare the stages of the compiler as sub-modules.
//    Rust will look for these as directories inside `src/compiler/`.
mod backend;
pub(crate) mod frontend;
pub(crate) mod middle_end;
mod optimizer;
mod bundler;

// 2. Bring necessary types and modules into the current scope.
use crate::core::*;
use crate::core::util::is_valid_identifier;
use crate::error::{CompilerError, Result};
use crate::{CompilerOptions, CompilationStats}; // Refers to the struct in src/lib.rs

use crate::compiler::frontend::semantic::{SemanticAnalyzer, convert_ast_to_state};
use crate::compiler::frontend::ast::AstNode;
use crate::compiler::frontend::parse_module_graph_to_ast;
use crate::compiler::middle_end::module_context::ModuleGraph;
use crate::compiler::middle_end::style_resolver::{StyleResolver, apply_style_properties_to_elements};
use crate::compiler::middle_end::setup_from_module_graph;
use crate::compiler::middle_end::script::{process_template_variables, collect_function_templates, process_resolved_scripts};

use crate::compiler::middle_end::component_resolver::ComponentResolver;
use crate::compiler::backend::size_calculator::SizeCalculator;
use crate::compiler::backend::codegen::CodeGenerator;

/// The main entry point for compiling a file.
/// This function orchestrates the entire pipeline from source to binary.
pub fn compile_with_options(
    input_path: &str,
    output_path: &str,
    options: CompilerOptions,
) -> Result<CompilationStats> {
    use std::fs;
    use std::time::Instant;

    let start_time = Instant::now();
    let source_size = fs::metadata(input_path)?.len();

    // =======================================================
    // THE COMPILER PIPELINE
    // =======================================================

    // STAGE 1: PREPROCESSING (Middle-End)
    // Handle @includes and build the module graph before any parsing.
    let mut preprocessor = middle_end::preprocessor::Preprocessor::new();
    let module_graph = preprocessor.process_includes_isolated(input_path)?;

    // STAGE 2: SETUP
    // Initialize the main CompilerState and the VariableContext from the module graph.
    let mut state = CompilerState::new();
    state.current_file_path = input_path.to_string();
    setup_from_module_graph(&mut state, &module_graph, &options)?;

    // STAGE 3: PARSING (Frontend)
    // Parse all modules into a single Abstract Syntax Tree (AST).
    let mut ast = frontend::parse_module_graph(&module_graph, &options)?;

    // STAGE 4: SEMANTIC ANALYSIS (Frontend)
    // Collect definitions (styles, components) from the AST and perform initial validation.
    let mut semantic_analyzer = frontend::semantic::SemanticAnalyzer::new();
    semantic_analyzer.analyze(&mut ast, &mut state)?;

    // STAGE 4.5: FUNCTION TEMPLATE COLLECTION (Middle-End)
    // Collect function templates from component definitions for later instantiation.
    collect_function_templates(&ast, &mut state)?;

    // STAGE 5: RESOLUTION (Middle-End)
    // Resolve style inheritance and expand component instances in the AST.
    let mut style_resolver = middle_end::style_resolver::StyleResolver::new();
    style_resolver.resolve_all_styles(&mut state)?;

    // Component resolution - templates are now available from semantic analysis
    let component_count = state.component_defs.len();
    println!("DEBUG: Component count in main compile: {}", component_count);
    
    // Always process template structures (@for loops, @if statements) even if no components
    let mut component_resolver = middle_end::component_resolver::ComponentResolver::new();
    component_resolver.resolve_components(&mut ast, &mut state)?;

    // STAGE 5.5: RESOLVED SCRIPT PROCESSING (Middle-End)
    // Convert resolved function templates to script entries for the KRB
    process_resolved_scripts(&mut state)?;

    // STAGE 6: STATE CONVERSION
    // Convert the final, resolved AST into the internal `CompilerState` representation.
    convert_ast_to_state(&ast, &mut state)?;
    apply_style_properties_to_elements(&mut state)?;
    process_template_variables(&mut state, &options)?;

    // STAGE 7: OPTIMIZATION
    // Run optimization passes on the generated internal state.
    let mut optim = optimizer::Optimizer::new();
    optim.optimize(&mut state, options.optimization_level)?;

    // STAGE 8: CODE GENERATION (Backend)
    // Calculate final sizes, offsets, and generate the binary KRB data.
    let size_calculator = backend::size_calculator::SizeCalculator::new();
    size_calculator.calculate_sizes(&mut state)?;
    size_calculator.validate_limits(&mut state)?;

    let mut code_generator = backend::codegen::CodeGenerator::new();
    let krb_data = code_generator.generate(&mut state)?;

    // =======================================================
    // FINAL STATS & OUTPUT
    // =======================================================

    let mut stats = CompilationStats::default();
    stats.source_size = source_size;
    stats.output_size = krb_data.len() as u64;
    stats.compression_ratio = if source_size > 0 {
        stats.output_size as f64 / source_size as f64
    } else {
        0.0
    };
    stats.compile_time_ms = start_time.elapsed().as_millis() as u64;
    stats.element_count = state.elements.len();
    stats.style_count = state.styles.len();
    stats.component_count = state.component_defs.len();
    stats.script_count = state.scripts.len();
    stats.resource_count = state.resources.len();
    stats.string_count = state.strings.len();
    stats.include_count = module_graph.modules.len();
    stats.variable_count = state.variables.len();
    
    fs::write(output_path, krb_data)?;

    Ok(stats)
}


/// NEW: Compile with a module graph (module-aware compilation)
pub fn compile_with_module_graph(
    module_graph: &ModuleGraph,
    filename: &str,
    options: CompilerOptions
) -> Result<(Vec<u8>, CompilationStats)> {
    let mut state = CompilerState::new();
    state.current_file_path = filename.to_string();
    
    let mut stats = CompilationStats::default();
    
    if options.debug_mode {
        log::debug!("Starting module-aware compilation pipeline for {}", filename);
        log::debug!("Module graph has {} modules", module_graph.modules.len());
    }
    
    // Phase 0.1: Module processing - already done in preprocessor
    if options.debug_mode {
        log::debug!("Phase 0.1: Module processing complete");
    }
    
    // Phase 0.2: Process variables with module isolation
    if options.debug_mode {
        log::debug!("Phase 0.2: Processing variables with module isolation...");
    }
    
    // Set up the variable context with module support
    let root_module = module_graph.modules.get(&module_graph.root_module)
        .ok_or_else(|| CompilerError::Include { 
            message: "Root module not found in module graph".to_string() 
        })?;
    
    // Set current module context
    state.variable_context.set_current_module(module_graph.root_module.clone());
    
    // Add variables from all modules in dependency order
    for module in module_graph.get_ordered_modules() {
        state.variable_context.add_module_variables(module)?;
        
        // Also add to legacy variables map for backward compatibility
        for (name, var_def) in &module.variables {
            if !module.is_private(name) || module.file_path == module_graph.root_module {
                state.variables.insert(name.clone(), var_def.clone());
            }
        }
    }
    
    // Inject custom variables
    for (name, value) in &options.custom_variables {
        if !is_valid_identifier(name) {
            return Err(CompilerError::variable_legacy(
                0,
                format!("Invalid custom variable name '{}'", name)
            ));
        }
        
        state.variables.insert(name.clone(), crate::core::VariableDef {
            value: value.clone(),
            raw_value: value.clone(),
            def_line: 0,
            is_resolving: false,
            is_resolved: true,
        });
        
        state.variable_context.add_string_variable(
            name.clone(),
            value.clone(),
            "custom".to_string(),
            0
        )?;
    }
    
    stats.variable_count = state.variables.len();
    
    if options.debug_mode {
        log::debug!("Phase 0.2 complete. Variables resolved: {}", stats.variable_count);
    }
    
    // Phase 1: Lexical analysis and parsing
    if options.debug_mode {
        log::debug!("Phase 1: Lexical analysis and parsing...");
    }
    
    // Parse each module separately to preserve component template boundaries
    let ast = parse_module_graph_to_ast(module_graph, &options)?;
    
    if options.debug_mode {
        log::debug!("Phase 1 complete. AST parsed successfully");
    }
    
    // Continue with remaining phases using the existing pipeline
    let result = compile_ast_with_state(ast, &mut state, &options)?;
    
    stats.element_count = state.elements.len();
    stats.style_count = state.styles.len();
    stats.component_count = state.component_defs.len();
    stats.script_count = state.scripts.len();
    stats.resource_count = state.resources.len();
    stats.string_count = state.strings.len();
    stats.include_count = module_graph.modules.len();
    
    Ok((result, stats))
}


/// Helper function to continue compilation from AST with existing state
fn compile_ast_with_state(
    mut ast: AstNode,
    state: &mut CompilerState,
    options: &CompilerOptions
) -> Result<Vec<u8>> {
    // Phase 1.2: Semantic analysis
    if options.debug_mode {
        log::debug!("Phase 1.2: Semantic analysis...");
    }
    
    println!("DEBUG: Before semantic analysis");
    
    let mut semantic_analyzer = SemanticAnalyzer::new();
    semantic_analyzer.analyze(&mut ast, state)?;
    
    if options.debug_mode {
        log::debug!("Phase 1.2 complete. Semantic analysis passed");
    }
    
    // Phase 1.25: Style resolution
    if options.debug_mode {
        log::debug!("Phase 1.25: Resolving style inheritance...");
    }
    
    let mut style_resolver = StyleResolver::new();
    style_resolver.resolve_all_styles(state)?;
    
    if options.debug_mode {
        log::debug!("Phase 1.25 complete. Style inheritance resolved");
    }
    
    // Phase 1.3: Component resolution and template structure processing (moved before AST conversion)
    if options.debug_mode {
        log::debug!("Phase 1.3: Resolving components and template structures...");
    }
    
    let component_count = state.component_defs.len();
    
    // Always process template structures (@for loops, @if statements) even if no components
    let mut component_resolver = ComponentResolver::new();
    component_resolver.resolve_components(&mut ast, state)?;
    
    if options.debug_mode {
        log::debug!("Phase 1.3 complete. Template structures and component instances resolved in AST");
    }
    
    // Clear component state since components are now expanded to regular elements
    state.component_defs.clear();
    state.component_ast_templates.clear();
    
    // Phase 1.4: Convert AST to internal representation
    if options.debug_mode {
        log::debug!("Phase 1.4: Converting AST to internal representation...");
    }
    
    convert_ast_to_state(&ast, state)?;
    
    // Phase 1.5: Apply style properties to elements
    if options.debug_mode {
        log::debug!("Phase 1.5: Applying style properties to elements...");
    }
    
    apply_style_properties_to_elements(state)?;
    
    if options.debug_mode {
        log::debug!("Phase 1.5 complete. Style properties applied to elements");
        log::debug!("Elements: {}, Styles: {}", state.elements.len(), state.styles.len());
    }
    
    // Phase 1.6: Process template variables
    if options.debug_mode {
        log::debug!("Phase 1.6: Processing template variables...");
    }
    
    process_template_variables(state, options)?;
    
    if options.debug_mode {
        log::debug!("Phase 1.6 complete. Template variables: {}, bindings: {}", 
                   state.template_variables.len(), state.template_bindings.len());
    }
    
    // Phase 2: Calculate sizes
    if options.debug_mode {
        log::debug!("Phase 2: Calculating sizes...");
    }
    
    let size_calculator = SizeCalculator::new();
    size_calculator.calculate_sizes(state)?;
    size_calculator.validate_limits(state)?;
    
    let size_stats = size_calculator.get_size_stats(state);
    
    if options.debug_mode {
        log::debug!("Phase 2 complete. Total size: {} bytes", size_stats.total_size);
        if options.optimization_level >= 2 {
            size_stats.print_breakdown();
        }
    }
    
    // Phase 3: Generate KRB binary
    if options.debug_mode {
        log::debug!("Phase 3: Generating KRB binary...");
    }
    
    let mut code_generator = CodeGenerator::new();
    let krb_data = code_generator.generate(state)?;

    if options.debug_mode {
        log::debug!("Phase 3 complete. KRB data size: {} bytes", krb_data.len());
    }
    
    Ok(krb_data)
}

