use glam::{Vec2, Vec4};
use ratatui::{
    backend::Backend,
    layout::{Alignment, Rect},
    style::{Color, Style},
    widgets::{Block, Clear, Paragraph, Wrap},
    Frame, Terminal,
};

use kryon_core::TextAlignment;
use kryon_render::{CommandRenderer, RenderCommand, RenderError, RenderResult, Renderer};

pub struct RatatuiRenderer<B: Backend> {
    pub terminal: Terminal<B>,
    source_size: Vec2,
}

pub struct RatatuiContext; // A simple marker context

impl<B: Backend> Renderer for RatatuiRenderer<B> {
    type Surface = B;
    type Context = RatatuiContext;

    fn initialize(surface: Self::Surface) -> RenderResult<Self> {
        let terminal = Terminal::new(surface)
            .map_err(|e| RenderError::InitializationFailed(e.to_string()))?;
        Ok(Self {
            terminal,
            source_size: Vec2::new(800.0, 600.0), // Default, will be updated
        })
    }

    fn begin_frame(&mut self, _clear_color: Vec4) -> RenderResult<Self::Context> {
        Ok(RatatuiContext)
    }

    fn end_frame(&mut self, _context: Self::Context) -> RenderResult<()> {
        Ok(())
    }

    fn render_element(&mut self, _c: &mut Self::Context, _: &kryon_core::Element, _: &kryon_layout::LayoutResult, _: kryon_core::ElementId) -> RenderResult<()> { Ok(()) }
    
    fn resize(&mut self, new_size: Vec2) -> RenderResult<()> {
        self.terminal
            .resize(Rect::new(0, 0, new_size.x as u16, new_size.y as u16))
            .map_err(|e| RenderError::RenderFailed(format!("Terminal resize failed: {}", e)))
    }
    
    fn viewport_size(&self) -> Vec2 {
        let size = self.terminal.size().unwrap_or_default();
        Vec2::new(size.width as f32, size.height as f32)
    }
}

impl<B: Backend> CommandRenderer for RatatuiRenderer<B> {
    fn execute_commands(
        &mut self,
        _context: &mut Self::Context,
        commands: &[RenderCommand],
    ) -> RenderResult<()> {
        self.terminal.draw(|frame| {
            // First Pass: Configuration
            for command in commands {
                if let RenderCommand::SetCanvasSize(size) = command {
                    if size.x > 0.0 && size.y > 0.0 {
                        self.source_size = *size;
                    }
                }
            }

            // Second Pass: Drawing
            render_commands_to_frame(commands, frame, self.source_size);

        }).map_err(|e| RenderError::RenderFailed(e.to_string()))?;

        Ok(())
    }
}

