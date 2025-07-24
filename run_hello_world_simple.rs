// Simple Hello World with Raylib - NO KRB parsing
use raylib::prelude::*;

fn main() {
    println!("🚀 Starting Raylib Hello World Window");
    
    let (mut rl, thread) = raylib::init()
        .size(800, 600)
        .title("Hello World Example")
        .build();
    
    rl.set_target_fps(60);
    
    println!("✅ Raylib window initialized!");
    println!("🖼️ Window will show: Blue container with 'Hello World' text");
    
    while !rl.window_should_close() {
        let mut d = rl.begin_drawing(&thread);
        
        // Dark gray background (from appstyle)
        d.clear_background(Color::new(25, 25, 25, 255));
        
        // Blue container with border (from containerstyle)
        d.draw_rectangle_lines_ex(
            Rectangle::new(200.0, 100.0, 200.0, 100.0),
            1.0,
            Color::new(0, 153, 255, 255), // Light blue border
        );
        d.draw_rectangle(200, 100, 200, 100, Color::new(25, 25, 112, 255)); // Blue container
        
        // Yellow "Hello World" text (centered)
        let text = "Hello World";
        let font_size = 24;
        let text_width = measure_text(text, font_size);
        let text_x = 200 + (200 - text_width) / 2; // Center in container
        let text_y = 100 + (100 - font_size) / 2; // Center in container
        
        d.draw_text(text, text_x, text_y, font_size, Color::new(255, 255, 0, 255)); // Yellow
        
        // Instructions
        d.draw_text("Press ESC to close", 10, 10, 20, Color::WHITE);
    }
    
    println!("🎉 Window closed successfully!");
}