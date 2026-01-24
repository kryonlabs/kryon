// Kryon Hare Frontend - DSL Functions
//
// This module provides the DSL functions for building UI components.
// These are the main functions users will call to create UIs.

use types::c;
use fmt;
use strings;

// ============================================================================
// Current Component Context
// ============================================================================

// Global context for building components
let current_context: *c::void = null;
let current_component: *c::void = null;

// ============================================================================
// Helper Functions
// ============================================================================

// Convert Hare string to C string (must be freed with ir_free_string)
fn str_to_c(s: str) *c::char = {
	const cstr = strings::to_c(s);
	return cstr: *c::char;
};

// ============================================================================
// Component Builders
// ============================================================================

// Begin a new component
export fn component_start(comp_type: str) void = {
	if (current_component != null) {
		// TODO: Handle nested components properly
		return;
	};
	const ctype = str_to_c(comp_type);
	current_component = ir_component_create(ctype);
};

// Set a string property on the current component
export fn set_string(name: str, value: str) void = {
	if (current_component == null) return;
	const cname = str_to_c(name);
	const cvalue = str_to_c(value);
	ir_component_set_string(current_component, cname, cvalue);
};

// Set a numeric property on the current component
export fn set_number(name: str, value: f64) void = {
	if (current_component == null) return;
	const cname = str_to_c(name);
	ir_component_set_number(current_component, cname, value);
};

// Set a boolean property on the current component
export fn set_bool(name: str, value: bool) void = {
	if (current_component == null) return;
	const cname = str_to_c(name);
	const cvalue = if (value) 1 else 0;
	ir_component_set_bool(current_component, cname, cvalue);
};

// Add a child component to the current component
export fn add_child(child: *c::void) void = {
	if (current_component == null) return;
	ir_component_add_child(current_component, child);
};

// Finish building the current component and return it
export fn component_finish() *c::void = {
	const comp = current_component;
	current_component = null;
	return comp;
};

// ============================================================================
// Convenience Functions - Container
// ============================================================================

export type container_builder = struct {
	component: *c::void,
};

export fn container(fn: fn(*container_builder) void) *c::void = {
	const comp = ir_component_create(str_to_c("Container"));
	const builder = &container_builder { component = comp };
	fn(builder);
	return comp;
};

export fn (b: *container_builder) width(value: str) void = {
	ir_component_set_string(b.component, str_to_c("width"), str_to_c(value));
};

export fn (b: *container_builder) height(value: str) void = {
	ir_component_set_string(b.component, str_to_c("height"), str_to_c(value));
};

export fn (b: *container_builder) padding(value: u32) void = {
	ir_component_set_number(b.component, str_to_c("padding"), value: f64);
};

export fn (b: *container_builder) background_color(value: str) void = {
	ir_component_set_string(b.component, str_to_c("backgroundColor"), str_to_c(value));
};

export fn (b: *container_builder) child(child: *c::void) void = {
	ir_component_add_child(b.component, child);
};

// ============================================================================
// Convenience Functions - Row
// ============================================================================

export type row_builder = struct {
	component: *c::void,
};

export fn row(fn: fn(*row_builder) void) *c::void = {
	const comp = ir_component_create(str_to_c("Row"));
	const builder = &row_builder { component = comp };
	fn(builder);
	return comp;
};

export fn (b: *row_builder) gap(value: u32) void = {
	ir_component_set_number(b.component, str_to_c("gap"), value: f64);
};

export fn (b: *row_builder) align_items(value: str) void = {
	ir_component_set_string(b.component, str_to_c("alignItems"), str_to_c(value));
};

export fn (b: *row_builder) justify_content(value: str) void = {
	ir_component_set_string(b.component, str_to_c("justifyContent"), str_to_c(value));
};

export fn (b: *row_builder) width(value: str) void = {
	ir_component_set_string(b.component, str_to_c("width"), str_to_c(value));
};

