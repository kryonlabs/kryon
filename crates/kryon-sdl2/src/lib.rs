// crates/kryon-sdl2/src/lib.rs
use kryon_render::{
    Renderer, CommandRenderer, RenderCommand, RenderResult, KeyCode, TextManager,
    text_manager::RenderedText
};
#[cfg(feature = "svg")]
use kryon_render::SvgRenderer;
use kryon_render::events::{InputEvent, MouseButton, KeyModifiers};
use kryon_core::CursorType;
use kryon_layout::LayoutResult;
use glam::{Vec2, Vec4};
use sdl2::pixels::Color;
use sdl2::render::Canvas;
use sdl2::video::Window;
use sdl2::rect::Rect;
use kryon_render::RenderError;

pub struct Sdl2Renderer {
    canvas: Canvas<Window>,
    event_pump: sdl2::EventPump,
    size: Vec2,
    text_manager: TextManager,
    pending_commands: Vec<RenderCommand>,
    prev_mouse_pos: Vec2,
    current_cursor: CursorType,
    #[cfg(feature = "svg")]
    svg_renderer: SvgRenderer,
}

pub struct Sdl2RenderContext {
    // Empty context - commands are stored in renderer
}

impl Renderer for Sdl2Renderer {
    type Surface = (i32, i32, String); // (width, height, title)
    type Context = Sdl2RenderContext;
    
    fn initialize(surface: Self::Surface) -> RenderResult<Self> where Self: Sized {
        let (width, height, title) = surface;
        
        let sdl_context = sdl2::init()
            .map_err(|e| RenderError::InitializationFailed(format!("SDL2 init failed: {}", e)))?;
        
        let video_subsystem = sdl_context.video()
            .map_err(|e| RenderError::InitializationFailed(format!("SDL2 video init failed: {}", e)))?;
        
        let window = video_subsystem
            .window(&title, width as u32, height as u32)
            .position_centered()
            .resizable()
            .build()
            .map_err(|e| RenderError::InitializationFailed(format!("SDL2 window creation failed: {}", e)))?;
        
        let canvas = window
            .into_canvas()
            .accelerated()
            .present_vsync()
            .build()
            .map_err(|e| RenderError::InitializationFailed(format!("SDL2 canvas creation failed: {}", e)))?;
        
        let _texture_creator = canvas.texture_creator();
        
        let event_pump = sdl_context.event_pump()
            .map_err(|e| RenderError::InitializationFailed(format!("SDL2 event pump creation failed: {}", e)))?;
        
        eprintln!("[SDL2_INIT] Window initialized: {}x{}", width, height);
        
        Ok(Self {
            canvas,
            event_pump,
            size: Vec2::new(width as f32, height as f32),
            text_manager: TextManager::new(),
            pending_commands: Vec::new(),
            prev_mouse_pos: Vec2::new(-1.0, -1.0),
            current_cursor: CursorType::Default,
            #[cfg(feature = "svg")]
            svg_renderer: SvgRenderer::new(),
        })
    }
    
    fn begin_frame(&mut self, clear_color: Vec4) -> RenderResult<Self::Context> {
        self.pending_commands.clear();
        
        // Set clear color
        let color = vec4_to_sdl_color(clear_color);
        self.canvas.set_draw_color(color);
        self.canvas.clear();
        
        Ok(Sdl2RenderContext {})
    }
    
    fn end_frame(&mut self, _context: Self::Context) -> RenderResult<()> {
        // Execute all pending commands
        let commands = std::mem::take(&mut self.pending_commands);
        
        for command in &commands {
            self.execute_single_command(command)?;
        }
        
        self.canvas.present();
        Ok(())
    }
    
    fn render_element(
        &mut self,
        _context: &mut Self::Context,
        _element: &kryon_core::Element,
        _layout: &LayoutResult,
        _element_id: kryon_core::ElementId,
    ) -> RenderResult<()> {
        // This method is not used in command-based rendering
        Ok(())
    }
    
