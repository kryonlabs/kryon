// crates/kryon-sdl2/src/lib.rs
use kryon_render::{
    Renderer, CommandRenderer, RenderCommand, RenderResult, KeyCode, TextManager
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
use sdl2::ttf::Sdl2TtfContext;
use sdl2::mouse::{SystemCursor, Cursor};
use kryon_render::RenderError;
use kryon_core::TextAlignment;
use std::path::Path;

// Helper function to check if a point is inside a triangle
fn point_in_triangle(px: f32, py: f32, x1: f32, y1: f32, x2: f32, y2: f32, x3: f32, y3: f32) -> bool {
    // Calculate barycentric coordinates
    let v0x = x3 - x1;
    let v0y = y3 - y1;
    let v1x = x2 - x1;
    let v1y = y2 - y1;
    let v2x = px - x1;
    let v2y = py - y1;
    
    let dot00 = v0x * v0x + v0y * v0y;
    let dot01 = v0x * v1x + v0y * v1y;
    let dot02 = v0x * v2x + v0y * v2y;
    let dot11 = v1x * v1x + v1y * v1y;
    let dot12 = v1x * v2x + v1y * v2y;
    
    let inv_denom = 1.0 / (dot00 * dot11 - dot01 * dot01);
    let u = (dot11 * dot02 - dot01 * dot12) * inv_denom;
    let v = (dot00 * dot12 - dot01 * dot02) * inv_denom;
    
    (u >= 0.0) && (v >= 0.0) && (u + v <= 1.0)
}

pub struct Sdl2Renderer {
    canvas: Canvas<Window>,
    event_pump: sdl2::EventPump,
    size: Vec2,
    #[allow(dead_code)]
    text_manager: TextManager,
    pending_commands: Vec<RenderCommand>,
    #[allow(dead_code)]
    prev_mouse_pos: Vec2,
    current_cursor: CursorType,
    #[cfg(feature = "svg")]
    #[allow(dead_code)]
    svg_renderer: SvgRenderer,
    // SDL2 TTF support
    ttf_context: Sdl2TtfContext,
    default_font_path: Option<String>,
    // Hover tracking
    current_mouse_pos: Vec2,
    #[allow(dead_code)]
    hovered_elements: Vec<(Vec2, Vec2)>, // (position, size) of hoverable elements
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
        
        // Initialize TTF subsystem
        let ttf_context = sdl2::ttf::init()
            .map_err(|e| RenderError::InitializationFailed(format!("SDL2 TTF init failed: {}", e)))?;
        
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
            ttf_context,
            default_font_path: None,
            current_mouse_pos: Vec2::new(-1.0, -1.0),
            hovered_elements: Vec::new(),
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
                    self.current_mouse_pos = Vec2::new(x as f32, y as f32);
                    events.push(InputEvent::MouseMove {
                        position: Vec2::new(x as f32, y as f32),
                    });
                }
                sdl2::event::Event::KeyDown { keycode: Some(keycode), keymod, .. } => {
                    if let Some(key) = sdl_keycode_to_kryon(keycode) {
                        let modifiers = convert_sdl_modifiers(keymod);
                        events.push(InputEvent::KeyPress {
                            key,
                            modifiers,
                        });
                    }
                }
                sdl2::event::Event::KeyUp { keycode: Some(keycode), keymod, .. } => {
                    if let Some(key) = sdl_keycode_to_kryon(keycode) {
                        let modifiers = convert_sdl_modifiers(keymod);
                        events.push(InputEvent::KeyRelease {
                            key,
                            modifiers,
                        });
                    }
                }
                sdl2::event::Event::TextInput { text, .. } => {
                    events.push(InputEvent::TextInput { text });
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
                alignment,
                max_width,
                max_height,
                .. 
            } => {
                // Calculate proper text position based on alignment
                let text_pos = self.calculate_text_position(
                    text, 
                    *position, 
                    Vec2::new(max_width.unwrap_or(1000.0), max_height.unwrap_or(100.0)),
                    *font_size,
                    *alignment
                )?;
                
                // Use SDL2 TTF for text rendering
                self.draw_text_ttf(text, text_pos, *color, *font_size, font_family.as_deref())?;
            }
            RenderCommand::DrawRichText {
                rich_text,
                position,
                default_color,
                max_width: _,
                max_height: _,
                ..
            } => {
                // For rich text, render as plain text for now
                let plain_text = rich_text.to_plain_text();
                let font_size = rich_text.spans.first()
                    .and_then(|span| span.font_size)
                    .unwrap_or(16.0);
                self.draw_text_ttf(&plain_text, *position, *default_color, font_size, None)?;
            }
            RenderCommand::DrawImage { 
                source, 
                position, 
                size: _,
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
            RenderCommand::SetCanvasSize(size) => {
                // This is informational - SDL2 manages its own window size
                // We could potentially use this to set viewport or scaling
                self.size = *size;
            }
            RenderCommand::DrawSelectButton {
                position,
                size,
                text,
                is_open,
                font_size,
                text_color,
                background_color,
                border_color,
                border_width,
                ..
            } => {
                // Draw select button background
                let button_rect = Rect::new(
                    position.x as i32,
                    position.y as i32,
                    size.x as u32,
                    size.y as u32,
                );
                
                // Draw background
                let bg_color = vec4_to_sdl_color(*background_color);
                self.canvas.set_draw_color(bg_color);
                self.canvas.fill_rect(button_rect)
                    .map_err(|e| RenderError::RenderFailed(format!("Failed to draw select button: {}", e)))?;
                
                // Draw border
                if *border_width > 0.0 {
                    self.draw_border(button_rect, *border_width, *border_color)?;
                }
                
                // Draw text
                let text_padding = 10.0;
                let text_pos = Vec2::new(position.x + text_padding, position.y + (size.y - font_size) / 2.0);
                self.draw_text_ttf(text, text_pos, *text_color, *font_size, None)?;
                
                // Draw dropdown arrow
                let arrow_size = 8.0;
                let arrow_x = position.x + size.x - arrow_size * 2.0;
                let arrow_y = position.y + (size.y - arrow_size) / 2.0;
                self.draw_dropdown_arrow(Vec2::new(arrow_x, arrow_y), arrow_size, *text_color, *is_open)?;
            }
            RenderCommand::DrawSelectMenu {
                position,
                size,
                options,
                highlighted_index,
                font_size,
                text_color,
                background_color,
                border_color,
                border_width,
                highlight_color,
                selected_color,
                ..
            } => {
                // Draw menu background
                let menu_rect = Rect::new(
                    position.x as i32,
                    position.y as i32,
                    size.x as u32,
                    size.y as u32,
                );
                
                // Draw background
                let bg_color = vec4_to_sdl_color(*background_color);
                self.canvas.set_draw_color(bg_color);
                self.canvas.fill_rect(menu_rect)
                    .map_err(|e| RenderError::RenderFailed(format!("Failed to draw select menu: {}", e)))?;
                
                // Draw border
                if *border_width > 0.0 {
                    self.draw_border(menu_rect, *border_width, *border_color)?;
                }
                
                // Draw options
                let option_height = 32.0;
                for (index, option) in options.iter().enumerate() {
                    let option_y = position.y + (index as f32 * option_height);
                    let option_rect = Rect::new(
                        position.x as i32,
                        option_y as i32,
                        size.x as u32,
                        option_height as u32,
                    );
                    
                    // Draw highlight or selection background
                    if Some(index) == *highlighted_index {
                        let highlight_bg = vec4_to_sdl_color(*highlight_color);
                        self.canvas.set_draw_color(highlight_bg);
                        self.canvas.fill_rect(option_rect)
                            .map_err(|e| RenderError::RenderFailed(format!("Failed to draw highlight: {}", e)))?;
                    } else if option.selected {
                        let selected_bg = vec4_to_sdl_color(*selected_color);
                        self.canvas.set_draw_color(selected_bg);
                        self.canvas.fill_rect(option_rect)
                            .map_err(|e| RenderError::RenderFailed(format!("Failed to draw selection: {}", e)))?;
                    }
                    
                    // Draw option text
                    let text_padding = 10.0;
                    let text_pos = Vec2::new(
                        position.x + text_padding,
                        option_y + (option_height - font_size) / 2.0
                    );
                    self.draw_text_ttf(&option.text, text_pos, *text_color, *font_size, None)?;
                }
            }
            RenderCommand::DrawTextInput {
                position,
                size,
                text,
                placeholder,
                font_size,
                text_color,
                placeholder_color,
                background_color,
                border_color,
                focus_border_color,
                border_width,
                border_radius: _,
                is_focused,
                is_editing,
                is_readonly: _,
                cursor_position,
                selection_start,
                selection_end,
                text_scroll_offset,
                input_type: _,
                ..
            } => {
                self.draw_text_input(
                    *position, *size, text, placeholder, *font_size,
                    *text_color, *placeholder_color, *background_color,
                    *border_color, *focus_border_color, *border_width,
                    *is_focused, *is_editing, *cursor_position,
                    *selection_start, *selection_end, *text_scroll_offset
                )?;
            }
            RenderCommand::DrawCheckbox {
                position,
                size,
                is_checked,
                text,
                font_size,
                text_color,
                background_color,
                border_color,
                border_width,
                check_color,
                ..
            } => {
                self.draw_checkbox(
                    *position, *size, *is_checked, text, *font_size,
                    *text_color, *background_color, *border_color,
                    *border_width, *check_color
                )?;
            }
            RenderCommand::DrawSlider {
                position,
                size,
                value,
                min_value,
                max_value,
                track_color,
                thumb_color,
                border_color,
                border_width,
                ..
            } => {
                self.draw_slider(
                    *position, *size, *value, *min_value, *max_value,
                    *track_color, *thumb_color, *border_color, *border_width
                )?;
            }
            RenderCommand::DrawTriangle {
                points,
                color,
            } => {
                if points.len() >= 3 {
                    // SDL2 doesn't have a direct triangle drawing function, so we use filled polygon
                    let sdl_color = vec4_to_sdl_color(*color);
                    
                    // Convert Vec2 points to SDL2 points
                    let x_coords: Vec<i16> = points.iter().map(|p| p.x as i16).collect();
                    let y_coords: Vec<i16> = points.iter().map(|p| p.y as i16).collect();
                    
                    // Draw filled triangle using gfx primitives
                    // Note: SDL2 doesn't have built-in filled polygon, so we draw it as lines
                    self.canvas.set_draw_color(sdl_color);
                    
                    // Draw the three lines of the triangle
                    for i in 0..3 {
                        let start = i;
                        let end = (i + 1) % 3;
                        self.canvas.draw_line(
                            (x_coords[start] as i32, y_coords[start] as i32),
                            (x_coords[end] as i32, y_coords[end] as i32)
                        ).map_err(|e| RenderError::RenderFailed(format!("Failed to draw triangle line: {}", e)))?;
                    }
                    
                    // Fill the triangle using a simple scanline approach
                    // Find bounding box
                    let min_x = x_coords.iter().min().copied().unwrap_or(0) as i32;
                    let max_x = x_coords.iter().max().copied().unwrap_or(0) as i32;
                    let min_y = y_coords.iter().min().copied().unwrap_or(0) as i32;
                    let max_y = y_coords.iter().max().copied().unwrap_or(0) as i32;
                    
                    // Simple triangle fill - for small triangles like dropdown arrows
                    for y in min_y..=max_y {
                        for x in min_x..=max_x {
                            if point_in_triangle(
                                x as f32, y as f32,
                                points[0].x, points[0].y,
                                points[1].x, points[1].y,
                                points[2].x, points[2].y
                            ) {
                                self.canvas.draw_point((x, y))
                                    .map_err(|e| RenderError::RenderFailed(format!("Failed to fill triangle: {}", e)))?;
                            }
                        }
                    }
                }
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
        
        // Convert Kryon cursor type to SDL2 system cursor
        let sdl_cursor = match cursor_type {
            CursorType::Default => SystemCursor::Arrow,
            CursorType::Pointer => SystemCursor::Hand,
            CursorType::Text => SystemCursor::IBeam,
            CursorType::Move => SystemCursor::SizeAll,
            CursorType::NotAllowed => SystemCursor::No,
        };
        
        // Create and set the cursor
        if let Ok(cursor) = Cursor::from_system(sdl_cursor) {
            cursor.set();
        }
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
    
    /// Draw text using SDL2 TTF
    fn draw_text_ttf(&mut self, text: &str, position: Vec2, color: Vec4, font_size: f32, _font_family: Option<&str>) -> RenderResult<()> {
        // Skip empty text
        if text.is_empty() {
            return Ok(());
        }
        
        // Find a system font - try common locations including nix store
        let mut font_paths: Vec<String> = vec![
            // Add the known nix store path first
            "/nix/store/74c6rbbw5w7i5h2w18dbfdiixzvbxfdq-dejavu-fonts-minimal-2.37/share/fonts/truetype/DejaVuSans.ttf".to_string(),
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf".to_string(),
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf".to_string(),
            "/System/Library/Fonts/Helvetica.ttc".to_string(),
            "C:\\Windows\\Fonts\\arial.ttf".to_string(),
            "/usr/share/fonts/TTF/DejaVuSans.ttf".to_string(),
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf".to_string(),
        ];
        
        // Try to find fonts in nix store dynamically (in case the path changes)
        if let Ok(entries) = std::fs::read_dir("/nix/store") {
            for entry in entries.flatten().take(1000) {
                let path = entry.path();
                if path.is_dir() {
                    let font_path = path.join("share/fonts/truetype/DejaVuSans.ttf");
                    if font_path.exists() {
                        font_paths.insert(0, font_path.to_string_lossy().to_string());
                        break;
                    }
                }
            }
        }
        
        // Also check if there's a cached default font path
        if let Some(ref default_path) = self.default_font_path.clone() {
            font_paths.insert(0, default_path.clone());
        }
        
        // Find first existing font
        let font_path = font_paths.iter()
            .find(|path| Path::new(path).exists())
            .ok_or_else(|| RenderError::RenderFailed("No suitable font found".to_string()))?;
        
        // Cache the successful font path
        if self.default_font_path.is_none() {
            self.default_font_path = Some(font_path.clone());
        }
        
        // Load font with the requested size
        let font = self.ttf_context.load_font(font_path, font_size as u16)
            .map_err(|e| RenderError::RenderFailed(format!("Failed to load font: {}", e)))?;
        
        // Render text to surface
        let sdl_color = vec4_to_sdl_color(color);
        let surface = font.render(text)
            .blended(sdl_color)
            .map_err(|e| RenderError::RenderFailed(format!("Failed to render text: {}", e)))?;
        
        // Create texture from surface
        let texture_creator = self.canvas.texture_creator();
        let texture = texture_creator.create_texture_from_surface(&surface)
            .map_err(|e| RenderError::RenderFailed(format!("Failed to create text texture: {}", e)))?;
        
        // Get text dimensions
        let query = texture.query();
        let dest_rect = Rect::new(
            position.x as i32,
            position.y as i32,
            query.width,
            query.height,
        );
        
        // Draw the text texture
        self.canvas.copy(&texture, None, Some(dest_rect))
            .map_err(|e| RenderError::RenderFailed(format!("Failed to draw text: {}", e)))?;
        
        Ok(())
    }
    
    /// Check if mouse is over a rectangular area
    #[allow(dead_code)]
    fn is_mouse_over(&self, position: Vec2, size: Vec2) -> bool {
        self.current_mouse_pos.x >= position.x
            && self.current_mouse_pos.x <= position.x + size.x
            && self.current_mouse_pos.y >= position.y
            && self.current_mouse_pos.y <= position.y + size.y
    }
    
    /// Calculate text position based on alignment within a container
    fn calculate_text_position(
        &mut self, 
        text: &str, 
        container_pos: Vec2, 
        container_size: Vec2, 
        font_size: f32,
        alignment: TextAlignment
    ) -> RenderResult<Vec2> {
        // Get approximate text dimensions (simplified calculation)
        let text_width = text.len() as f32 * font_size * 0.6; // Rough approximation
        let text_height = font_size;
        
        let pos = match alignment {
            TextAlignment::Start => {
                // Left aligned - small padding from left
                Vec2::new(container_pos.x + 8.0, container_pos.y + (container_size.y - text_height) / 2.0)
            },
            TextAlignment::Center => {
                // Center aligned
                Vec2::new(
                    container_pos.x + (container_size.x - text_width) / 2.0,
                    container_pos.y + (container_size.y - text_height) / 2.0
                )
            },
            TextAlignment::End => {
                // Right aligned - small padding from right
                Vec2::new(
                    container_pos.x + container_size.x - text_width - 8.0,
                    container_pos.y + (container_size.y - text_height) / 2.0
                )
            },
            TextAlignment::Justify => {
                // Default to left for justify (complex to implement properly)
                Vec2::new(container_pos.x + 8.0, container_pos.y + (container_size.y - text_height) / 2.0)
            }
        };
        
        Ok(pos)
    }
    
    /// Draw a dropdown arrow (triangle)
    fn draw_dropdown_arrow(&mut self, position: Vec2, size: f32, color: Vec4, is_open: bool) -> RenderResult<()> {
        let sdl_color = vec4_to_sdl_color(color);
        self.canvas.set_draw_color(sdl_color);
        
        // Create triangle points
        let _points = if is_open {
            // Upward pointing triangle when open
            vec![
                Vec2::new(position.x + size / 2.0, position.y),
                Vec2::new(position.x, position.y + size),
                Vec2::new(position.x + size, position.y + size),
            ]
        } else {
            // Downward pointing triangle when closed
            vec![
                Vec2::new(position.x, position.y),
                Vec2::new(position.x + size, position.y),
                Vec2::new(position.x + size / 2.0, position.y + size),
            ]
        };
        
        // Draw filled triangle using lines (SDL2 doesn't have native filled triangle)
        for y in 0..=(size as i32) {
            let t = y as f32 / size;
            let y_pos = position.y + (y as f32);
            
            // Calculate x bounds for this scanline
            let (x_start, x_end) = if is_open {
                let width = size * t;
                (
                    position.x + size / 2.0 - width / 2.0,
                    position.x + size / 2.0 + width / 2.0
                )
            } else {
                let width = size * (1.0 - t);
                (
                    position.x + size / 2.0 - width / 2.0,
                    position.x + size / 2.0 + width / 2.0
                )
            };
            
            // Draw horizontal line
            self.canvas.draw_line(
                (x_start as i32, y_pos as i32),
                (x_end as i32, y_pos as i32)
            ).map_err(|e| RenderError::RenderFailed(format!("Failed to draw arrow: {}", e)))?;
        }
        
        Ok(())
    }
    
    /// Draw a text input field with cursor and selection support
    fn draw_text_input(
        &mut self,
        position: Vec2,
        size: Vec2,
        text: &str,
        placeholder: &str,
        font_size: f32,
        text_color: Vec4,
        placeholder_color: Vec4,
        background_color: Vec4,
        border_color: Vec4,
        focus_border_color: Vec4,
        border_width: f32,
        is_focused: bool,
        is_editing: bool,
        cursor_position: usize,
        selection_start: Option<usize>,
        selection_end: Option<usize>,
        text_scroll_offset: f32,
    ) -> RenderResult<()> {
        // Draw input background
        let input_rect = Rect::new(
            position.x as i32,
            position.y as i32,
            size.x as u32,
            size.y as u32,
        );
        
        let bg_color = vec4_to_sdl_color(background_color);
        self.canvas.set_draw_color(bg_color);
        self.canvas.fill_rect(input_rect)
            .map_err(|e| RenderError::RenderFailed(format!("Failed to draw input background: {}", e)))?;
        
        // Draw border (focused or normal)
        let border_color_final = if is_focused { focus_border_color } else { border_color };
        if border_width > 0.0 {
            self.draw_border(input_rect, border_width, border_color_final)?;
        }
        
        // Determine what text to display
        let display_text = if text.is_empty() { placeholder } else { text };
        let text_color_final = if text.is_empty() { placeholder_color } else { text_color };
        
        if !display_text.is_empty() {
            // Calculate text position with padding
            let text_padding = 8.0;
            let text_y = position.y + (size.y - font_size) / 2.0;
            let text_x = position.x + text_padding - text_scroll_offset;
            
            // Draw selection background if any
            if let (Some(start), Some(end)) = (selection_start, selection_end) {
                self.draw_text_selection(
                    Vec2::new(text_x, text_y),
                    display_text,
                    font_size,
                    start,
                    end,
                    Vec4::new(0.3, 0.5, 1.0, 0.3) // Light blue selection
                )?;
            }
            
            // Draw the text
            self.draw_text_ttf(display_text, Vec2::new(text_x, text_y), text_color_final, font_size, None)?;
            
            // Draw cursor if editing
            if is_editing && is_focused {
                self.draw_text_cursor(
                    Vec2::new(text_x, text_y),
                    display_text,
                    cursor_position,
                    font_size,
                    text_color,
                    size.y
                )?;
            }
        }
        
        Ok(())
    }
    
    /// Draw a checkbox with optional label
    fn draw_checkbox(
        &mut self,
        position: Vec2,
        size: Vec2,
        is_checked: bool,
        label: &str,
        font_size: f32,
        text_color: Vec4,
        background_color: Vec4,
        border_color: Vec4,
        border_width: f32,
        check_color: Vec4,
    ) -> RenderResult<()> {
        // Calculate checkbox size (square)
        let checkbox_size = 20.0_f32.min(size.y);
        let checkbox_rect = Rect::new(
            position.x as i32,
            (position.y + (size.y - checkbox_size) / 2.0) as i32,
            checkbox_size as u32,
            checkbox_size as u32,
        );
        
        // Draw checkbox background
        let bg_color = vec4_to_sdl_color(background_color);
        self.canvas.set_draw_color(bg_color);
        self.canvas.fill_rect(checkbox_rect)
            .map_err(|e| RenderError::RenderFailed(format!("Failed to draw checkbox background: {}", e)))?;
        
        // Draw checkbox border
        if border_width > 0.0 {
            self.draw_border(checkbox_rect, border_width, border_color)?;
        }
        
        // Draw checkmark if checked
        if is_checked {
            self.draw_checkmark(
                Vec2::new(checkbox_rect.x as f32, checkbox_rect.y as f32),
                checkbox_size,
                check_color
            )?;
        }
        
        // Draw label if provided
        if !label.is_empty() {
            let label_x = position.x + checkbox_size + 8.0; // 8px gap
            let label_y = position.y + (size.y - font_size) / 2.0;
            self.draw_text_ttf(label, Vec2::new(label_x, label_y), text_color, font_size, None)?;
        }
        
        Ok(())
    }
    
    /// Draw a slider control
    fn draw_slider(
        &mut self,
        position: Vec2,
        size: Vec2,
        value: f32,
        min_value: f32,
        max_value: f32,
        track_color: Vec4,
        thumb_color: Vec4,
        border_color: Vec4,
        border_width: f32,
    ) -> RenderResult<()> {
        let track_height = 6.0;
        let thumb_size = 16.0;
        
        // Calculate track rectangle
        let track_y = position.y + (size.y - track_height) / 2.0;
        let track_rect = Rect::new(
            position.x as i32,
            track_y as i32,
            size.x as u32,
            track_height as u32,
        );
        
        // Draw track
        let track_sdl_color = vec4_to_sdl_color(track_color);
        self.canvas.set_draw_color(track_sdl_color);
        self.canvas.fill_rect(track_rect)
            .map_err(|e| RenderError::RenderFailed(format!("Failed to draw slider track: {}", e)))?;
        
        // Draw track border
        if border_width > 0.0 {
            self.draw_border(track_rect, border_width, border_color)?;
        }
        
        // Calculate thumb position
        let value_normalized = ((value - min_value) / (max_value - min_value)).clamp(0.0, 1.0);
        let thumb_x = position.x + (size.x - thumb_size) * value_normalized;
        let thumb_y = position.y + (size.y - thumb_size) / 2.0;
        
        // Draw thumb
        let thumb_rect = Rect::new(
            thumb_x as i32,
            thumb_y as i32,
            thumb_size as u32,
            thumb_size as u32,
        );
        
        let thumb_sdl_color = vec4_to_sdl_color(thumb_color);
        self.canvas.set_draw_color(thumb_sdl_color);
        self.canvas.fill_rect(thumb_rect)
            .map_err(|e| RenderError::RenderFailed(format!("Failed to draw slider thumb: {}", e)))?;
        
        // Draw thumb border
        if border_width > 0.0 {
            self.draw_border(thumb_rect, border_width, border_color)?;
        }
        
        Ok(())
    }
    
    /// Draw text selection background
    fn draw_text_selection(
        &mut self,
        text_position: Vec2,
        text: &str,
        font_size: f32,
        selection_start: usize,
        selection_end: usize,
        selection_color: Vec4,
    ) -> RenderResult<()> {
        let start = selection_start.min(selection_end);
        let end = selection_start.max(selection_end);
        
        if start >= end || start >= text.len() {
            return Ok(()); // Invalid selection
        }
        
        // Calculate approximate character width and positions
        let char_width = font_size * 0.6; // Rough approximation
        let selection_start_x = text_position.x + (start as f32 * char_width);
        let selection_width = ((end - start) as f32 * char_width).min(text.len() as f32 * char_width - start as f32 * char_width);
        
        let selection_rect = Rect::new(
            selection_start_x as i32,
            text_position.y as i32,
            selection_width as u32,
            font_size as u32,
        );
        
        let selection_sdl_color = vec4_to_sdl_color(selection_color);
        self.canvas.set_draw_color(selection_sdl_color);
        self.canvas.fill_rect(selection_rect)
            .map_err(|e| RenderError::RenderFailed(format!("Failed to draw text selection: {}", e)))?;
        
        Ok(())
    }
    
    /// Draw text cursor (blinking vertical line)
    fn draw_text_cursor(
        &mut self,
        text_position: Vec2,
        text: &str,
        cursor_position: usize,
        font_size: f32,
        cursor_color: Vec4,
        input_height: f32,
    ) -> RenderResult<()> {
        let char_width = font_size * 0.6; // Rough approximation
        let cursor_x = text_position.x + (cursor_position.min(text.len()) as f32 * char_width);
        
        // Draw vertical cursor line
        let cursor_start_y = text_position.y + 2.0;
        let cursor_end_y = text_position.y + input_height - 4.0;
        
        let cursor_sdl_color = vec4_to_sdl_color(cursor_color);
        self.canvas.set_draw_color(cursor_sdl_color);
        
        // Draw a 2-pixel wide cursor line
        for i in 0..2 {
            self.canvas.draw_line(
                ((cursor_x + i as f32) as i32, cursor_start_y as i32),
                ((cursor_x + i as f32) as i32, cursor_end_y as i32)
            ).map_err(|e| RenderError::RenderFailed(format!("Failed to draw cursor: {}", e)))?;
        }
        
        Ok(())
    }
    
    /// Draw a checkmark symbol
    fn draw_checkmark(
        &mut self,
        position: Vec2,
        size: f32,
        color: Vec4,
    ) -> RenderResult<()> {
        let check_color = vec4_to_sdl_color(color);
        self.canvas.set_draw_color(check_color);
        
        // Draw checkmark as two lines forming a "check" shape
        let center_x = position.x + size / 2.0;
        let center_y = position.y + size / 2.0;
        let check_size = size * 0.6;
        
        // First line (left part of check)
        let start1_x = center_x - check_size / 2.0;
        let start1_y = center_y;
        let mid_x = center_x - check_size / 6.0;
        let mid_y = center_y + check_size / 3.0;
        
        // Second line (right part of check)
        let end_x = center_x + check_size / 2.0;
        let end_y = center_y - check_size / 3.0;
        
        // Draw thick checkmark lines
        for thickness in 0..3 {
            let offset = thickness as f32;
            
            // Left part
            self.canvas.draw_line(
                ((start1_x + offset) as i32, (start1_y + offset) as i32),
                ((mid_x + offset) as i32, (mid_y + offset) as i32)
            ).map_err(|e| RenderError::RenderFailed(format!("Failed to draw checkmark: {}", e)))?;
            
            // Right part
            self.canvas.draw_line(
                ((mid_x + offset) as i32, (mid_y + offset) as i32),
                ((end_x + offset) as i32, (end_y + offset) as i32)
            ).map_err(|e| RenderError::RenderFailed(format!("Failed to draw checkmark: {}", e)))?;
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
        SdlKey::Left => KeyCode::ArrowLeft,
        SdlKey::Right => KeyCode::ArrowRight,
        SdlKey::Up => KeyCode::ArrowUp,
        SdlKey::Down => KeyCode::ArrowDown,
        SdlKey::Home => KeyCode::Home,
        SdlKey::End => KeyCode::End,
        _ => return None,
    })
}

fn convert_sdl_modifiers(keymod: sdl2::keyboard::Mod) -> KeyModifiers {
    use sdl2::keyboard::Mod;
    
    let mut modifiers = KeyModifiers::none();
    
    if keymod.contains(Mod::LSHIFTMOD) || keymod.contains(Mod::RSHIFTMOD) {
        modifiers = modifiers.shift();
    }
    if keymod.contains(Mod::LCTRLMOD) || keymod.contains(Mod::RCTRLMOD) {
        modifiers = modifiers.ctrl();
    }
    if keymod.contains(Mod::LALTMOD) || keymod.contains(Mod::RALTMOD) {
        modifiers = modifiers.alt();
    }
    if keymod.contains(Mod::LGUIMOD) || keymod.contains(Mod::RGUIMOD) {
        modifiers = modifiers.meta();
    }
    
    modifiers
}

// Re-export commonly used types
pub use sdl2;