export fn (b: *row_builder) height(value: str) void = {
	ir_component_set_string(b.component, str_to_c("height"), str_to_c(value));
};

export fn (b: *row_builder) child(child: *c::void) void = {
	ir_component_add_child(b.component, child);
};

// ============================================================================
// Convenience Functions - Column
// ============================================================================

export type column_builder = struct {
	component: *c::void,
};

export fn column(fn: fn(*column_builder) void) *c::void = {
	const comp = ir_component_create(str_to_c("Column"));
	const builder = &column_builder { component = comp };
	fn(builder);
	return comp;
};

export fn (b: *column_builder) gap(value: u32) void = {
	ir_component_set_number(b.component, str_to_c("gap"), value: f64);
};

export fn (b: *column_builder) align_items(value: str) void = {
	ir_component_set_string(b.component, str_to_c("alignItems"), str_to_c(value));
};

export fn (b: *column_builder) justify_content(value: str) void = {
	ir_component_set_string(b.component, str_to_c("justifyContent"), str_to_c(value));
};

export fn (b: *column_builder) width(value: str) void = {
	ir_component_set_string(b.component, str_to_c("width"), str_to_c(value));
};

export fn (b: *column_builder) height(value: str) void = {
	ir_component_set_string(b.component, str_to_c("height"), str_to_c(value));
};

export fn (b: *column_builder) child(child: *c::void) void = {
	ir_component_add_child(b.component, child);
};

// ============================================================================
// Convenience Functions - Text
// ============================================================================

export type text_builder = struct {
	component: *c::void,
};

export fn text(fn: fn(*text_builder) void) *c::void = {
	const comp = ir_component_create(str_to_c("Text"));
	const builder = &text_builder { component = comp };
	fn(builder);
	return comp;
};

export fn (b: *text_builder) text(value: str) void = {
	ir_component_set_string(b.component, str_to_c("text"), str_to_c(value));
};

export fn (b: *text_builder) font_size(value: u32) void = {
	ir_component_set_number(b.component, str_to_c("fontSize"), value: f64);
};

export fn (b: *text_builder) font_weight(value: u32) void = {
	ir_component_set_number(b.component, str_to_c("fontWeight"), value: f64);
};

export fn (b: *text_builder) color(value: str) void = {
	ir_component_set_string(b.component, str_to_c("color"), str_to_c(value));
};

export fn (b: *text_builder) text_align(value: str) void = {
	ir_component_set_string(b.component, str_to_c("textAlign"), str_to_c(value));
};

// ============================================================================
// Convenience Functions - Button
// ============================================================================

export type button_builder = struct {
	component: *c::void,
};

export fn button(fn: fn(*button_builder) void) *c::void = {
	const comp = ir_component_create(str_to_c("Button"));
	const builder = &button_builder { component = comp };
	fn(builder);
	return comp;
};

export fn (b: *button_builder) text(value: str) void = {
	ir_component_set_string(b.component, str_to_c("text"), str_to_c(value));
};

export fn (b: *button_builder) width(value: str) void = {
	ir_component_set_string(b.component, str_to_c("width"), str_to_c(value));
};

export fn (b: *button_builder) height(value: str) void = {
	ir_component_set_string(b.component, str_to_c("height"), str_to_c(value));
};

// ============================================================================
// Convenience Functions - Input
// ============================================================================

export type input_builder = struct {
	component: *c::void,
};

export fn input(fn: fn(*input_builder) void) *c::void = {
	const comp = ir_component_create(str_to_c("Input"));
	const builder = &input_builder { component = comp };
	fn(builder);
	return comp;
};

export fn (b: *input_builder) placeholder(value: str) void = {
	ir_component_set_string(b.component, str_to_c("placeholder"), str_to_c(value));
};

export fn (b: *input_builder) text(value: str) void = {
	ir_component_set_string(b.component, str_to_c("text"), str_to_c(value));
};

export fn (b: *input_builder) width(value: str) void = {
	ir_component_set_string(b.component, str_to_c("width"), str_to_c(value));
};