    fn resize(&mut self, new_size: Vec2) -> RenderResult<()> {
        self.size = new_size;
        Ok(())
    }
    
    fn viewport_size(&self) -> Vec2 {
        let size = self.canvas.output_size()
            .map_err(|e| RenderError::RenderFailed(format!("Failed to get canvas size: {}", e)))
            .unwrap_or((800, 600));
        Vec2::new(size.0 as f32, size.1 as f32)
    }
}

impl CommandRenderer for Sdl2Renderer {
    fn execute_commands(
        &mut self,
        _context: &mut Self::Context,
        commands: &[RenderCommand],
    ) -> RenderResult<()> {
        // Store commands to be executed in end_frame
        self.pending_commands.extend_from_slice(commands);
        Ok(())
    }
}

impl Sdl2Renderer {
    pub fn should_close(&mut self) -> bool {
        for event in self.event_pump.poll_iter() {
            match event {
                sdl2::event::Event::Quit { .. } => return true,
                _ => {}
            }
        }
        false
    }
    
    pub fn poll_events(&mut self) -> Vec<InputEvent> {
        let mut events = Vec::new();
        
        for event in self.event_pump.poll_iter() {
            match event {
                sdl2::event::Event::Quit { .. } => {
                    // Handle quit in should_close()
                }
                sdl2::event::Event::MouseButtonDown { mouse_btn, x, y, .. } => {
                    let button = match mouse_btn {
                        sdl2::mouse::MouseButton::Left => MouseButton::Left,
                        sdl2::mouse::MouseButton::Right => MouseButton::Right,
                        sdl2::mouse::MouseButton::Middle => MouseButton::Middle,
                        _ => continue,
                    };
                    events.push(InputEvent::MousePress {
                        button,
                        position: Vec2::new(x as f32, y as f32),
                    });
                }
                sdl2::event::Event::MouseButtonUp { mouse_btn, x, y, .. } => {
                    let button = match mouse_btn {
                        sdl2::mouse::MouseButton::Left => MouseButton::Left,
                        sdl2::mouse::MouseButton::Right => MouseButton::Right,
                        sdl2::mouse::MouseButton::Middle => MouseButton::Middle,
                        _ => continue,
                    };
                    events.push(InputEvent::MouseRelease {
                        button,
                        position: Vec2::new(x as f32, y as f32),
                    });
                }
                sdl2::event::Event::MouseMotion { x, y, .. } => {
                    events.push(InputEvent::MouseMove {
                        position: Vec2::new(x as f32, y as f32),
                    });
                }
                sdl2::event::Event::KeyDown { keycode: Some(keycode), .. } => {
                    if let Some(key) = sdl_keycode_to_kryon(keycode) {
                        events.push(InputEvent::KeyPress {
                            key,
                            modifiers: KeyModifiers::none(), // TODO: Get actual modifiers
                        });
                    }
                }
                sdl2::event::Event::KeyUp { keycode: Some(keycode), .. } => {
                    if let Some(key) = sdl_keycode_to_kryon(keycode) {
                        events.push(InputEvent::KeyRelease {
                            key,
                            modifiers: KeyModifiers::none(),
                        });
                    }
                }
                _ => {}
            }
        }
        
        events
    }
    
