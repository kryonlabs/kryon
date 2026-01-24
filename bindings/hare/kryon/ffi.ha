// Kryon Hare Frontend - C FFI Bindings
//
// This module provides direct bindings to the Kryon IR C library.

use types::c;

// ============================================================================
// IR Context Management
// ============================================================================

// Create a new IR context
@symbol("ir_create_context") @cfunc extern fn ir_create_context() *c::void;

// Set the current IR context
@symbol("ir_set_context") @cfunc extern fn ir_set_context(ctx: *c::void) void;

// Get the current IR context
@symbol("ir_get_context") @cfunc extern fn ir_get_context() *c::void;

// Free an IR context
@symbol("ir_free_context") @cfunc extern fn ir_free_context(ctx: *c::void) void;

// ============================================================================
// Component Creation
// ============================================================================

// Create a component
@symbol("ir_component_create") @cfunc extern fn ir_component_create(
	type: *c::char,
) *c::void;

// Set a string property on a component
@symbol("ir_component_set_string") @cfunc extern fn ir_component_set_string(
	comp: *c::void,
	name: *c::char,
	value: *c::char,
) c::int;

// Set a numeric property on a component
@symbol("ir_component_set_number") @cfunc extern fn ir_component_set_number(
	comp: *c::void,
	name: *c::char,
	value: f64,
) c::int;

// Set a boolean property on a component
@symbol("ir_component_set_bool") @cfunc extern fn ir_component_set_bool(
	comp: *c::void,
	name: *c::char,
	value: c::int,
) c::int;

// Add a child component
@symbol("ir_component_add_child") @cfunc extern fn ir_component_add_child(
	parent: *c::void,
	child: *c::void,
) c::int;

// ============================================================================
// Application Management
// ============================================================================

// Create the root application component
@symbol("ir_create_app") @cfunc extern fn ir_create_app(
	width: c::int,
	height: c::int,
	title: *c::char,
) *c::void;

// Set the root component
@symbol("ir_set_root") @cfunc extern fn ir_set_root(root: *c::void) c::int;

// Update the application
@symbol("ir_update") @cfunc extern fn ir_update() c::int;

// Render the application
@symbol("ir_render") @cfunc extern fn ir_render() c::int;

// ============================================================================
// Serialization
// ============================================================================

// Serialize IR to JSON
@symbol("ir_serialize_to_json") @cfunc extern fn ir_serialize_to_json(
	comp: *c::void,
	out_json: **c::char,
) c::int;

// Deserialize IR from JSON
@symbol("ir_deserialize_from_json") @cfunc extern fn ir_deserialize_from_json(
	json: *c::char,
) *c::void;

// Free serialized JSON string
@symbol("ir_free_json") @cfunc extern fn ir_free_json(json: *c::char) void;

// ============================================================================
// State Management
// ============================================================================

// Create a reactive state
@symbol("ir_state_create") @cfunc extern fn ir_state_create(
	initial_value: *c::void,
	value_type: c::int,
) *c::void;

// Get state value
@symbol("ir_state_get") @cfunc extern fn ir_state_get(
	state: *c::void,
) *c::void;

// Set state value
@symbol("ir_state_set") @cfunc extern fn ir_state_set(
	state: *c::void,
	value: *c::void,
) void;

// Subscribe to state changes
@symbol("ir_state_subscribe") @cfunc extern fn ir_state_subscribe(
	state: *c::void,
	callback: *fn(*c::void, *c::void) void,
	userdata: *c::void,
) c::int;

// ============================================================================
// Memory Management
// ============================================================================

// Free a component
@symbol("ir_component_free") @cfunc extern fn ir_component_free(comp: *c::void) void;

// Free a string allocated by the IR library
@symbol("ir_free_string") @cfunc extern fn ir_free_string(s: *c::char) void;