fn render_commands_to_frame(commands: &[RenderCommand], frame: &mut Frame, app_canvas_size: Vec2) {
    let terminal_area = frame.size();

    for command in commands {
        match command {
            RenderCommand::DrawRect { position, size, color, border_width, border_color, transform, .. } => {
                let (final_position, final_size) = apply_transform_ratatui(*position, *size, transform);
                if let Some(area) = translate_rect(final_position, final_size, app_canvas_size, terminal_area) {
                    let mut block = Block::default().style(Style::default().bg(vec4_to_ratatui_color(*color)));
                    if *border_width > 0.0 {
                        block = block.borders(ratatui::widgets::Borders::ALL)
                                     .border_style(Style::default().fg(vec4_to_ratatui_color(*border_color)));
                    }
                    frame.render_widget(Clear, area);
                    frame.render_widget(block, area);
                }
            }
            RenderCommand::DrawText { position, text, alignment, color, max_width, transform, .. } => {
                let text_width = max_width.unwrap_or(text.len() as f32 * 8.0);
                let text_size = Vec2::new(text_width, 16.0); 

                let (final_position, final_size) = apply_transform_ratatui(*position, text_size, transform);
                if let Some(area) = translate_rect(final_position, final_size, app_canvas_size, terminal_area) {
                    let paragraph = Paragraph::new(text.as_str())
                        .style(Style::default().fg(vec4_to_ratatui_color(*color)))
                        .alignment(match alignment {
                            TextAlignment::Start => Alignment::Left,
                            TextAlignment::Center => Alignment::Center,
                            TextAlignment::End => Alignment::Right,
                            TextAlignment::Justify => Alignment::Left,
                        });
                    frame.render_widget(paragraph, area);
                }
            }
            RenderCommand::SetCanvasSize(_) => {},
            
            // Basic 2D drawing commands (limited support in terminal)
            RenderCommand::DrawLine { start, end, color, width: _, transform, .. } => {
                let (final_start, _) = apply_transform_ratatui(*start, Vec2::new(1.0, 1.0), transform);
                let (final_end, _) = apply_transform_ratatui(*end, Vec2::new(1.0, 1.0), transform);
                
                // Very basic line rendering using dashes or pipe characters
                let dx = final_end.x - final_start.x;
                let dy = final_end.y - final_start.y;
                let length = (dx * dx + dy * dy).sqrt();
                
                if length > 0.0 {
                    // Approximate line with a single character at midpoint
                    let mid_x = (final_start.x + final_end.x) / 2.0;
                    let mid_y = (final_start.y + final_end.y) / 2.0;
                    
                    if let Some(area) = translate_rect(Vec2::new(mid_x, mid_y), Vec2::new(8.0, 16.0), app_canvas_size, terminal_area) {
                        let line_char = if dx.abs() > dy.abs() { "─" } else { "│" };
                        let paragraph = Paragraph::new(line_char)
                            .style(Style::default().fg(vec4_to_ratatui_color(*color)));
                        frame.render_widget(paragraph, area);
                    }
                }
            }
            
            RenderCommand::DrawCircle { center, radius, fill_color, stroke_color: _, stroke_width: _, transform, .. } => {
                let (final_center, _) = apply_transform_ratatui(*center, Vec2::new(1.0, 1.0), transform);
                
                // Approximate circle with "●" character
                if let Some(fill) = fill_color {
                    let circle_size = Vec2::new(*radius * 2.0, *radius * 2.0);
                    let circle_pos = Vec2::new(final_center.x - radius, final_center.y - radius);
                    
                    if let Some(area) = translate_rect(circle_pos, circle_size, app_canvas_size, terminal_area) {
                        let paragraph = Paragraph::new("●")
                            .style(Style::default().fg(vec4_to_ratatui_color(*fill)))
                            .alignment(Alignment::Center);
                        frame.render_widget(paragraph, area);
                    }
                }
            }
            
            RenderCommand::DrawEllipse { center, rx, ry, fill_color, stroke_color: _, stroke_width: _, transform, .. } => {
                let (final_center, _) = apply_transform_ratatui(*center, Vec2::new(1.0, 1.0), transform);
                
                // Approximate ellipse with "○" character
                if let Some(fill) = fill_color {
                    let ellipse_size = Vec2::new(*rx * 2.0, *ry * 2.0);
                    let ellipse_pos = Vec2::new(final_center.x - rx, final_center.y - ry);
                    
                    if let Some(area) = translate_rect(ellipse_pos, ellipse_size, app_canvas_size, terminal_area) {
                        let paragraph = Paragraph::new("○")
                            .style(Style::default().fg(vec4_to_ratatui_color(*fill)))
                            .alignment(Alignment::Center);
                        frame.render_widget(paragraph, area);
                    }
                }
            }
            
            RenderCommand::DrawPolygon { points, fill_color, stroke_color: _, stroke_width: _, transform, .. } => {
                if points.len() >= 3 {
                    // Approximate polygon with "◆" character at centroid
                    if let Some(fill) = fill_color {
                        let centroid = points.iter().fold(Vec2::ZERO, |acc, p| acc + *p) / points.len() as f32;
                        let (final_centroid, _) = apply_transform_ratatui(centroid, Vec2::new(1.0, 1.0), transform);
                        
                        if let Some(area) = translate_rect(final_centroid, Vec2::new(16.0, 16.0), app_canvas_size, terminal_area) {
                            let paragraph = Paragraph::new("◆")
                                .style(Style::default().fg(vec4_to_ratatui_color(*fill)))
                                .alignment(Alignment::Center);
                            frame.render_widget(paragraph, area);
                        }
                    }
                }
            }
            
            RenderCommand::DrawPath { path_data: _, fill_color, stroke_color: _, stroke_width: _, transform: _, .. } => {
                // Path rendering is too complex for terminal - show placeholder
                if let Some(fill) = fill_color {
                    if let Some(area) = translate_rect(Vec2::new(50.0, 50.0), Vec2::new(100.0, 100.0), app_canvas_size, terminal_area) {
                        let paragraph = Paragraph::new("PATH")
                            .style(Style::default().fg(vec4_to_ratatui_color(*fill)))
                            .alignment(Alignment::Center);
                        frame.render_widget(paragraph, area);
                    }
                }
            }
            
            RenderCommand::DrawSvg { position, size, source, opacity: _, transform: _, background_color, .. } => {
                // SVG rendering is complex for terminal - show placeholder with source info
                if let Some(area) = translate_rect(*position, *size, app_canvas_size, terminal_area) {
                    let source_name = if source.ends_with(".svg") {
                        source.split('/').last().unwrap_or(source)
                    } else {
                        "SVG"
                    };
                    
                    let display_text = format!("SVG\n{}", source_name);
                    let color = background_color.map(vec4_to_ratatui_color).unwrap_or(Color::Yellow);
                    
                    let paragraph = Paragraph::new(display_text)
                        .style(Style::default().fg(color))
                        .alignment(Alignment::Center)
                        .wrap(Wrap { trim: true });
                    
                    // Add border to indicate SVG boundary
                    let block = ratatui::widgets::Block::default()
                        .borders(ratatui::widgets::Borders::ALL)
                        .border_style(ratatui::style::Style::default().fg(color))
                        .title("SVG");
                    let inner_area = block.inner(area);
                    
                    frame.render_widget(block, area);
                    frame.render_widget(paragraph, inner_area);
                }
            }
            // Canvas rendering commands
            RenderCommand::BeginCanvas { canvas_id: _, position, size } => {
                // For ratatui, we can draw a simple border to represent the canvas
                let canvas_area = translate_rect(*position, *size, app_canvas_size, terminal_area);
                if let Some(area) = canvas_area {
                    let block = ratatui::widgets::Block::default()
                        .borders(ratatui::widgets::Borders::ALL)
                        .border_style(ratatui::style::Style::default().fg(ratatui::style::Color::DarkGray))
                        .title("Canvas");
                    frame.render_widget(block, area);
                }
            }
            RenderCommand::EndCanvas => {
                // Nothing to do for ratatui - just a marker
            }
            RenderCommand::DrawCanvasRect { position, size, fill_color, stroke_color: _, stroke_width: _ } => {
                // For ratatui, draw a filled rectangle using block characters
                let canvas_area = translate_rect(*position, *size, app_canvas_size, terminal_area);
                if let Some(area) = canvas_area {
                    if let Some(fill) = fill_color {
                        let color = ratatui::style::Color::Rgb(
                            (fill.x * 255.0) as u8,
                            (fill.y * 255.0) as u8,
                            (fill.z * 255.0) as u8,
                        );
                        let block = ratatui::widgets::Block::default()
                            .style(ratatui::style::Style::default().bg(color));
                        frame.render_widget(block, area);
                    }
                }
            }
            RenderCommand::DrawCanvasCircle { center: _, radius: _, fill_color: _, stroke_color: _, stroke_width: _ } => {
                // Terminal circles are difficult - skip for now
            }
            RenderCommand::DrawCanvasLine { start: _, end: _, color: _, width: _ } => {
                // Terminal lines are difficult - skip for now
            }
            RenderCommand::DrawCanvasText { position, text, font_size: _, color, font_family: _, alignment: _ } => {
                // Draw text within the canvas area
                let text_area = translate_rect(*position, Vec2::new(text.len() as f32 * 8.0, 16.0), app_canvas_size, terminal_area);
                if let Some(area) = text_area {
                    let color = ratatui::style::Color::Rgb(
                        (color.x * 255.0) as u8,
                        (color.y * 255.0) as u8,
                        (color.z * 255.0) as u8,
                    );
                    let paragraph = ratatui::widgets::Paragraph::new(text.as_str())
                        .style(ratatui::style::Style::default().fg(color));
                    frame.render_widget(paragraph, area);
                }
            }
            // WASM View rendering commands
            RenderCommand::BeginWasmView { wasm_id: _, position, size } => {
                // For ratatui, draw a purple border to represent the WASM view
                let wasm_area = translate_rect(*position, *size, app_canvas_size, terminal_area);
                if let Some(area) = wasm_area {
                    let block = ratatui::widgets::Block::default()
                        .borders(ratatui::widgets::Borders::ALL)
                        .border_style(ratatui::style::Style::default().fg(ratatui::style::Color::Magenta))
                        .title("WASM View");
                    frame.render_widget(block, area);
                }
            }
            RenderCommand::EndWasmView => {
                // Nothing to do for ratatui - just a marker
            }
            RenderCommand::ExecuteWasmFunction { function_name: _, params: _ } => {
                // In terminal mode, WASM execution is limited - just log it
                // The actual WASM execution would happen elsewhere
            }
            RenderCommand::DrawSelectButton {
                position,
                size,
                text,
                is_open,
                font_size: _,
                text_color,
                background_color,
                border_color,
                border_width,
                transform,
            } => {
                let (final_position, final_size) = apply_transform_ratatui(*position, *size, transform);
                if let Some(area) = translate_rect(final_position, final_size, app_canvas_size, terminal_area) {
                    // Create the button background
                    let mut block = Block::default()
                        .style(Style::default().bg(vec4_to_ratatui_color(*background_color)));
                    
                    // Add border if specified
                    if *border_width > 0.0 {
                        block = block
                            .borders(ratatui::widgets::Borders::ALL)
                            .border_style(Style::default().fg(vec4_to_ratatui_color(*border_color)));
                    }
                    
                    // Render the block background
                    frame.render_widget(block.clone(), area);
                    
                    // Create dropdown arrow symbol
                    let arrow = if *is_open { "▲" } else { "▼" };
                    let button_text = format!(" {} {}", text, arrow);
                    
                    // Render the text content centered
                    let inner_area = if *border_width > 0.0 { block.inner(area) } else { area };
                    let paragraph = Paragraph::new(button_text)
                        .style(Style::default().fg(vec4_to_ratatui_color(*text_color)))
                        .alignment(Alignment::Center);
                    
                    frame.render_widget(paragraph, inner_area);
                }
            },
            RenderCommand::DrawSelectMenu {
                position,
                size,
                options,
                highlighted_index,
                font_size: _,
                text_color,
                background_color,
                border_color,
                border_width,
                highlight_color,
                selected_color,
                transform,
            } => {
                let (final_position, final_size) = apply_transform_ratatui(*position, *size, transform);
                if let Some(area) = translate_rect(final_position, final_size, app_canvas_size, terminal_area) {
                    // Create the menu background
                    let mut block = Block::default()
                        .style(Style::default().bg(vec4_to_ratatui_color(*background_color)));
                    
                    // Add border if specified
                    if *border_width > 0.0 {
                        block = block
                            .borders(ratatui::widgets::Borders::ALL)
                            .border_style(Style::default().fg(vec4_to_ratatui_color(*border_color)));
                    }
                    
                    // Render the block background
                    frame.render_widget(block.clone(), area);
                    
                    // Prepare the menu content
                    let inner_area = if *border_width > 0.0 { block.inner(area) } else { area };
                    
                    // Build the options text with proper highlighting
                    let mut menu_lines = Vec::new();
                    for (index, option) in options.iter().enumerate() {
                        let mut line_style = Style::default().fg(vec4_to_ratatui_color(*text_color));
                        
                        // Apply highlighting/selection styling
                        if Some(index) == *highlighted_index {
                            line_style = line_style.bg(vec4_to_ratatui_color(*highlight_color));
                        } else if option.selected {
                            line_style = line_style.bg(vec4_to_ratatui_color(*selected_color));
                        }
                        
                        // Dim text for disabled options
                        if option.disabled {
                            line_style = line_style.fg(Color::DarkGray);
                        }
                        
                        // Add selection marker for selected options
                        let prefix = if option.selected { "✓ " } else { "  " };
                        let option_text = format!("{}{}", prefix, option.text);
                        
                        menu_lines.push(ratatui::text::Line::from(vec![
                            ratatui::text::Span::styled(option_text, line_style)
                        ]));
                    }
                    
                    // Render the menu options
                    let paragraph = Paragraph::new(menu_lines)
                        .wrap(Wrap { trim: false });
                    
                    frame.render_widget(paragraph, inner_area);
                }
            },
            _ => {} 
        }
    }
}