    fn execute_single_command(&mut self, command: &RenderCommand) -> RenderResult<()> {
        match command {
            RenderCommand::DrawRect { 
                position, 
                size, 
                color, 
                border_width,
                border_color,
                border_radius: _,  // TODO: Implement rounded rectangles
                .. 
            } => {
                // Draw main rectangle
                let rect = Rect::new(
                    position.x as i32,
                    position.y as i32,
                    size.x as u32,
                    size.y as u32,
                );
                let sdl_color = vec4_to_sdl_color(*color);
                self.canvas.set_draw_color(sdl_color);
                self.canvas.fill_rect(rect)
                    .map_err(|e| RenderError::RenderFailed(format!("Failed to draw rect: {}", e)))?;
                
                // Draw border if specified
                if *border_width > 0.0 {
                    self.draw_border(rect, *border_width, *border_color)?;
                }
            }
            RenderCommand::DrawText { 
                text, 
                position, 
                color, 
                font_size, 
                font_family,
                .. 
            } => {
                // Render text using TextManager and cosmic-text
                let rendered_text = self.text_manager.render_simple_text(
                    text,
                    *font_size,
                    *color,
                    None,
                );
                
                // For SDL2, we need to convert glyphs to pixels and draw them
                // This is a simplified implementation - in a real renderer you'd 
                // rasterize glyphs to textures and draw them
                self.draw_text_glyphs(&rendered_text, *position)?;
            }
            RenderCommand::DrawRichText {
                rich_text,
                position,
                default_color,
                max_width,
                max_height: _,
                ..
            } => {
                let rendered_text = self.text_manager.render_rich_text(
                    rich_text,
                    *max_width,
                    *default_color,
                );
                
                self.draw_text_glyphs(&rendered_text, *position)?;
            }
            RenderCommand::DrawImage { 
                source, 
                position, 
                size, 
                .. 
            } => {
                // TODO: Load and render image
                eprintln!("[SDL2] Image rendering not yet implemented: '{}' at {:?}", source, position);
            }
            #[cfg(feature = "svg")]
            RenderCommand::DrawSvg { .. } => {
                // TODO: Implement SVG rendering
                eprintln!("[SDL2] SVG rendering not yet implemented");
            }
            // Transform commands are not separate - they're part of individual commands
            _ => {
                // TODO: Implement remaining commands
                eprintln!("[SDL2] Command not yet implemented: {:?}", command);
            }
        }
        Ok(())
    }
    
    pub fn set_cursor(&mut self, cursor_type: CursorType) {
        self.current_cursor = cursor_type;
        // TODO: Actually set SDL2 cursor
    }
    
    /// Draw borders around a rectangle
    fn draw_border(&mut self, rect: Rect, border_width: f32, border_color: Vec4) -> RenderResult<()> {
        let border_sdl_color = vec4_to_sdl_color(border_color);
        self.canvas.set_draw_color(border_sdl_color);
        
        let border_w = border_width as i32;
        
        // Top border
        let top_rect = Rect::new(rect.x, rect.y, rect.width(), border_w as u32);
        self.canvas.fill_rect(top_rect)
            .map_err(|e| RenderError::RenderFailed(format!("Failed to draw top border: {}", e)))?;
        
        // Bottom border
        let bottom_rect = Rect::new(
            rect.x, 
            rect.y + rect.height() as i32 - border_w, 
            rect.width(), 
            border_w as u32
        );
        self.canvas.fill_rect(bottom_rect)
            .map_err(|e| RenderError::RenderFailed(format!("Failed to draw bottom border: {}", e)))?;
        
        // Left border
        let left_rect = Rect::new(rect.x, rect.y, border_w as u32, rect.height());
        self.canvas.fill_rect(left_rect)
            .map_err(|e| RenderError::RenderFailed(format!("Failed to draw left border: {}", e)))?;
        
        // Right border
        let right_rect = Rect::new(
            rect.x + rect.width() as i32 - border_w, 
            rect.y, 
            border_w as u32, 
            rect.height()
        );
        self.canvas.fill_rect(right_rect)
            .map_err(|e| RenderError::RenderFailed(format!("Failed to draw right border: {}", e)))?;
        
        Ok(())
    }
    
