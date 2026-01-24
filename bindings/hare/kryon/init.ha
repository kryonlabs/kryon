// Kryon Hare Frontend - Main Entry Point
//
// This is the main module that users import to use Kryon in Hare.
//
// Usage:
//   use kryon::*;
//
//   export fn main() void = {
//       const root = container { b => {
//           b.width("100%");
//           b.height("100%");
//           b.child(row { r => {
//               r.gap(10);
//               r.child(text { t => {
//                   t.text("Hello, Hare!");
//               }});
//               r.child(button { btn => {
//                   btn.text("Click me");
//               }});
//           }});
//       }};
//
//       set_root(root)!;
//       run();
//   };

use fmt;
use strings;

// ============================================================================
// Re-export all public APIs
// ============================================================================

// FFI bindings (internal)
use ffi;

// Type definitions
export use types;

// DSL functions for building UIs
export use dsl::{
	container,
	row,
	column,
	text,
	button,
	input,
};

// Runtime functions
export use runtime::{
	init,
	create_app,
	set_root,
	update,
	render,
	run,
	quit,
	cleanup,
};

// ============================================================================
// C String Helpers
// ============================================================================

// Convert Hare string to C string
export fn to_cstr(s: str) *c::char = {
	return strings::to_c(s): *c::char;
};

// Convert C string to Hare string (caller must free the C string)
export fn from_cstr(cs: *c::char) str = {
	if (cs == null) return "";
	const len = c::strlen(cs);
	const bytes = types::c::to_slice(cs: *[*]u8, len);
	return strings::from_utf8_unsafe(bytes);
};

// Free a C string allocated by the IR library
export fn free_cstr(s: *c::char) void = {
	ffi::ir_free_string(s);
};

// ============================================================================
// Example Application
// ============================================================================

// Example "Hello World" application
export fn example_main() void = {
	// Initialize context
	init()!;

	// Build UI
	const root = container { b => {
		b.width("100%");
		b.height("100%");
		b.background_color("#1a1a1a");

		b.child(column { c => {
			c.gap(20);
			c.align_items("center");
			c.justify_content("center");

			c.child(text { t => {
				t.text("Hello from Hare!");
				t.font_size(32);
				t.font_weight(700);
				t.color("#ffffff");
			}});

			c.child(button { btn => {
				btn.text("Click me");
				btn.width("200px");
				btn.height("50px");
			}});
		}});
	}};

	// Set as root and run
	set_root(root)!;
	run();
	cleanup();
};