fn translate_rect(source_pos: Vec2, source_size: Vec2, app_canvas_size: Vec2, terminal_area: Rect) -> Option<Rect> {
    if app_canvas_size.x == 0.0 || app_canvas_size.y == 0.0 { return None; }

    let rel_x = source_pos.x / app_canvas_size.x;
    let rel_y = source_pos.y / app_canvas_size.y;
    let rel_w = source_size.x / app_canvas_size.x;
    let rel_h = source_size.y / app_canvas_size.y;

    let target_w_f32 = terminal_area.width as f32;
    let target_h_f32 = terminal_area.height as f32;

    let term_x = (rel_x * target_w_f32).floor() as u16;
    let term_y = (rel_y * target_h_f32).floor() as u16;
    let term_w = (rel_w * target_w_f32).ceil() as u16;
    let term_h = (rel_h * target_h_f32).ceil() as u16;
    
    let final_x = term_x.min(terminal_area.right());
    let final_y = term_y.min(terminal_area.bottom());
    let final_w = term_w.min(terminal_area.width.saturating_sub(final_x));
    let final_h = term_h.min(terminal_area.height.saturating_sub(final_y));

    let final_rect = Rect::new(final_x, final_y, final_w, final_h);
    
    if final_rect.width > 0 && final_rect.height > 0 { Some(final_rect) } else { None }
}

