// Kryon Hare Example - Hello World
//
// A simple "Hello World" application demonstrating the Kryon Hare DSL.

use kryon::*;
use fmt;

export fn main() void = {
	// Initialize Kryon context
	fmt::println("Initializing Kryon...")!;
	match (init()) {
	case void =>
		fmt::errorfln("Failed to initialize Kryon context")!;
		return;
	case let ctx: *c::void =>
		fmt::println("Context initialized")!;
	};

	// Build the UI
	fmt::println("Building UI...")!;

	const root = container { b => {
		b.width("100%");
		b.height("100%");
		b.background_color("#1a1a1a");

		b.child(column { c => {
			c.gap(20);
			c.align_items("center");
			c.justify_content("center");
			c.width("100%");
			c.height("100%");

			// Title
			c.child(text { t => {
				t.text("Hello from Hare!");
				t.font_size(48);
				t.font_weight(700);
				t.color("#00ff00");
				t.text_align("center");
			}});

			// Subtitle
			c.child(text { t => {
				t.text("Kryon UI Framework");
				t.font_size(24);
				t.color("#ffffff");
				t.text_align("center");
			}});

			// Button
			c.child(row { r => {
				r.gap(10);
				r.align_items("center");

				r.child(button { btn => {
					btn.text("Click Me");
					btn.width("200px");
					btn.height("50px");
				}});

				r.child(button { btn => {
					btn.text("Quit");
					btn.width("200px");
					btn.height("50px");
				}});
			}});

			// Input field
			c.child(row { r => {
				r.gap(10);
				r.align_items("center");
				r.width("400px");

				r.child(text { t => {
					t.text("Enter text:");
					t.font_size(16);
					t.color("#cccccc");
				}});

				r.child(input { i => {
					i.placeholder("Type something...");
					i.width("300px");
				}});
			}});
		}});
	}};

	fmt::println("UI built successfully")!;

	// Set as root
	fmt::println("Setting root component...")!;
	match (set_root(root)) {
	case void =>
		fmt::println("Root component set")!;
	case let err: c::int =>
		fmt::errorfln("Failed to set root: {}", err)!;
		cleanup();
		return;
	};

	// Run application
	fmt::println("Starting application...")!;
	fmt::println("Press Ctrl+C to quit")!;
	run();

	// Cleanup
	fmt::println("Cleaning up...")!;
	cleanup();
	fmt::println("Goodbye!")!;
};