    /// Draw text glyphs using simple pixel rendering
    /// This is a simplified implementation - in production you'd rasterize glyphs to textures
    fn draw_text_glyphs(&mut self, rendered_text: &RenderedText, base_position: Vec2) -> RenderResult<()> {
        for glyph in &rendered_text.glyphs {
            // For this simplified implementation, we'll draw a small rectangle for each glyph
            // In a real implementation, you'd rasterize the actual glyph using SwashCache
            let glyph_rect = Rect::new(
                (base_position.x + glyph.position.x) as i32,
                (base_position.y + glyph.position.y) as i32,
                glyph.size.x.max(1.0) as u32,
                glyph.size.y.max(1.0) as u32,
            );
            
            let glyph_color = vec4_to_sdl_color(glyph.color);
            self.canvas.set_draw_color(glyph_color);
            
            // Draw a simple rectangle representing the glyph
            // This is very basic - real text rendering would rasterize actual glyphs
            self.canvas.fill_rect(glyph_rect)
                .map_err(|e| RenderError::RenderFailed(format!("Failed to draw glyph: {}", e)))?;
        }
        
        Ok(())
    }
}

// Helper functions
fn vec4_to_sdl_color(color: Vec4) -> Color {
    Color::RGBA(
        (color.x * 255.0) as u8,
        (color.y * 255.0) as u8,
        (color.z * 255.0) as u8,
        (color.w * 255.0) as u8,
    )
}

fn sdl_keycode_to_kryon(keycode: sdl2::keyboard::Keycode) -> Option<KeyCode> {
    use sdl2::keyboard::Keycode as SdlKey;
    
    Some(match keycode {
        SdlKey::A => KeyCode::Character('a'),
        SdlKey::B => KeyCode::Character('b'),
        SdlKey::C => KeyCode::Character('c'),
        SdlKey::D => KeyCode::Character('d'),
        SdlKey::E => KeyCode::Character('e'),
        SdlKey::F => KeyCode::Character('f'),
        SdlKey::G => KeyCode::Character('g'),
        SdlKey::H => KeyCode::Character('h'),
        SdlKey::I => KeyCode::Character('i'),
        SdlKey::J => KeyCode::Character('j'),
        SdlKey::K => KeyCode::Character('k'),
        SdlKey::L => KeyCode::Character('l'),
        SdlKey::M => KeyCode::Character('m'),
        SdlKey::N => KeyCode::Character('n'),
        SdlKey::O => KeyCode::Character('o'),
        SdlKey::P => KeyCode::Character('p'),
        SdlKey::Q => KeyCode::Character('q'),
        SdlKey::R => KeyCode::Character('r'),
        SdlKey::S => KeyCode::Character('s'),
        SdlKey::T => KeyCode::Character('t'),
        SdlKey::U => KeyCode::Character('u'),
        SdlKey::V => KeyCode::Character('v'),
        SdlKey::W => KeyCode::Character('w'),
        SdlKey::X => KeyCode::Character('x'),
        SdlKey::Y => KeyCode::Character('y'),
        SdlKey::Z => KeyCode::Character('z'),
        SdlKey::Num0 => KeyCode::Character('0'),
        SdlKey::Num1 => KeyCode::Character('1'),
        SdlKey::Num2 => KeyCode::Character('2'),
        SdlKey::Num3 => KeyCode::Character('3'),
        SdlKey::Num4 => KeyCode::Character('4'),
        SdlKey::Num5 => KeyCode::Character('5'),
        SdlKey::Num6 => KeyCode::Character('6'),
        SdlKey::Num7 => KeyCode::Character('7'),
        SdlKey::Num8 => KeyCode::Character('8'),
        SdlKey::Num9 => KeyCode::Character('9'),
        SdlKey::Space => KeyCode::Space,
        SdlKey::Return => KeyCode::Enter,
        SdlKey::Escape => KeyCode::Escape,
        SdlKey::Backspace => KeyCode::Backspace,
        SdlKey::Tab => KeyCode::Tab,
        SdlKey::Delete => KeyCode::Delete,
        _ => return None,
    })
}

// Re-export commonly used types
pub use sdl2;