// Test to verify the HTML renderer fix for text inside containers
use kryon_html::{HtmlRenderer, HtmlConfig};
use kryon_render::RenderCommand;
use glam::{Vec2, Vec4};
use kryon_core::TextAlignment;
use kryon_render::LayoutStyle;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut renderer = HtmlRenderer::new(Vec2::new(800.0, 600.0));
    
    // Start frame
    renderer.begin_frame(Vec4::new(0.1, 0.1, 0.1, 1.0))?;
    
    // Create container with flex layout
    let layout_style = LayoutStyle {
        display: Some("flex".to_string()),
        flex_direction: Some("column".to_string()),
        align_items: Some("center".to_string()),
        justify_content: Some("center".to_string()),
        ..Default::default()
    };
    
    let commands = vec![
        RenderCommand::BeginContainer {
            element_id: "container".to_string(),
            position: Vec2::new(200.0, 100.0),
            size: Vec2::new(200.0, 100.0),
            color: Vec4::new(0.1, 0.1, 0.44, 1.0), // Blue container
            border_radius: 0.0,
            border_width: 1.0,
            border_color: Vec4::new(0.0, 0.6, 1.0, 1.0), // Light blue border
            layout_style: Some(layout_style),
            z_index: 0,
        },
        RenderCommand::DrawText {
            element_id: "text".to_string(),
            position: Vec2::new(0.0, 0.0), // Position within container
            text: "Hello World".to_string(),
            font_size: 16.0,
            color: Vec4::new(1.0, 1.0, 0.0, 1.0), // Yellow text
            alignment: TextAlignment::Center,
            max_width: None,
            max_height: None,
            font_family: None,
            z_index: 1,
        },
        RenderCommand::EndContainer,
    ];
    
    // Execute commands
    renderer.execute_commands(&mut (), &commands)?;
    
    // End frame
    renderer.end_frame(())?;
    
    // Generate HTML
    let config = HtmlConfig::default();
    let html = renderer.generate_html(&config);
    
    println!("Generated HTML with fix:");
    println!("{}", html);
    
    println!("\n=== Key points of the fix ===");
    println!("1. Text elements inside flex containers now flow naturally");
    println!("2. Absolute positioning removed for text elements");
    println!("3. Container has proper flex layout with center alignment");
    println!("4. Text should now appear properly inside the container");
    
    Ok(())
}