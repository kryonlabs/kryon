use kryon::prelude::*;

fn main() {
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
                content: "Kryon Macro Demo",
                font_size: 32.0,
                color: "#eee",
            },

            Row {
                gap: 15.0,

                Button {
                    text: "Action",
                    background: "#16213e",
                    color: "#fff",
                    padding: 15.0,
                },

                Button {
                    text: "Cancel",
                    background: "#533483",
                    color: "#fff",
                    padding: 15.0,
                },
            },
        }
    };

    // Write to file
    match doc.write_to_file("/tmp/rust_macro.kir") {
        Ok(_) => println!("✓ Generated /tmp/rust_macro.kir"),
        Err(e) => eprintln!("✗ Error: {}", e),
    }

    // Print JSON
    println!("\n{}", doc.to_json().unwrap());
}
