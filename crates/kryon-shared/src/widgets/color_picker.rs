use super::{KryonWidget, EventResult, LayoutConstraints, PropertyBag};
use crate::elements::ElementId;
use crate::types::{InputEvent, LayoutResult, KeyCode, MouseButton};
use crate::render::RenderCommand;
use glam::{Vec2, Vec4};
use serde::{Serialize, Deserialize};
use anyhow::Result;

/// Color picker widget with HSV color wheel, palette, and input fields
#[derive(Default)]
pub struct ColorPickerWidget;

impl ColorPickerWidget {
    pub fn new() -> Self {
        Self
    }
}

/// State for the color picker widget
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct ColorPickerState {
    /// Currently selected color (RGBA)
    pub selected_color: Vec4,
    /// HSV representation of the selected color
    pub hsv_color: HsvColor,
    /// Whether the picker is open
    pub is_open: bool,
    /// Current input mode
    pub input_mode: ColorInputMode,
    /// Text input values for each channel
    pub text_inputs: ColorTextInputs,
    /// Predefined color palette
    pub color_palette: Vec<Vec4>,
    /// Recently used colors
    pub recent_colors: Vec<Vec4>,
    /// Whether currently dragging in color wheel
    pub is_dragging: bool,
    /// Current drag target
    pub drag_target: Option<DragTarget>,
}

impl Default for ColorPickerState {
    fn default() -> Self {
        Self {
            selected_color: Vec4::new(1.0, 0.0, 0.0, 1.0), // Red
            hsv_color: HsvColor::from_rgb(Vec4::new(1.0, 0.0, 0.0, 1.0)),
            is_open: false,
            input_mode: ColorInputMode::Rgb,
            text_inputs: ColorTextInputs::default(),
            color_palette: Self::default_palette(),
            recent_colors: Vec::new(),
            is_dragging: false,
            drag_target: None,
        }
    }
}

impl ColorPickerState {
    fn default_palette() -> Vec<Vec4> {
        vec![
            Vec4::new(1.0, 0.0, 0.0, 1.0), // Red
            Vec4::new(0.0, 1.0, 0.0, 1.0), // Green
            Vec4::new(0.0, 0.0, 1.0, 1.0), // Blue
            Vec4::new(1.0, 1.0, 0.0, 1.0), // Yellow
            Vec4::new(1.0, 0.0, 1.0, 1.0), // Magenta
            Vec4::new(0.0, 1.0, 1.0, 1.0), // Cyan
            Vec4::new(1.0, 1.0, 1.0, 1.0), // White
            Vec4::new(0.0, 0.0, 0.0, 1.0), // Black
        ]
    }
}

/// HSV color representation
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct HsvColor {
    pub hue: f32,        // 0-360
    pub saturation: f32, // 0-1
    pub value: f32,      // 0-1
    pub alpha: f32,      // 0-1
}

impl HsvColor {
    pub fn new(hue: f32, saturation: f32, value: f32, alpha: f32) -> Self {
        Self { hue, saturation, value, alpha }
    }
    
    pub fn from_rgb(rgb: Vec4) -> Self {
        let r = rgb.x;
        let g = rgb.y;
        let b = rgb.z;
        let a = rgb.w;
        
        let max = r.max(g).max(b);
        let min = r.min(g).min(b);
        let delta = max - min;
        
        let hue = if delta == 0.0 {
            0.0
        } else if max == r {
            60.0 * (((g - b) / delta) % 6.0)
        } else if max == g {
            60.0 * (((b - r) / delta) + 2.0)
        } else {
            60.0 * (((r - g) / delta) + 4.0)
        };
        
        let saturation = if max == 0.0 { 0.0 } else { delta / max };
        let value = max;
        
        Self::new(hue.abs(), saturation, value, a)
    }
    
    pub fn to_rgb(&self) -> Vec4 {
        let c = self.value * self.saturation;
        let x = c * (1.0 - ((self.hue / 60.0) % 2.0 - 1.0).abs());
        let m = self.value - c;
        
        let (r_prime, g_prime, b_prime) = if self.hue < 60.0 {
            (c, x, 0.0)
        } else if self.hue < 120.0 {
            (x, c, 0.0)
        } else if self.hue < 180.0 {
            (0.0, c, x)
        } else if self.hue < 240.0 {
            (0.0, x, c)
        } else if self.hue < 300.0 {
            (x, 0.0, c)
        } else {
            (c, 0.0, x)
        };
        
        Vec4::new(r_prime + m, g_prime + m, b_prime + m, self.alpha)
    }
}

