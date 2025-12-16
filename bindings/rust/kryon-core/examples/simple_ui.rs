use kryon_core::{ComponentType, IRComponent, IRDocument};

fn main() {
    // Create a simple UI matching the demo_app structure
    let mut column = IRComponent::new(ComponentType::Column, 2);
    column.width = Some("100%".to_string());
    column.height = Some("100%".to_string());
    column.background = Some("#1a1a2e".to_string());
    column.padding = Some(30.0);
    column.gap = Some(20.0);

    // Add text
    let mut text = IRComponent::new(ComponentType::Text, 3);
    text.content = Some("Kryon Rust Frontend".to_string());
    text.color = Some("#eee".to_string());

    column.add_child(text);

    // Add row with buttons
    let mut row = IRComponent::new(ComponentType::Row, 4);
    row.gap = Some(15.0);

    let mut button1 = IRComponent::new(ComponentType::Button, 5);
    button1.text = Some("Action".to_string());
    button1.background = Some("#16213e".to_string());
    button1.color = Some("#fff".to_string());
    button1.padding = Some(15.0);

    let mut button2 = IRComponent::new(ComponentType::Button, 6);
    button2.text = Some("Cancel".to_string());
    button2.background = Some("#533483".to_string());
    button2.color = Some("#fff".to_string());
    button2.padding = Some(15.0);

    row.add_child(button1);
    row.add_child(button2);

    column.add_child(row);

    // Create root container
    let mut root = IRComponent::new(ComponentType::Column, 1);
    root.width = Some("500px".to_string());
    root.height = Some("300px".to_string());
    root.add_child(column);

    // Create document
    let doc = IRDocument::new(root);

    // Serialize to JSON
    match doc.write_to_file("/tmp/rust_generated.kir") {
        Ok(_) => println!("✓ Generated /tmp/rust_generated.kir"),
        Err(e) => eprintln!("✗ Error: {}", e),
    }

    println!("\n{}", doc.to_json().unwrap());
}
