use kryon::prelude::*;

fn main() {
    // Create reactive state
    let mut count = signal(0);

    // Build UI with reactive bindings
    let doc = kryon_app! {
        Column {
            width: "100%",
            height: "100%",
            padding: 30.0,
            gap: 20.0,
            background: "#1a1a2e",
            justify_content: Alignment::Center,
            align_items: Alignment::Center,

            Text {
                content: "Reactive Counter Demo",
                font_size: 32.0,
                color: "#eee",
            },

            Text {
                content: format!("Count: {}", count.get()),
                font_size: 48.0,
                color: "#fff",
            },

            Row {
                gap: 16.0,

                Button {
                    text: "-",
                    background: "#e74c3c",
                    color: "#fff",
                    padding: 15.0,
                    width: "80px",
                },

                Button {
                    text: "Reset",
                    background: "#95a5a6",
                    color: "#fff",
                    padding: 15.0,
                    width: "80px",
                },

                Button {
                    text: "+",
                    background: "#2ecc71",
                    color: "#fff",
                    padding: 15.0,
                    width: "80px",
                },
            },

            Text {
                content: "Click buttons to update count (handler logic TBD)",
                font_size: 14.0,
                color: "#999",
            },
        }
    };

    // Write to file
    match doc.write_to_file("/tmp/rust_counter.kir") {
        Ok(_) => println!("✓ Generated /tmp/rust_counter.kir"),
        Err(e) => eprintln!("✗ Error: {}", e),
    }

    // Print summary
    println!("\nGenerated reactive counter UI:");
    println!("- 3 buttons (-, Reset, +)");
    println!("- Reactive count display: {}", count.get());
    println!("\nNote: Event handler binding will be implemented in next phase");
}