/// Color input modes
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum ColorInputMode {
    Rgb,
    Hsv,
    Hex,
}

/// Text input values for color channels
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct ColorTextInputs {
    pub rgb: RgbInputs,
    pub hsv: HsvInputs,
    pub hex: String,
}

impl Default for ColorTextInputs {
    fn default() -> Self {
        Self {
            rgb: RgbInputs::default(),
            hsv: HsvInputs::default(),
            hex: "#FF0000".to_string(),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct RgbInputs {
    pub r: String,
    pub g: String,
    pub b: String,
    pub a: String,
}

impl Default for RgbInputs {
    fn default() -> Self {
        Self {
            r: "255".to_string(),
            g: "0".to_string(),
            b: "0".to_string(),
            a: "255".to_string(),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct HsvInputs {
    pub h: String,
    pub s: String,
    pub v: String,
    pub a: String,
}

impl Default for HsvInputs {
    fn default() -> Self {
        Self {
            h: "0".to_string(),
            s: "100".to_string(),
            v: "100".to_string(),
            a: "100".to_string(),
        }
    }
}

/// Drag targets in the color picker
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum DragTarget {
    ColorWheel,
    SaturationValue,
    HueSlider,
    AlphaSlider,
}

/// Configuration for the color picker widget
#[derive(Debug, Clone)]
pub struct ColorPickerConfig {
    /// Background color
    pub background_color: Vec4,
    /// Text color
    pub text_color: Vec4,
    /// Border color
    pub border_color: Vec4,
    /// Font size
    pub font_size: f32,
    /// Border radius
    pub border_radius: f32,
    /// Show alpha channel controls
    pub show_alpha: bool,
    /// Show color palette
    pub show_palette: bool,
    /// Show recent colors
    pub show_recent: bool,
    /// Show text inputs
    pub show_inputs: bool,
    /// Default input mode
    pub default_input_mode: ColorInputMode,
    /// Color wheel size
    pub wheel_size: f32,
    /// Picker popup width
    pub picker_width: f32,
    /// Picker popup height
    pub picker_height: f32,
    /// Maximum recent colors to remember
    pub max_recent_colors: usize,
}

impl Default for ColorPickerConfig {
    fn default() -> Self {
        Self {
            background_color: Vec4::new(1.0, 1.0, 1.0, 1.0),
            text_color: Vec4::new(0.2, 0.2, 0.2, 1.0),
            border_color: Vec4::new(0.7, 0.7, 0.7, 1.0),
            font_size: 12.0,
            border_radius: 4.0,
            show_alpha: true,
            show_palette: true,
            show_recent: true,
            show_inputs: true,
            default_input_mode: ColorInputMode::Rgb,
            wheel_size: 150.0,
            picker_width: 280.0,
            picker_height: 320.0,
            max_recent_colors: 8,
        }
    }
}

impl From<&PropertyBag> for ColorPickerConfig {
    fn from(props: &PropertyBag) -> Self {
        let mut config = Self::default();
        
        if let Some(size) = props.get_f32("font_size") {
            config.font_size = size;
        }
        
        if let Some(radius) = props.get_f32("border_radius") {
            config.border_radius = radius;
        }
        
        if let Some(show_alpha) = props.get_bool("show_alpha") {
            config.show_alpha = show_alpha;
        }
        
        if let Some(show_palette) = props.get_bool("show_palette") {
            config.show_palette = show_palette;
        }
        
        if let Some(show_recent) = props.get_bool("show_recent") {
            config.show_recent = show_recent;
        }
        
        if let Some(show_inputs) = props.get_bool("show_inputs") {
            config.show_inputs = show_inputs;
        }
        
        if let Some(wheel_size) = props.get_f32("wheel_size") {
            config.wheel_size = wheel_size;
        }
        
        if let Some(width) = props.get_f32("picker_width") {
            config.picker_width = width;
        }
        
        if let Some(height) = props.get_f32("picker_height") {
            config.picker_height = height;
        }
        
        config
    }
}

impl KryonWidget for ColorPickerWidget {
    const WIDGET_NAME: &'static str = "color-picker";
    type State = ColorPickerState;
    type Config = ColorPickerConfig;
    
    fn create_state(&self) -> Self::State {
        Self::State::default()
    }
    
    fn render(
        &self,
        state: &Self::State,
        config: &Self::Config,
        layout: &LayoutResult,
        element_id: ElementId,
    ) -> Result<Vec<RenderCommand>> {
        let mut commands = Vec::new();
        
        // Color preview button
        commands.push(RenderCommand::DrawRect {
            position: layout.position,
            size: layout.size,
            color: state.selected_color,
            border_radius: config.border_radius,
            border_width: 2.0,
            border_color: config.border_color,
        });
        
        // Color picker popup
        if state.is_open {
            self.render_color_picker(&mut commands, state, config, layout)?;
        }
        
        Ok(commands)
    }
    
    fn handle_event(
        &self,
        state: &mut Self::State,
        config: &Self::Config,
        event: &InputEvent,
        layout: &LayoutResult,
    ) -> Result<EventResult> {
        match event {
            InputEvent::MousePress { position, button: MouseButton::Left } => {
                // Check if click is on color preview
                if self.point_in_rect(*position, layout.position, layout.size) {
                    state.is_open = !state.is_open;
                    return Ok(EventResult::HandledWithRedraw);
                }
                
                // Check if click is in color picker popup
                if state.is_open {
                    if let Some(drag_target) = self.get_drag_target(state, config, layout, *position) {
                        state.is_dragging = true;
                        state.drag_target = Some(drag_target.clone());
                        self.handle_color_interaction(state, config, layout, *position, &drag_target)?;
                        return Ok(EventResult::HandledWithRedraw);
                    } else if !self.point_in_picker_popup(state, config, layout, *position) {
                        // Click outside picker - close it
                        state.is_open = false;
                        return Ok(EventResult::HandledWithRedraw);
                    }
                }
                
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::MouseDrag { position, .. } => {
                if state.is_dragging {
                    if let Some(drag_target) = &state.drag_target {
                        self.handle_color_interaction(state, config, layout, *position, drag_target)?;
                        return Ok(EventResult::HandledWithRedraw);
                    }
                }
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::MouseRelease { .. } => {
                if state.is_dragging {
                    state.is_dragging = false;
                    state.drag_target = None;
                    return Ok(EventResult::HandledWithRedraw);
                }
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::KeyPress { key: KeyCode::Escape, .. } => {
                if state.is_open {
                    state.is_open = false;
                    return Ok(EventResult::HandledWithRedraw);
                }
                Ok(EventResult::NotHandled)
            }
            
            _ => Ok(EventResult::NotHandled),
        }
    }
    
    fn can_focus(&self) -> bool {
        true
    }
    
    fn layout_constraints(&self, config: &Self::Config) -> Option<LayoutConstraints> {
        Some(LayoutConstraints {
            min_width: Some(40.0),
            max_width: None,
            min_height: Some(32.0),
            max_height: None,
            aspect_ratio: None,
        })
    }
}

impl ColorPickerWidget {
    fn render_color_picker(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &ColorPickerState,
        config: &ColorPickerConfig,
        layout: &LayoutResult,
    ) -> Result<()> {
        let picker_x = layout.position.x;
        let picker_y = layout.position.y + layout.size.y + 4.0;
        let picker_pos = Vec2::new(picker_x, picker_y);
        
        // Picker background
        commands.push(RenderCommand::DrawRect {
            position: picker_pos,
            size: Vec2::new(config.picker_width, config.picker_height),
            color: config.background_color,
            border_radius: config.border_radius,
            border_width: 1.0,
            border_color: config.border_color,
        });
        
        let mut y_offset = 12.0;
        
        // Color wheel
        self.render_color_wheel(commands, state, config, picker_pos + Vec2::new(12.0, y_offset))?;
        y_offset += config.wheel_size + 12.0;
        
        // Color sliders
        if config.show_alpha {
            self.render_alpha_slider(commands, state, config, 
                picker_pos + Vec2::new(12.0, y_offset), config.picker_width - 24.0)?;
            y_offset += 24.0;
        }
        
        // Color palette
        if config.show_palette {
            self.render_color_palette(commands, state, config, 
                picker_pos + Vec2::new(12.0, y_offset), config.picker_width - 24.0)?;
            y_offset += 32.0;
        }
        
        // Recent colors
        if config.show_recent && !state.recent_colors.is_empty() {
            self.render_recent_colors(commands, state, config, 
                picker_pos + Vec2::new(12.0, y_offset), config.picker_width - 24.0)?;
            y_offset += 32.0;
        }
        
        // Text inputs
        if config.show_inputs {
            self.render_color_inputs(commands, state, config, 
                picker_pos + Vec2::new(12.0, y_offset), config.picker_width - 24.0)?;
        }
        
        Ok(())
    }
    
    fn render_color_wheel(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &ColorPickerState,
        config: &ColorPickerConfig,
        position: Vec2,
    ) -> Result<()> {
        let center = position + Vec2::new(config.wheel_size / 2.0, config.wheel_size / 2.0);
        let radius = config.wheel_size / 2.0 - 5.0;
        
        // Render color wheel (simplified - would need proper HSV color wheel rendering)
        // For now, just draw a circle with segments
        let segments = 360;
        for i in 0..segments {
            let angle = (i as f32) * std::f32::consts::PI * 2.0 / segments as f32;
            let hue = (i as f32) * 360.0 / segments as f32;
            let color = HsvColor::new(hue, 1.0, 1.0, 1.0).to_rgb();
            
            let x1 = center.x + (radius * 0.7) * angle.cos();
            let y1 = center.y + (radius * 0.7) * angle.sin();
            let x2 = center.x + radius * angle.cos();
            let y2 = center.y + radius * angle.sin();
            
            commands.push(RenderCommand::DrawLine {
                start: Vec2::new(x1, y1),
                end: Vec2::new(x2, y2),
                color,
                width: 2.0,
            });
        }
        
        // Saturation/Value square in the center
        let square_size = radius * 0.6;
        let square_pos = center - Vec2::new(square_size / 2.0, square_size / 2.0);
        
        // Background (white to current hue)
        let hue_color = HsvColor::new(state.hsv_color.hue, 1.0, 1.0, 1.0).to_rgb();
        commands.push(RenderCommand::DrawRect {
            position: square_pos,
            size: Vec2::new(square_size, square_size),
            color: hue_color,
            border_radius: 0.0,
            border_width: 1.0,
            border_color: config.border_color,
        });
        
        // Selection indicator
        let selection_x = square_pos.x + state.hsv_color.saturation * square_size;
        let selection_y = square_pos.y + (1.0 - state.hsv_color.value) * square_size;
        
        commands.push(RenderCommand::DrawRect {
            position: Vec2::new(selection_x - 3.0, selection_y - 3.0),
            size: Vec2::new(6.0, 6.0),
            color: Vec4::new(1.0, 1.0, 1.0, 1.0),
            border_radius: 3.0,
            border_width: 2.0,
            border_color: Vec4::new(0.0, 0.0, 0.0, 1.0),
        });
        
        Ok(())
    }
    
    fn render_alpha_slider(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &ColorPickerState,
        config: &ColorPickerConfig,
        position: Vec2,
        width: f32,
    ) -> Result<()> {
        let height = 16.0;
        
        // Checkerboard background for transparency
        let checker_size = 8.0;
        let checkers_x = (width / checker_size).ceil() as i32;
        let checkers_y = (height / checker_size).ceil() as i32;
        
        for x in 0..checkers_x {
            for y in 0..checkers_y {
                let checker_color = if (x + y) % 2 == 0 {
                    Vec4::new(0.9, 0.9, 0.9, 1.0)
                } else {
                    Vec4::new(0.7, 0.7, 0.7, 1.0)
                };
                
                commands.push(RenderCommand::DrawRect {
                    position: position + Vec2::new(x as f32 * checker_size, y as f32 * checker_size),
                    size: Vec2::new(checker_size.min(width - x as f32 * checker_size), 
                                   checker_size.min(height - y as f32 * checker_size)),
                    color: checker_color,
                    border_radius: 0.0,
                    border_width: 0.0,
                    border_color: Vec4::ZERO,
                });
            }
        }
        
        // Alpha gradient overlay
        let base_color = Vec4::new(state.selected_color.x, state.selected_color.y, state.selected_color.z, 0.0);
        let full_color = Vec4::new(state.selected_color.x, state.selected_color.y, state.selected_color.z, 1.0);
        
        // Simplified gradient - would need proper gradient rendering
        commands.push(RenderCommand::DrawRect {
            position,
            size: Vec2::new(width, height),
            color: full_color,
            border_radius: 0.0,
            border_width: 1.0,
            border_color: config.border_color,
        });
        
        // Alpha slider handle
        let handle_x = position.x + state.hsv_color.alpha * width;
        commands.push(RenderCommand::DrawRect {
            position: Vec2::new(handle_x - 2.0, position.y - 2.0),
            size: Vec2::new(4.0, height + 4.0),
            color: Vec4::new(1.0, 1.0, 1.0, 1.0),
            border_radius: 2.0,
            border_width: 1.0,
            border_color: Vec4::new(0.0, 0.0, 0.0, 1.0),
        });
        
        Ok(())
    }
    
    fn render_color_palette(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &ColorPickerState,
        config: &ColorPickerConfig,
        position: Vec2,
        width: f32,
    ) -> Result<()> {
        let swatch_size = 24.0;
        let spacing = 4.0;
        let swatches_per_row = ((width + spacing) / (swatch_size + spacing)) as usize;
        
        for (i, &color) in state.color_palette.iter().enumerate() {
            let row = i / swatches_per_row;
            let col = i % swatches_per_row;
            
            let swatch_x = position.x + col as f32 * (swatch_size + spacing);
            let swatch_y = position.y + row as f32 * (swatch_size + spacing);
            
            commands.push(RenderCommand::DrawRect {
                position: Vec2::new(swatch_x, swatch_y),
                size: Vec2::new(swatch_size, swatch_size),
                color,
                border_radius: 2.0,
                border_width: 1.0,
                border_color: config.border_color,
            });
        }
        
        Ok(())
    }
    
    fn render_recent_colors(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &ColorPickerState,
        config: &ColorPickerConfig,
        position: Vec2,
        width: f32,
    ) -> Result<()> {
        let swatch_size = 20.0;
        let spacing = 2.0;
        
        commands.push(RenderCommand::DrawText {
            text: "Recent:".to_string(),
            position,
            font_size: config.font_size,
            color: config.text_color,
            max_width: None,
        });
        
        let recent_start = position + Vec2::new(50.0, 0.0);
        
        for (i, &color) in state.recent_colors.iter().enumerate() {
            let swatch_x = recent_start.x + i as f32 * (swatch_size + spacing);
            
            commands.push(RenderCommand::DrawRect {
                position: Vec2::new(swatch_x, recent_start.y),
                size: Vec2::new(swatch_size, swatch_size),
                color,
                border_radius: 2.0,
                border_width: 1.0,
                border_color: config.border_color,
            });
        }
        
        Ok(())
    }
    
    fn render_color_inputs(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &ColorPickerState,
        config: &ColorPickerConfig,
        position: Vec2,
        width: f32,
    ) -> Result<()> {
        let input_height = 20.0;
        let input_width = 50.0;
        let label_width = 20.0;
        let spacing = 8.0;
        
        match state.input_mode {
            ColorInputMode::Rgb => {
                let labels = ["R:", "G:", "B:"];
                let values = [&state.text_inputs.rgb.r, &state.text_inputs.rgb.g, &state.text_inputs.rgb.b];
                
                for (i, (&label, value)) in labels.iter().zip(values.iter()).enumerate() {
                    let y = position.y + i as f32 * (input_height + spacing);
                    
                    // Label
                    commands.push(RenderCommand::DrawText {
                        text: label.to_string(),
                        position: Vec2::new(position.x, y + 4.0),
                        font_size: config.font_size,
                        color: config.text_color,
                        max_width: Some(label_width),
                    });
                    
                    // Input field
                    commands.push(RenderCommand::DrawRect {
                        position: Vec2::new(position.x + label_width + 4.0, y),
                        size: Vec2::new(input_width, input_height),
                        color: config.background_color,
                        border_radius: 2.0,
                        border_width: 1.0,
                        border_color: config.border_color,
                    });
                    
                    commands.push(RenderCommand::DrawText {
                        text: value.clone(),
                        position: Vec2::new(position.x + label_width + 8.0, y + 4.0),
                        font_size: config.font_size,
                        color: config.text_color,
                        max_width: Some(input_width - 8.0),
                    });
                }
            }
            ColorInputMode::Hsv => {
                let labels = ["H:", "S:", "V:"];
                let values = [&state.text_inputs.hsv.h, &state.text_inputs.hsv.s, &state.text_inputs.hsv.v];
                
                for (i, (&label, value)) in labels.iter().zip(values.iter()).enumerate() {
                    let y = position.y + i as f32 * (input_height + spacing);
                    
                    commands.push(RenderCommand::DrawText {
                        text: label.to_string(),
                        position: Vec2::new(position.x, y + 4.0),
                        font_size: config.font_size,
                        color: config.text_color,
                        max_width: Some(label_width),
                    });
                    
                    commands.push(RenderCommand::DrawRect {
                        position: Vec2::new(position.x + label_width + 4.0, y),
                        size: Vec2::new(input_width, input_height),
                        color: config.background_color,
                        border_radius: 2.0,
                        border_width: 1.0,
                        border_color: config.border_color,
                    });
                    
                    commands.push(RenderCommand::DrawText {
                        text: value.clone(),
                        position: Vec2::new(position.x + label_width + 8.0, y + 4.0),
                        font_size: config.font_size,
                        color: config.text_color,
                        max_width: Some(input_width - 8.0),
                    });
                }
            }
            ColorInputMode::Hex => {
                commands.push(RenderCommand::DrawText {
                    text: "Hex:".to_string(),
                    position: Vec2::new(position.x, position.y + 4.0),
                    font_size: config.font_size,
                    color: config.text_color,
                    max_width: Some(label_width),
                });
                
                commands.push(RenderCommand::DrawRect {
                    position: Vec2::new(position.x + label_width + 4.0, position.y),
                    size: Vec2::new(input_width + 20.0, input_height),
                    color: config.background_color,
                    border_radius: 2.0,
                    border_width: 1.0,
                    border_color: config.border_color,
                });
                
                commands.push(RenderCommand::DrawText {
                    text: state.text_inputs.hex.clone(),
                    position: Vec2::new(position.x + label_width + 8.0, position.y + 4.0),
                    font_size: config.font_size,
                    color: config.text_color,
                    max_width: Some(input_width + 12.0),
                });
            }
        }
        
        Ok(())
    }
    
    fn get_drag_target(
        &self,
        state: &ColorPickerState,
        config: &ColorPickerConfig,
        layout: &LayoutResult,
        position: Vec2,
    ) -> Option<DragTarget> {
        let picker_pos = Vec2::new(layout.position.x, layout.position.y + layout.size.y + 4.0);
        
        // Check color wheel area
        let wheel_center = picker_pos + Vec2::new(12.0 + config.wheel_size / 2.0, 12.0 + config.wheel_size / 2.0);
        let wheel_radius = config.wheel_size / 2.0 - 5.0;
        let distance_from_center = (position - wheel_center).length();
        
        if distance_from_center <= wheel_radius {
            if distance_from_center > wheel_radius * 0.6 {
                return Some(DragTarget::ColorWheel);
            } else {
                return Some(DragTarget::SaturationValue);
            }
        }
        
        // Check alpha slider
        if config.show_alpha {
            let alpha_y = picker_pos.y + 12.0 + config.wheel_size + 12.0;
            if position.y >= alpha_y && position.y <= alpha_y + 16.0 &&
               position.x >= picker_pos.x + 12.0 && position.x <= picker_pos.x + config.picker_width - 12.0 {
                return Some(DragTarget::AlphaSlider);
            }
        }
        
        None
    }
    
    fn handle_color_interaction(
        &self,
        state: &mut ColorPickerState,
        config: &ColorPickerConfig,
        layout: &LayoutResult,
        position: Vec2,
        drag_target: &DragTarget,
    ) -> Result<()> {
        let picker_pos = Vec2::new(layout.position.x, layout.position.y + layout.size.y + 4.0);
        
        match drag_target {
            DragTarget::ColorWheel => {
                let wheel_center = picker_pos + Vec2::new(12.0 + config.wheel_size / 2.0, 12.0 + config.wheel_size / 2.0);
                let angle = (position.y - wheel_center.y).atan2(position.x - wheel_center.x);
                let hue = (angle.to_degrees() + 360.0) % 360.0;
                state.hsv_color.hue = hue;
            }
            DragTarget::SaturationValue => {
                let square_center = picker_pos + Vec2::new(12.0 + config.wheel_size / 2.0, 12.0 + config.wheel_size / 2.0);
                let square_size = (config.wheel_size / 2.0 - 5.0) * 0.6;
                let square_pos = square_center - Vec2::new(square_size / 2.0, square_size / 2.0);
                
                let relative_pos = position - square_pos;
                state.hsv_color.saturation = (relative_pos.x / square_size).clamp(0.0, 1.0);
                state.hsv_color.value = 1.0 - (relative_pos.y / square_size).clamp(0.0, 1.0);
            }
            DragTarget::AlphaSlider => {
                let alpha_x = picker_pos.x + 12.0;
                let alpha_width = config.picker_width - 24.0;
                let relative_x = position.x - alpha_x;
                state.hsv_color.alpha = (relative_x / alpha_width).clamp(0.0, 1.0);
            }
            _ => {}
        }
        
        // Update selected color
        state.selected_color = state.hsv_color.to_rgb();
        
        // Update text inputs
        self.update_text_inputs(state);
        
        Ok(())
    }
    
    fn update_text_inputs(&self, state: &mut ColorPickerState) {
        let rgb = state.selected_color;
        
        state.text_inputs.rgb.r = ((rgb.x * 255.0) as u8).to_string();
        state.text_inputs.rgb.g = ((rgb.y * 255.0) as u8).to_string();
        state.text_inputs.rgb.b = ((rgb.z * 255.0) as u8).to_string();
        state.text_inputs.rgb.a = ((rgb.w * 255.0) as u8).to_string();
        
        state.text_inputs.hsv.h = (state.hsv_color.hue as u16).to_string();
        state.text_inputs.hsv.s = ((state.hsv_color.saturation * 100.0) as u8).to_string();
        state.text_inputs.hsv.v = ((state.hsv_color.value * 100.0) as u8).to_string();
        state.text_inputs.hsv.a = ((state.hsv_color.alpha * 100.0) as u8).to_string();
        
        state.text_inputs.hex = format!("#{:02X}{:02X}{:02X}", 
            (rgb.x * 255.0) as u8, 
            (rgb.y * 255.0) as u8, 
            (rgb.z * 255.0) as u8
        );
    }
    
    fn point_in_rect(&self, point: Vec2, rect_pos: Vec2, rect_size: Vec2) -> bool {
        point.x >= rect_pos.x &&
        point.x <= rect_pos.x + rect_size.x &&
        point.y >= rect_pos.y &&
        point.y <= rect_pos.y + rect_size.y
    }
    
    fn point_in_picker_popup(
        &self,
        state: &ColorPickerState,
        config: &ColorPickerConfig,
        layout: &LayoutResult,
        position: Vec2,
    ) -> bool {
        let picker_pos = Vec2::new(layout.position.x, layout.position.y + layout.size.y + 4.0);
        let picker_size = Vec2::new(config.picker_width, config.picker_height);
        
        self.point_in_rect(position, picker_pos, picker_size)
    }
}

// Implement the factory trait
crate::impl_widget_factory!(ColorPickerWidget);

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_color_picker_creation() {
        let widget = ColorPickerWidget::new();
        let state = widget.create_state();
        
        assert_eq!(state.selected_color, Vec4::new(1.0, 0.0, 0.0, 1.0));
        assert!(!state.is_open);
    }
    
    #[test]
    fn test_hsv_color_conversion() {
        let rgb = Vec4::new(1.0, 0.0, 0.0, 1.0); // Red
        let hsv = HsvColor::from_rgb(rgb);
        
        assert_eq!(hsv.hue, 0.0);
        assert_eq!(hsv.saturation, 1.0);
        assert_eq!(hsv.value, 1.0);
        
        let back_to_rgb = hsv.to_rgb();
        assert!((back_to_rgb.x - 1.0).abs() < 0.01);
        assert!((back_to_rgb.y - 0.0).abs() < 0.01);
        assert!((back_to_rgb.z - 0.0).abs() < 0.01);
    }
    
    #[test]
    fn test_color_palette() {
        let palette = ColorPickerState::default_palette();
        assert_eq!(palette.len(), 8);
        assert_eq!(palette[0], Vec4::new(1.0, 0.0, 0.0, 1.0)); // Red
        assert_eq!(palette[7], Vec4::new(0.0, 0.0, 0.0, 1.0)); // Black
    }
}