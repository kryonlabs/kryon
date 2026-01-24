// Kryon Hare Frontend - Runtime Functions
//
// This module provides runtime functions for running Kryon applications.

use types::c;
use fmt;

// ============================================================================
// Application Context
// ============================================================================

// Global application state
let app_context: *c::void = null;
let is_running: bool = false;

// ============================================================================
// Application Lifecycle
// ============================================================================

// Initialize the Kryon context
export fn init() (*c::void | void) = {
	if (app_context != null) {
		return app_context;
	};
	app_context = ir_create_context();
	if (app_context == null) {
		return void; // Error
	};
	ir_set_context(app_context);
	return app_context;
};

// Create a new application
export fn create_app(width: u32, height: u32, title: str) (*c::void | void) = {
	init();

	const ctitle = strings::to_c(title);
	defer free(ctitle);

	const app = ir_create_app(width: c::int, height: c::int, ctitle: *c::char);
	if (app == null) {
		return void; // Error
	};
	return app;
};

// Set the root component
export fn set_root(root: *c::void) (void | c::int) = {
	const result = ir_set_root(root);
	if (result != 0) {
		return result;
	};
	return;
};

// Update the application (process events, recompute layout, etc.)
export fn update() (void | c::int) = {
	const result = ir_update();
	if (result != 0) {
		return result;
	};
	return;
};

// Render the application
export fn render() (void | c::int) = {
	const result = ir_render();
	if (result != 0) {
		return result;
	};
	return;
};

// Main application loop
export fn run() void = {
	is_running = true;
	for (is_running) {
		match (update()) {
			case void =>
				yield;
			case let err: c::int =>
				fmt::errorfln("Update error: {}", err)!;
				is_running = false;
				break;
		};

		match (render()) {
			case void =>
				yield;
			case let err: c::int =>
				fmt::errorfln("Render error: {}", err)!;
				is_running = false;
				break;
		};
	};
};

// Stop the application
export fn quit() void = {
	is_running = false;
};

// Cleanup resources
export fn cleanup() void = {
	if (app_context != null) {
		ir_free_context(app_context);
		app_context = null;
	};
};
