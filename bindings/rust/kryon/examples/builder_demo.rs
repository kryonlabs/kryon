use kryon::prelude::*;

fn main() {
    // Counter for component IDs
    let mut id = 1;

    // Build a UI using the builder API
    let root = column()
        .width("100%")
        .height("100%")
        .background("#1a1a2e")
        .padding(30.0)
        .gap(20.0)
        .justify_content(Alignment::Center)
        .align_items(Alignment::Center)
        .child(
            text("Kryon Rust Builder API")
                .font_size(32.0)
                .color("#eee")
                .build(id),
        )
        .child({
            id += 1;
            row()
                .gap(15.0)
                .child({
                    id += 1;
                    button("Action")
                        .background("#16213e")
                        .color("#fff")
                        .padding(15.0)
                        .build(id)
                })
                .child({
                    id += 1;
                    button("Cancel")
                        .background("#533483")
                        .color("#fff")
                        .padding(15.0)
                        .build(id)
                })
                .build({
                    id += 1;
                    id
                })
        })
        .build(0);

    // Create IR document
    let doc = IRDocument::new(root);

    // Write to file
    match doc.write_to_file("/tmp/rust_builder.kir") {
        Ok(_) => println!("✓ Generated /tmp/rust_builder.kir"),
        Err(e) => eprintln!("✗ Error: {}", e),
    }

    // Print JSON
    println!("\n{}", doc.to_json().unwrap());
}
