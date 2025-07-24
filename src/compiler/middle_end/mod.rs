// FILE: src/compiler/middle_end/mod.rs (Updated)

// Declare all the files within this module
pub mod module_context;
pub mod preprocessor;
pub mod style_resolver;
pub mod component_resolver;
pub mod script;
pub mod script_compiler;
pub mod variable_context;

// Also add a helper function here that was previously in compiler/mod.rs
// This function sets up the variable context from the module graph.
use crate::core::CompilerState;
use crate::core::util::is_valid_identifier;
use crate::error::Result;
use module_context::ModuleGraph;

pub fn setup_from_module_graph(
    state: &mut CompilerState,
    graph: &ModuleGraph,
    options: &crate::CompilerOptions,
) -> Result<()> {
    state.variable_context.set_current_module(graph.root_module.clone());

    for module in graph.get_ordered_modules() {
        state.variable_context.add_module_variables(module)?;
        for (name, var_def) in &module.variables {
            if !module.is_private(name) || module.file_path == graph.root_module {
                state.variables.insert(name.clone(), var_def.clone());
            }
        }
    }

    for (name, value) in &options.custom_variables {
        if !is_valid_identifier(name) {
            return Err(crate::error::CompilerError::Variable {
                file: "".to_string(), line: 0,
                message: format!("Invalid custom variable name '{}'", name)
            });
        }
        state.variables.insert(name.clone(), crate::core::VariableDef {
            value: value.clone(), raw_value: value.clone(), def_line: 0, is_resolving: false, is_resolved: true
        });
        state.variable_context.add_string_variable(
            name.clone(), value.clone(), "custom".to_string(), 0
        )?;
    }
    Ok(())
}