fn vec4_to_ratatui_color(color: Vec4) -> Color {
    if color.w < 0.1 { return Color::Reset; }
    Color::Rgb((color.x * 255.0) as u8, (color.y * 255.0) as u8, (color.z * 255.0) as u8)
}

/// Apply basic transform to position and size for ratatui (text-based rendering)
/// Note: ratatui has limited transform capabilities, so we only handle basic translation and scaling
fn apply_transform_ratatui(position: Vec2, size: Vec2, transform: &Option<kryon_core::TransformData>) -> (Vec2, Vec2) {
    let Some(transform_data) = transform else {
        return (position, size);
    };
    
    let mut final_position = position;
    let mut final_size = size;
    
    // Apply transform properties
    for property in &transform_data.properties {
        match property.property_type {
            kryon_core::TransformPropertyType::Scale => {
                let scale_value = css_unit_to_value(&property.value);
                final_size.x *= scale_value;
                final_size.y *= scale_value;
            }
            kryon_core::TransformPropertyType::ScaleX => {
                let scale_value = css_unit_to_value(&property.value);
                final_size.x *= scale_value;
            }
            kryon_core::TransformPropertyType::ScaleY => {
                let scale_value = css_unit_to_value(&property.value);
                final_size.y *= scale_value;
            }
            kryon_core::TransformPropertyType::TranslateX => {
                let translate_value = css_unit_to_value(&property.value);
                final_position.x += translate_value;
            }
            kryon_core::TransformPropertyType::TranslateY => {
                let translate_value = css_unit_to_value(&property.value);
                final_position.y += translate_value;
            }
            // Note: Rotation and skew are not well-supported in text-based rendering
            // We'll ignore them for now
            _ => {
                // Ignore unsupported transform properties in text-based rendering
            }
        }
    }
    
    (final_position, final_size)
}

/// Convert CSS unit value to a simple float value for ratatui
fn css_unit_to_value(unit_value: &kryon_core::CSSUnitValue) -> f32 {
    match unit_value.unit {
        kryon_core::CSSUnit::Pixels => unit_value.value as f32,
        kryon_core::CSSUnit::Number => unit_value.value as f32,
        kryon_core::CSSUnit::Em => unit_value.value as f32 * 16.0, // Assume 16px base
        kryon_core::CSSUnit::Rem => unit_value.value as f32 * 16.0, // Assume 16px base
        kryon_core::CSSUnit::Percentage => unit_value.value as f32 / 100.0,
        _ => unit_value.value as f32, // Default fallback
    }
}