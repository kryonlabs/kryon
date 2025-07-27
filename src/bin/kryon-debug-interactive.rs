use anyhow::{Context, Result};
use clap::Parser;
use std::path::Path;
use kryon_runtime::KryonApp;
use kryon_render::{Renderer, CommandRenderer, RenderCommand, RenderResult};
use kryon_core::{InteractionState, ElementId, Element};
use kryon_layout::LayoutResult;
use glam::{Vec2, Vec4};
use std::collections::HashMap;

#[derive(Parser)]
#[command(name = "kryon-debug-interactive")]
#[command(about = "Interactive debug renderer that runs layout and shows element positions")]
struct Args {
    /// Path to the .krb file to debug
    krb_file: String,

    /// Show element interaction states
    #[arg(long)]
    show_interactions: bool,

    /// Simulate mouse hover at position x,y
    #[arg(long)]
    hover_at: Option<String>,

    /// Simulate mouse click at position x,y
    #[arg(long)]
    click_at: Option<String>,

    /// Show detailed layout computation
    #[arg(long)]
    show_layout_debug: bool,
}

// Debug renderer that logs all commands
#[derive(Debug)]
struct DebugRenderer {
    commands: Vec<RenderCommand>,
    viewport_size: Vec2,
}

impl DebugRenderer {
    fn new() -> Self {
        Self {
            commands: Vec::new(),
            viewport_size: Vec2::new(800.0, 400.0),
        }
    }
    
    fn get_commands(&self) -> &[RenderCommand] {
        &self.commands
    }
}

impl Renderer for DebugRenderer {
    type Surface = Vec2;
    type Context = ();
    
    fn initialize(_surface: Vec2) -> RenderResult<Self> {
        Ok(DebugRenderer::new())
    }
    
    fn begin_frame(&mut self, _clear_color: Vec4) -> RenderResult<()> {
        self.commands.clear();
        Ok(())
    }
    
    fn end_frame(&mut self, _context: ()) -> RenderResult<()> {
        Ok(())
    }
    
    fn render_element(&mut self, _context: &mut (), _element: &Element, _layout: &LayoutResult, _element_id: ElementId) -> RenderResult<()> {
        Ok(())
    }
    
    fn resize(&mut self, new_size: Vec2) -> RenderResult<()> {
        self.viewport_size = new_size;
        Ok(())
    }
    
    fn viewport_size(&self) -> Vec2 {
        self.viewport_size
    }
}

impl CommandRenderer for DebugRenderer {
    fn execute_commands(&mut self, _context: &mut (), commands: &[RenderCommand]) -> RenderResult<()> {
        self.commands.extend_from_slice(commands);
        println!("🔧 [DEBUG] Executed {} render commands", commands.len());
        for (i, command) in commands.iter().enumerate() {
            println!("  Command {}: {:?}", i, command);
        }
        Ok(())
    }
}

fn main() -> Result<()> {
    let args = Args::parse();

    // Validate file path
    if !Path::new(&args.krb_file).exists() {
        anyhow::bail!("KRB file not found: {}", args.krb_file);
    }

    println!("🔧 [DEBUG] Loading KRB file: {}", args.krb_file);
    
    // Create debug renderer
    let debug_renderer = DebugRenderer::new();
    
    // Create Kryon app with debug renderer
    let mut app = KryonApp::new(&args.krb_file, debug_renderer)
        .context("Failed to create Kryon app")?;

    println!("🔧 [DEBUG] App created successfully");

    // Simulate interactions if specified
    if let Some(ref hover_pos) = args.hover_at {
        let pos = parse_position(hover_pos)?;
        println!("🔧 [DEBUG] Simulating hover at ({:.1}, {:.1})", pos.x, pos.y);
        simulate_hover(&mut app, pos)?;
    }

    if let Some(ref click_pos) = args.click_at {
        let pos = parse_position(click_pos)?;
        println!("🔧 [DEBUG] Simulating click at ({:.1}, {:.1})", pos.x, pos.y);
        simulate_click(&mut app, pos)?;
    }

    // Force render to apply all layout computations
    println!("🔧 [DEBUG] Computing layout and rendering...");
    app.render()?;

    // Get the computed layout and element states
    print_debug_info(&app, &args)?;

    Ok(())
}

fn parse_position(pos_str: &str) -> Result<Vec2> {
    let parts: Vec<&str> = pos_str.split(',').collect();
    if parts.len() != 2 {
        anyhow::bail!("Position must be in format 'x,y', got: {}", pos_str);
    }
    
    let x: f32 = parts[0].parse().context("Invalid x coordinate")?;
    let y: f32 = parts[1].parse().context("Invalid y coordinate")?;
    
    Ok(Vec2::new(x, y))
}

fn simulate_hover(app: &mut KryonApp<DebugRenderer>, position: Vec2) -> Result<()> {
    use kryon_render::events::{InputEvent, MouseButton};
    
    // Simulate mouse move to trigger hover
    app.handle_input(InputEvent::MouseMove { position })?;
    Ok(())
}

fn simulate_click(app: &mut KryonApp<DebugRenderer>, position: Vec2) -> Result<()> {
    use kryon_render::events::{InputEvent, MouseButton};
    
    // Simulate full click sequence: press, then release
    app.handle_input(InputEvent::MousePress { position, button: MouseButton::Left })?;
    app.handle_input(InputEvent::MouseRelease { position, button: MouseButton::Left })?;
    Ok(())
}

fn print_debug_info(app: &KryonApp<DebugRenderer>, args: &Args) -> Result<()> {
    println!("\n=== COMPUTED LAYOUT & INTERACTION STATES ===");
    
    // Access KryonApp internals via trait implementations
    // We'll need to use workarounds since we can't access private fields
    
    println!("Viewport size: ({:.1}, {:.1})", app.viewport_size().x, app.viewport_size().y);
    
    // Get render commands from the debug renderer  
    let renderer = app.renderer();
    let commands = renderer.backend().get_commands();
    
    println!("\n=== RENDER COMMANDS ({}) ===", commands.len());
    for (i, command) in commands.iter().enumerate() {
        println!("{}: {:?}", i, command);
    }
    
    // For now we'll have to work with what we have from the rendered commands
    // to infer element positions and sizes
    
    Ok(())
}