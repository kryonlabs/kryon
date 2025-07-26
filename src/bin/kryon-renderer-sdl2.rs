use std::path::Path;
use std::time::{Duration, Instant};

use anyhow::{Context, Result};
use clap::Parser;
use tracing::{error, info};

use kryon_render::Renderer;
use kryon_runtime::KryonApp;
use kryon_sdl2::Sdl2Renderer;

#[derive(Parser)]
#[command(name = "kryon-renderer-sdl2")]
#[command(about = "SDL2-based renderer for Kryon .krb files")]
struct Args {
    /// Path to the .krb file to render
    krb_file: String,

    /// Window width. Overrides the value in the KRB file.
    #[arg(long)]
    width: Option<i32>,

    /// Window height. Overrides the value in the KRB file.
    #[arg(long)]
    height: Option<i32>,

    /// Window title. Overrides the value in the KRB file.
    #[arg(long)]
    title: Option<String>,

    /// Enable debug logging
    #[arg(short, long)]
    debug: bool,

    /// Enable standalone rendering mode (auto-wrap non-App elements)
    #[arg(long)]
    standalone: bool,
}

fn main() -> Result<()> {
    let args = Args::parse();

    // Initialize logging
    let subscriber = tracing_subscriber::fmt()
        .with_max_level(if args.debug {
            tracing::Level::DEBUG
        } else {
            tracing::Level::INFO
        })
        .with_target(false)
        .compact()
        .finish();
    
    tracing::subscriber::set_global_default(subscriber)
        .context("Failed to set tracing subscriber")?;

    // Validate file path
    if !Path::new(&args.krb_file).exists() {
        anyhow::bail!("KRB file not found: {}", args.krb_file);
    }

    info!("Loading KRB file: {}", args.krb_file);
    
    // Load the application definition first to get window properties
    let krb_file = kryon_core::load_krb_file(&args.krb_file)
        .context("Failed to load KRB file to read window properties")?;

    // Set default values
    let mut width = 800;
    let mut height = 600;
    let mut title = "Kryon SDL2 Renderer".to_string();

    // Read properties from the KRB file's root element
    if let Some(root_id) = krb_file.root_element_id {
        if let Some(root_element) = krb_file.elements.get(&root_id) {
            eprintln!("[WINDOW_INIT] Root element type: {:?}, ID: {}", root_element.element_type, root_element.id);
            
            // The KRB parser stores WindowWidth/WindowHeight in layout_size for App elements
            let size_x = root_element.layout_size.width.to_pixels(1.0);
            let size_y = root_element.layout_size.height.to_pixels(1.0);
            eprintln!("[WINDOW_INIT] Root element layout_size: {}x{}", size_x, size_y);
            
            if size_x > 0.0 {
                width = size_x as i32;
                eprintln!("[WINDOW_INIT] Found WindowWidth in layout_size: {}", width);
            }
            if size_y > 0.0 {
                height = size_y as i32;
                eprintln!("[WINDOW_INIT] Found WindowHeight in layout_size: {}", height);
            }
            
            // Window title is stored in the text field
            if !root_element.text.is_empty() {
                title = root_element.text.clone();
                eprintln!("[WINDOW_INIT] Found WindowTitle in text field: {}", title);
            }
        }
    }

    // Check if we're in standalone mode (auto-generated App)
    let is_standalone = if let Some(root_id) = krb_file.root_element_id {
        if let Some(root_element) = krb_file.elements.get(&root_id) {
            args.standalone || root_element.id == "auto_generated_app"
        } else {
            false
        }
    } else {
        false
    };
    
    // Allow CLI arguments to override KRB file properties
    // In standalone mode, prefer CLI arguments over KRB file properties
    let final_width = if is_standalone {
        args.width.unwrap_or(800) // Default for standalone
    } else {
        args.width.unwrap_or(width)
    };
    
    let final_height = if is_standalone {
        args.height.unwrap_or(600) // Default for standalone
    } else {
        args.height.unwrap_or(height)
    };
    
    let final_title = if is_standalone {
        args.title.clone().unwrap_or_else(|| "Kryon SDL2 Standalone Renderer".to_string())
    } else {
        args.title.clone().unwrap_or(title)
    };
    
    info!("Initializing SDL2 renderer with properties: {}x{} '{}'", final_width, final_height, &final_title);
    
    // Initialize renderer with the final, resolved properties
    let mut renderer = Sdl2Renderer::initialize((final_width, final_height, final_title))
        .context("Failed to initialize SDL2 renderer")?;

    let mut app = KryonApp::new(&args.krb_file, renderer)
        .context("Failed to create Kryon application")?;

    // Force initial render
    if let Err(e) = app.render() {
        error!("Failed to render initial frame: {}", e);
    }

    info!("Starting SDL2 render loop...");
    
    let mut last_frame_time = Instant::now();
    
    'main_loop: loop {
        // Check if window should close
        if app.renderer_mut().backend_mut().should_close() {
            info!("Window close requested");
            break;
        }
        
        let now = Instant::now();
        let delta_time = now.duration_since(last_frame_time);
        last_frame_time = now;
        
        // Poll and handle input events
        let input_events = app.renderer_mut().backend_mut().poll_events();
        for event in input_events {
            // Check for ESC key to quit application
            if let kryon_render::events::InputEvent::KeyPress { key, .. } = &event {
                if matches!(key, kryon_render::events::KeyCode::Escape) {
                    info!("ESC key pressed - quitting application");
                    break 'main_loop;
                }
            }
            
            if let Err(e) = app.handle_input(event) {
                error!("Failed to handle input event: {}", e);
            }
        }
        
        // Update application
        if let Err(e) = app.update(delta_time) {
            error!("Failed to update app: {}", e);
            error!("Continuing despite update error...");
        }
        
        // Update cursor based on current hover state
        let cursor_type = app.current_cursor();
        app.renderer_mut().backend_mut().set_cursor(cursor_type);
        
        // Render frame
        if let Err(e) = app.render() {
            error!("Failed to render frame: {}", e);
            error!("Continuing despite render error...");
        }
    }
    
    info!("SDL2 renderer shutdown complete");
    Ok(())
}