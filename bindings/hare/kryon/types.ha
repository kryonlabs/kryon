// Kryon Hare Frontend - Type Definitions
//
// This module provides type definitions for the Kryon IR bindings.
// Hare can directly call C functions using @cfunc and @symbol attributes.

use types::c;

// ============================================================================
// C FFI Types
// ============================================================================

// Pointer types (opaque C pointers)
export type IRContext = *c::void;
export type IRComponent = *c::void;
export type IRState = *c::void;
export type IREffect = *c::void;

// ============================================================================
// Component Property Types
// ============================================================================

// Dimension values can be pixels, percentages, or "auto"
export type dimension = struct {
	value: f64,
	unit: dimension_unit,
};

export type dimension_unit = enum {
	PIXELS,
	PERCENT,
	AUTO,
};

// Color representation (RGBA)
export type color = struct {
	r: u8,
	g: u8,
	b: u8,
	a: u8,
};

// Alignment options
export type alignment = enum {
	START,
	CENTER,
	END,
	STRETCH,
};

// Text alignment
export type text_align = enum {
	LEFT,
	CENTER,
	RIGHT,
	JUSTIFY,
};

// ============================================================================
// Component Types (Tagged Union)
// ============================================================================

export type component = union {
	container: container_props,
	row: flex_props,
	column: flex_props,
	text: text_props,
	button: button_props,
	input: input_props,
	checkbox: checkbox_props,
	image: image_props,
	canvas: canvas_props,
	tab_group: tab_group_props,
	tab_bar: tab_bar_props,
	tab_panel: tab_panel_props,
};

// ============================================================================
// Common Props
// ============================================================================

export type base_props = struct {
	width: dimension,
	height: dimension,
	padding: u32,
	margin: u32,
	background_color: color,
	opacity: f64,
	visible: bool,
};

// ============================================================================
// Container Props
// ============================================================================

export type container_props = struct {
	base: base_props,
	children: []component,
};

// ============================================================================
// Flexbox Props (Row/Column)
// ============================================================================

export type flex_props = struct {
	base: base_props,
	children: []component,
	gap: u32,
	align_items: alignment,
	justify_content: alignment,
};

// ============================================================================
// Text Props
// ============================================================================

export type text_props = struct {
	base: base_props,
	text: str,
	font_size: u32,
	font_weight: u32,
	color: color,
	text_align: text_align,
};

// ============================================================================
// Button Props
// ============================================================================

export type button_props = struct {
	base: base_props,
	text: str,
	on_click: *fn() void,
};

// ============================================================================
// Input Props
// ============================================================================

export type input_props = struct {
	base: base_props,
	placeholder: str,
	text: str,
	on_change: *fn(str) void,
};

// ============================================================================
// Checkbox Props
// ============================================================================

export type checkbox_props = struct {
	base: base_props,
	checked: bool,
	on_change: *fn(bool) void,
};

// ============================================================================
// Image Props
// ============================================================================

export type image_props = struct {
	base: base_props,
	src: str,
	alt: str,
};

// ============================================================================
// Canvas Props
// ============================================================================

export type canvas_props = struct {
	base: base_props,
	width: u32,
	height: u32,
	on_draw: *fn() void,
};

// ============================================================================
// Tab Group Props
// ============================================================================

export type tab_group_props = struct {
	base: base_props,
	children: []component,
};

// ============================================================================
// Tab Bar Props
// ============================================================================

export type tab_bar_props = struct {
	base: base_props,
	children: []component,
};

// ============================================================================
// Tab Panel Props
// ============================================================================

export type tab_panel_props = struct {
	base: base_props,
	title: str,
	children: []component,
};
