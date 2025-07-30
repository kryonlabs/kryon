use super::{KryonWidget, EventResult, LayoutConstraints, PropertyBag};
use crate::elements::ElementId;
use crate::types::{InputEvent, LayoutResult, KeyCode, MouseButton};
use crate::render::RenderCommand;
use glam::{Vec2, Vec4};
use serde::{Serialize, Deserialize};
use anyhow::Result;

/// Number input widget with validation, formatting, and step controls
#[derive(Default)]
pub struct NumberInputWidget;

impl NumberInputWidget {
    pub fn new() -> Self {
        Self
    }
}

/// State for the number input widget
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct NumberInputState {
    /// Current numeric value
    pub value: f64,
    /// Text representation of the value (for editing)
    pub text_value: String,
    /// Whether the input is currently focused
    pub is_focused: bool,
    /// Whether the input is currently being edited
    pub is_editing: bool,
    /// Text cursor position
    pub cursor_position: usize,
    /// Text selection range
    pub selection: Option<(usize, usize)>,
    /// Whether the input is disabled
    pub is_disabled: bool,
    /// Validation error message
    pub error_message: Option<String>,
    /// Whether the value is valid
    pub is_valid: bool,
    /// Which button is currently being pressed
    pub pressed_button: Option<StepButton>,
    /// Button press start time for repeat functionality
    pub button_press_time: f64,
    /// Current step repeat rate
    pub step_repeat_rate: f64,
}

impl Default for NumberInputState {
    fn default() -> Self {
        Self {
            value: 0.0,
            text_value: "0".to_string(),
            is_focused: false,
            is_editing: false,
            cursor_position: 0,
            selection: None,
            is_disabled: false,
            error_message: None,
            is_valid: true,
            pressed_button: None,
            button_press_time: 0.0,
            step_repeat_rate: 0.0,
        }
    }
}

/// Step button types
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum StepButton {
    Up,
    Down,
}

/// Number formatting options
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct NumberFormat {
    /// Number of decimal places to show
    pub decimal_places: Option<u8>,
    /// Use thousands separator
    pub use_thousands_separator: bool,
    /// Thousands separator character
    pub thousands_separator: String,
    /// Decimal separator character
    pub decimal_separator: String,
    /// Currency symbol (if formatting as currency)
    pub currency_symbol: Option<String>,
    /// Percentage formatting
    pub is_percentage: bool,
    /// Scientific notation for very large/small numbers
    pub use_scientific: bool,
    /// Prefix string
    pub prefix: String,
    /// Suffix string
    pub suffix: String,
}

impl Default for NumberFormat {
    fn default() -> Self {
        Self {
            decimal_places: None,
            use_thousands_separator: false,
            thousands_separator: ",".to_string(),
            decimal_separator: ".".to_string(),
            currency_symbol: None,
            is_percentage: false,
            use_scientific: false,
            prefix: String::new(),
            suffix: String::new(),
        }
    }
}

/// Number validation rules
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct NumberValidation {
    /// Minimum allowed value
    pub min: Option<f64>,
    /// Maximum allowed value
    pub max: Option<f64>,
    /// Step size for increment/decrement
    pub step: f64,
    /// Only allow integer values
    pub integer_only: bool,
    /// Allow negative values
    pub allow_negative: bool,
    /// Custom validation function name
    pub custom_validator: Option<String>,
}

impl Default for NumberValidation {
    fn default() -> Self {
        Self {
            min: None,
            max: None,
            step: 1.0,
            integer_only: false,
            allow_negative: true,
            custom_validator: None,
        }
    }
}

/// Configuration for the number input widget
#[derive(Debug, Clone)]
pub struct NumberInputConfig {
    /// Background color
    pub background_color: Vec4,
    /// Text color
    pub text_color: Vec4,
    /// Border color
    pub border_color: Vec4,
    /// Focus border color
    pub focus_border_color: Vec4,
    /// Error border color
    pub error_border_color: Vec4,
    /// Disabled background color
    pub disabled_background_color: Vec4,
    /// Disabled text color
    pub disabled_text_color: Vec4,
    /// Selection background color
    pub selection_background_color: Vec4,
    /// Step button color
    pub button_color: Vec4,
    /// Step button hover color
    pub button_hover_color: Vec4,
    /// Font size
    pub font_size: f32,
    /// Border radius
    pub border_radius: f32,
    /// Padding
    pub padding: f32,
    /// Show step buttons (up/down arrows)
    pub show_step_buttons: bool,
    /// Step button width
    pub step_button_width: f32,
    /// Placeholder text
    pub placeholder: String,
    /// Number formatting options
    pub format: NumberFormat,
    /// Validation rules
    pub validation: NumberValidation,
    /// Auto-select text on focus
    pub select_on_focus: bool,
    /// Allow mouse wheel to change value
    pub mouse_wheel_enabled: bool,
    /// Wheel step multiplier
    pub wheel_step_multiplier: f64,
    /// Button repeat delay (milliseconds)
    pub button_repeat_delay: f64,
    /// Button repeat rate (milliseconds)
    pub button_repeat_rate: f64,
}

impl Default for NumberInputConfig {
    fn default() -> Self {
        Self {
            background_color: Vec4::new(1.0, 1.0, 1.0, 1.0),
            text_color: Vec4::new(0.2, 0.2, 0.2, 1.0),
            border_color: Vec4::new(0.7, 0.7, 0.7, 1.0),
            focus_border_color: Vec4::new(0.2, 0.6, 1.0, 1.0),
            error_border_color: Vec4::new(0.8, 0.2, 0.2, 1.0),
            disabled_background_color: Vec4::new(0.95, 0.95, 0.95, 1.0),
            disabled_text_color: Vec4::new(0.6, 0.6, 0.6, 1.0),
            selection_background_color: Vec4::new(0.7, 0.8, 1.0, 0.5),
            button_color: Vec4::new(0.9, 0.9, 0.9, 1.0),
            button_hover_color: Vec4::new(0.8, 0.8, 0.8, 1.0),
            font_size: 14.0,
            border_radius: 4.0,
            padding: 8.0,
            show_step_buttons: true,
            step_button_width: 20.0,
            placeholder: "Enter number...".to_string(),
            format: NumberFormat::default(),
            validation: NumberValidation::default(),
            select_on_focus: false,
            mouse_wheel_enabled: true,
            wheel_step_multiplier: 1.0,
            button_repeat_delay: 500.0,
            button_repeat_rate: 100.0,
        }
    }
}

impl From<&PropertyBag> for NumberInputConfig {
    fn from(props: &PropertyBag) -> Self {
        let mut config = Self::default();
        
        if let Some(size) = props.get_f32("font_size") {
            config.font_size = size;
        }
        
        if let Some(radius) = props.get_f32("border_radius") {
            config.border_radius = radius;
        }
        
        if let Some(padding) = props.get_f32("padding") {
            config.padding = padding;
        }
        
        if let Some(placeholder) = props.get_string("placeholder") {
            config.placeholder = placeholder;
        }
        
        if let Some(show_buttons) = props.get_bool("show_step_buttons") {
            config.show_step_buttons = show_buttons;
        }
        
        if let Some(select_on_focus) = props.get_bool("select_on_focus") {
            config.select_on_focus = select_on_focus;
        }
        
        if let Some(mouse_wheel) = props.get_bool("mouse_wheel_enabled") {
            config.mouse_wheel_enabled = mouse_wheel;
        }
        
        // Validation
        if let Some(min) = props.get_f32("min") {
            config.validation.min = Some(min as f64);
        }
        
        if let Some(max) = props.get_f32("max") {
            config.validation.max = Some(max as f64);
        }
        
        if let Some(step) = props.get_f32("step") {
            config.validation.step = step as f64;
        }
        
        if let Some(integer_only) = props.get_bool("integer_only") {
            config.validation.integer_only = integer_only;
        }
        
        if let Some(allow_negative) = props.get_bool("allow_negative") {
            config.validation.allow_negative = allow_negative;
        }
        
        // Formatting
        if let Some(decimal_places) = props.get_i32("decimal_places") {
            config.format.decimal_places = Some(decimal_places.max(0) as u8);
        }
        
        if let Some(use_thousands) = props.get_bool("use_thousands_separator") {
            config.format.use_thousands_separator = use_thousands;
        }
        
        if let Some(currency) = props.get_string("currency_symbol") {
            config.format.currency_symbol = Some(currency);
        }
        
        if let Some(is_percentage) = props.get_bool("is_percentage") {
            config.format.is_percentage = is_percentage;
        }
        
        if let Some(prefix) = props.get_string("prefix") {
            config.format.prefix = prefix;
        }
        
        if let Some(suffix) = props.get_string("suffix") {
            config.format.suffix = suffix;
        }
        
        config
    }
}

impl KryonWidget for NumberInputWidget {
    const WIDGET_NAME: &'static str = "number-input";
    type State = NumberInputState;
    type Config = NumberInputConfig;
    
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
        
        let input_width = if config.show_step_buttons {
            layout.size.x - config.step_button_width
        } else {
            layout.size.x
        };
        
        // Input field background
        let bg_color = if state.is_disabled {
            config.disabled_background_color
        } else {
            config.background_color
        };
        
        let border_color = if !state.is_valid {
            config.error_border_color
        } else if state.is_focused {
            config.focus_border_color
        } else {
            config.border_color
        };
        
        commands.push(RenderCommand::DrawRect {
            position: layout.position,
            size: Vec2::new(input_width, layout.size.y),
            color: bg_color,
            border_radius: config.border_radius,
            border_width: 1.0,
            border_color,
        });
        
        // Text content
        let display_text = if state.is_editing {
            &state.text_value
        } else {
            &self.format_number(state.value, config)
        };
        
        let text_color = if state.is_disabled {
            config.disabled_text_color
        } else if display_text.is_empty() {
            config.disabled_text_color // Placeholder color
        } else {
            config.text_color
        };
        
        let final_text = if display_text.is_empty() && !state.is_focused {
            config.placeholder.clone()
        } else {
            display_text.clone()
        };
        
        // Text selection background
        if let Some((start, end)) = state.selection {
            if state.is_focused && start != end {
                let char_width = 8.0; // Approximate character width
                let selection_start = layout.position.x + config.padding + (start as f32 * char_width);
                let selection_width = (end - start) as f32 * char_width;
                
                commands.push(RenderCommand::DrawRect {
                    position: Vec2::new(selection_start, layout.position.y + 2.0),
                    size: Vec2::new(selection_width, layout.size.y - 4.0),
                    color: config.selection_background_color,
                    border_radius: 0.0,
                    border_width: 0.0,
                    border_color: Vec4::ZERO,
                });
            }
        }
        
        // Text
        commands.push(RenderCommand::DrawText {
            text: final_text,
            position: layout.position + Vec2::new(config.padding, (layout.size.y - config.font_size) / 2.0),
            font_size: config.font_size,
            color: text_color,
            max_width: Some(input_width - 2.0 * config.padding),
        });
        
        // Cursor (if focused and editing)
        if state.is_focused && state.is_editing {
            let char_width = 8.0; // Approximate character width
            let cursor_x = layout.position.x + config.padding + (state.cursor_position as f32 * char_width);
            
            commands.push(RenderCommand::DrawRect {
                position: Vec2::new(cursor_x, layout.position.y + 4.0),
                size: Vec2::new(1.0, layout.size.y - 8.0),
                color: config.text_color,
                border_radius: 0.0,
                border_width: 0.0,
                border_color: Vec4::ZERO,
            });
        }
        
        // Step buttons
        if config.show_step_buttons && !state.is_disabled {
            self.render_step_buttons(&mut commands, state, config, layout, input_width)?;
        }
        
        // Error message
        if let Some(error) = &state.error_message {
            commands.push(RenderCommand::DrawText {
                text: error.clone(),
                position: layout.position + Vec2::new(0.0, layout.size.y + 4.0),
                font_size: config.font_size * 0.9,
                color: config.error_border_color,
                max_width: Some(layout.size.x),
            });
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
        if state.is_disabled {
            return Ok(EventResult::NotHandled);
        }
        
        match event {
            InputEvent::MousePress { position, button: MouseButton::Left } => {
                let input_width = if config.show_step_buttons {
                    layout.size.x - config.step_button_width
                } else {
                    layout.size.x
                };
                
                // Check if click is on input field
                if self.point_in_rect(*position, layout.position, Vec2::new(input_width, layout.size.y)) {
                    state.is_focused = true;
                    state.is_editing = true;
                    
                    if config.select_on_focus && !state.text_value.is_empty() {
                        state.selection = Some((0, state.text_value.len()));
                    } else {
                        // Position cursor based on click position
                        let char_width = 8.0;
                        let relative_x = position.x - layout.position.x - config.padding;
                        let cursor_pos = (relative_x / char_width).round() as usize;
                        state.cursor_position = cursor_pos.min(state.text_value.len());
                        state.selection = None;
                    }
                    
                    // Prepare text for editing
                    if !state.is_editing {
                        state.text_value = state.value.to_string();
                    }
                    
                    return Ok(EventResult::HandledWithRedraw);
                }
                
                // Check step buttons
                if config.show_step_buttons {
                    if let Some(button) = self.get_clicked_button(*position, layout, input_width) {
                        state.pressed_button = Some(button.clone());
                        state.button_press_time = 0.0; // Would use actual time
                        
                        match button {
                            StepButton::Up => self.increment_value(state, config),
                            StepButton::Down => self.decrement_value(state, config),
                        }
                        
                        return Ok(EventResult::HandledWithRedraw);
                    }
                }
                
                // Click outside - lose focus
                state.is_focused = false;
                state.is_editing = false;
                state.selection = None;
                self.commit_value(state, config);
                
                Ok(EventResult::HandledWithRedraw)
            }
            
            InputEvent::MouseRelease { .. } => {
                if state.pressed_button.is_some() {
                    state.pressed_button = None;
                    return Ok(EventResult::HandledWithRedraw);
                }
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::KeyPress { key, modifiers } => {
                if !state.is_focused {
                    return Ok(EventResult::NotHandled);
                }
                
                match key {
                    KeyCode::Enter => {
                        state.is_focused = false;
                        state.is_editing = false;
                        state.selection = None;
                        self.commit_value(state, config);
                        Ok(EventResult::HandledWithRedraw)
                    }
                    KeyCode::Escape => {
                        state.is_focused = false;
                        state.is_editing = false;
                        state.selection = None;
                        // Revert to original value
                        state.text_value = state.value.to_string();
                        Ok(EventResult::HandledWithRedraw)
                    }
                    KeyCode::Backspace => {
                        if state.is_editing {
                            self.handle_backspace(state);
                            return Ok(EventResult::HandledWithRedraw);
                        }
                        Ok(EventResult::NotHandled)
                    }
                    KeyCode::Delete => {
                        if state.is_editing {
                            self.handle_delete(state);
                            return Ok(EventResult::HandledWithRedraw);
                        }
                        Ok(EventResult::NotHandled)
                    }
                    KeyCode::ArrowLeft => {
                        if state.is_editing {
                            self.move_cursor_left(state, modifiers.shift);
                            return Ok(EventResult::HandledWithRedraw);
                        }
                        Ok(EventResult::NotHandled)
                    }
                    KeyCode::ArrowRight => {
                        if state.is_editing {
                            self.move_cursor_right(state, modifiers.shift);
                            return Ok(EventResult::HandledWithRedraw);
                        }
                        Ok(EventResult::NotHandled)
                    }
                    KeyCode::ArrowUp => {
                        self.increment_value(state, config);
                        Ok(EventResult::HandledWithRedraw)
                    }
                    KeyCode::ArrowDown => {
                        self.decrement_value(state, config);
                        Ok(EventResult::HandledWithRedraw)
                    }
                    KeyCode::Home => {
                        if state.is_editing {
                            if modifiers.shift {
                                state.selection = Some((0, state.cursor_position));
                            } else {
                                state.selection = None;
                            }
                            state.cursor_position = 0;
                            return Ok(EventResult::HandledWithRedraw);
                        }
                        Ok(EventResult::NotHandled)
                    }
                    KeyCode::End => {
                        if state.is_editing {
                            let end_pos = state.text_value.len();
                            if modifiers.shift {
                                state.selection = Some((state.cursor_position, end_pos));
                            } else {
                                state.selection = None;
                            }
                            state.cursor_position = end_pos;
                            return Ok(EventResult::HandledWithRedraw);
                        }
                        Ok(EventResult::NotHandled)
                    }
                    _ => {
                        if modifiers.ctrl {
                            match key {
                                KeyCode::A => {
                                    if state.is_editing && !state.text_value.is_empty() {
                                        state.selection = Some((0, state.text_value.len()));
                                        return Ok(EventResult::HandledWithRedraw);
                                    }
                                }
                                KeyCode::C => {
                                    // Copy to clipboard
                                    return Ok(EventResult::Handled);
                                }
                                KeyCode::V => {
                                    // Paste from clipboard (would need clipboard access)
                                    return Ok(EventResult::HandledWithRedraw);
                                }
                                KeyCode::X => {
                                    // Cut to clipboard
                                    if let Some((start, end)) = state.selection {
                                        state.text_value.drain(start..end);
                                        state.cursor_position = start;
                                        state.selection = None;
                                        return Ok(EventResult::HandledWithRedraw);
                                    }
                                }
                                _ => {}
                            }
                        }
                        Ok(EventResult::NotHandled)
                    }
                }
            }
            
            InputEvent::TextInput { text } => {
                if state.is_focused && state.is_editing {
                    self.insert_text(state, config, text);
                    return Ok(EventResult::HandledWithRedraw);
                }
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::Scroll { delta, position } => {
                if config.mouse_wheel_enabled && 
                   self.point_in_rect(*position, layout.position, layout.size) {
                    let step = config.validation.step * config.wheel_step_multiplier;
                    if delta.y > 0.0 {
                        self.change_value(state, config, step);
                    } else if delta.y < 0.0 {
                        self.change_value(state, config, -step);
                    }
                    return Ok(EventResult::HandledWithRedraw);
                }
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::Focus => {
                state.is_focused = true;
                if config.select_on_focus && !state.text_value.is_empty() {
                    state.selection = Some((0, state.text_value.len()));
                }
                Ok(EventResult::HandledWithRedraw)
            }
            
            InputEvent::Blur => {
                state.is_focused = false;
                state.is_editing = false;
                state.selection = None;
                self.commit_value(state, config);
                Ok(EventResult::HandledWithRedraw)
            }
            
            _ => Ok(EventResult::NotHandled),
        }
    }
    
    fn can_focus(&self) -> bool {
        true
    }
    
    fn layout_constraints(&self, config: &Self::Config) -> Option<LayoutConstraints> {
        Some(LayoutConstraints {
            min_width: Some(80.0),
            max_width: None,
            min_height: Some(32.0),
            max_height: None,
            aspect_ratio: None,
        })
    }
}

impl NumberInputWidget {
    fn render_step_buttons(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &NumberInputState,
        config: &NumberInputConfig,
        layout: &LayoutResult,
        input_width: f32,
    ) -> Result<()> {
        let button_x = layout.position.x + input_width;
        let button_width = config.step_button_width;
        let button_height = layout.size.y / 2.0;
        
        // Up button
        let up_pressed = state.pressed_button == Some(StepButton::Up);
        let up_color = if up_pressed {
            config.button_hover_color
        } else {
            config.button_color
        };
        
        commands.push(RenderCommand::DrawRect {
            position: Vec2::new(button_x, layout.position.y),
            size: Vec2::new(button_width, button_height),
            color: up_color,
            border_radius: 0.0,
            border_width: 1.0,
            border_color: config.border_color,
        });
        
        commands.push(RenderCommand::DrawText {
            text: "▲".to_string(),
            position: Vec2::new(button_x + button_width / 2.0 - 4.0, layout.position.y + button_height / 2.0 - 6.0),
            font_size: 10.0,
            color: config.text_color,
            max_width: None,
        });
        
        // Down button
        let down_pressed = state.pressed_button == Some(StepButton::Down);
        let down_color = if down_pressed {
            config.button_hover_color
        } else {
            config.button_color
        };
        
        commands.push(RenderCommand::DrawRect {
            position: Vec2::new(button_x, layout.position.y + button_height),
            size: Vec2::new(button_width, button_height),
            color: down_color,
            border_radius: 0.0,
            border_width: 1.0,
            border_color: config.border_color,
        });
        
        commands.push(RenderCommand::DrawText {
            text: "▼".to_string(),
            position: Vec2::new(button_x + button_width / 2.0 - 4.0, layout.position.y + button_height + button_height / 2.0 - 6.0),
            font_size: 10.0,
            color: config.text_color,
            max_width: None,
        });
        
        Ok(())
    }
    
    fn get_clicked_button(&self, position: Vec2, layout: &LayoutResult, input_width: f32) -> Option<StepButton> {
        let button_x = layout.position.x + input_width;
        let button_width = 20.0; // config.step_button_width
        let button_height = layout.size.y / 2.0;
        
        if position.x >= button_x && position.x <= button_x + button_width {
            if position.y >= layout.position.y && position.y <= layout.position.y + button_height {
                Some(StepButton::Up)
            } else if position.y >= layout.position.y + button_height && 
                     position.y <= layout.position.y + layout.size.y {
                Some(StepButton::Down)
            } else {
                None
            }
        } else {
            None
        }
    }
    
    fn format_number(&self, value: f64, config: &NumberInputConfig) -> String {
        let mut result = String::new();
        
        // Add prefix
        result.push_str(&config.format.prefix);
        
        // Add currency symbol
        if let Some(currency) = &config.format.currency_symbol {
            result.push_str(currency);
        }
        
        // Format the number
        let mut number_str = if config.format.is_percentage {
            format!("{}", value * 100.0)
        } else if config.validation.integer_only {
            format!("{}", value as i64)
        } else if let Some(decimal_places) = config.format.decimal_places {
            format!("{:.prec$}", value, prec = decimal_places as usize)
        } else {
            format!("{}", value)
        };
        
        // Add thousands separator
        if config.format.use_thousands_separator && value.abs() >= 1000.0 {
            number_str = self.add_thousands_separators(&number_str, &config.format);
        }
        
        result.push_str(&number_str);
        
        // Add percentage symbol
        if config.format.is_percentage {
            result.push('%');
        }
        
        // Add suffix
        result.push_str(&config.format.suffix);
        
        result
    }
    
    fn add_thousands_separators(&self, number_str: &str, format: &NumberFormat) -> String {
        let parts: Vec<&str> = number_str.split(&format.decimal_separator).collect();
        let integer_part = parts[0];
        let decimal_part = if parts.len() > 1 { Some(parts[1]) } else { None };
        
        // Add separators to integer part
        let mut result = String::new();
        let chars: Vec<char> = integer_part.chars().collect();
        
        // Handle negative sign
        let start_index = if chars.first() == Some(&'-') {
            result.push('-');
            1
        } else {
            0
        };
        
        for (i, &ch) in chars[start_index..].iter().enumerate() {
            if i > 0 && (chars.len() - start_index - i) % 3 == 0 {
                result.push_str(&format.thousands_separator);
            }
            result.push(ch);
        }
        
        // Add decimal part if present
        if let Some(decimal) = decimal_part {
            result.push_str(&format.decimal_separator);
            result.push_str(decimal);
        }
        
        result
    }
    
    fn validate_value(&self, value: f64, config: &NumberInputConfig) -> Option<String> {
        // Check minimum value
        if let Some(min) = config.validation.min {
            if value < min {
                return Some(format!("Value must be at least {}", min));
            }
        }
        
        // Check maximum value
        if let Some(max) = config.validation.max {
            if value > max {
                return Some(format!("Value must be at most {}", max));
            }
        }
        
        // Check integer constraint
        if config.validation.integer_only && value.fract() != 0.0 {
            return Some("Value must be a whole number".to_string());
        }
        
        // Check negative constraint
        if !config.validation.allow_negative && value < 0.0 {
            return Some("Negative values are not allowed".to_string());
        }
        
        None
    }
    
    fn parse_text_value(&self, text: &str, config: &NumberInputConfig) -> Result<f64, String> {
        // Remove prefix and suffix
        let mut clean_text = text.to_string();
        
        if !config.format.prefix.is_empty() {
            clean_text = clean_text.strip_prefix(&config.format.prefix)
                .unwrap_or(&clean_text).to_string();
        }
        
        if !config.format.suffix.is_empty() {
            clean_text = clean_text.strip_suffix(&config.format.suffix)
                .unwrap_or(&clean_text).to_string();
        }
        
        // Remove currency symbol
        if let Some(currency) = &config.format.currency_symbol {
            clean_text = clean_text.replace(currency, "");
        }
        
        // Handle percentage
        let is_percentage = clean_text.ends_with('%');
        if is_percentage {
            clean_text = clean_text.trim_end_matches('%').to_string();
        }
        
        // Remove thousands separators
        clean_text = clean_text.replace(&config.format.thousands_separator, "");
        
        // Replace decimal separator with standard dot
        if config.format.decimal_separator != "." {
            clean_text = clean_text.replace(&config.format.decimal_separator, ".");
        }
        
        // Parse the number
        let mut value = clean_text.trim().parse::<f64>()
            .map_err(|_| "Invalid number format".to_string())?;
        
        // Handle percentage
        if is_percentage || config.format.is_percentage {
            value /= 100.0;
        }
        
        Ok(value)
    }
    
    fn increment_value(&self, state: &mut NumberInputState, config: &NumberInputConfig) {
        self.change_value(state, config, config.validation.step);
    }
    
    fn decrement_value(&self, state: &mut NumberInputState, config: &NumberInputConfig) {
        self.change_value(state, config, -config.validation.step);
    }
    
    fn change_value(&self, state: &mut NumberInputState, config: &NumberInputConfig, delta: f64) {
        let new_value = state.value + delta;
        
        // Apply constraints
        let constrained_value = if let Some(min) = config.validation.min {
            new_value.max(min)
        } else {
            new_value
        };
        
        let constrained_value = if let Some(max) = config.validation.max {
            constrained_value.min(max)
        } else {
            constrained_value
        };
        
        // Round to step if integer only
        let final_value = if config.validation.integer_only {
            constrained_value.round()
        } else {
            constrained_value
        };
        
        if final_value != state.value {
            state.value = final_value;
            state.text_value = final_value.to_string();
            state.error_message = None;
            state.is_valid = true;
        }
    }
    
    fn commit_value(&self, state: &mut NumberInputState, config: &NumberInputConfig) {
        if state.text_value.is_empty() {
            state.value = 0.0;
            state.text_value = "0".to_string();
            state.error_message = None;
            state.is_valid = true;
            return;
        }
        
        match self.parse_text_value(&state.text_value, config) {
            Ok(parsed_value) => {
                if let Some(error) = self.validate_value(parsed_value, config) {
                    state.error_message = Some(error);
                    state.is_valid = false;
                } else {
                    state.value = parsed_value;
                    state.error_message = None;
                    state.is_valid = true;
                }
            }
            Err(error) => {
                state.error_message = Some(error);
                state.is_valid = false;
            }
        }
    }
    
    fn insert_text(&self, state: &mut NumberInputState, config: &NumberInputConfig, text: &str) {
        // Filter out invalid characters
        let filtered_text: String = text.chars().filter(|&c| {
            c.is_ascii_digit() || 
            c == '.' || c == ',' || 
            (c == '-' && config.validation.allow_negative) ||
            c == '%' ||
            config.format.prefix.contains(c) ||
            config.format.suffix.contains(c) ||
            config.format.currency_symbol.as_ref().map_or(false, |s| s.contains(c))
        }).collect();
        
        if filtered_text.is_empty() {
            return;
        }
        
        // Handle selection replacement
        if let Some((start, end)) = state.selection {
            state.text_value.drain(start..end);
            state.text_value.insert_str(start, &filtered_text);
            state.cursor_position = start + filtered_text.len();
            state.selection = None;
        } else {
            state.text_value.insert_str(state.cursor_position, &filtered_text);
            state.cursor_position += filtered_text.len();
        }
        
        // Validate as we type
        if let Ok(value) = self.parse_text_value(&state.text_value, config) {
            if self.validate_value(value, config).is_none() {
                state.error_message = None;
                state.is_valid = true;
            }
        }
    }
    
    fn handle_backspace(&self, state: &mut NumberInputState) {
        if let Some((start, end)) = state.selection {
            // Delete selection
            state.text_value.drain(start..end);
            state.cursor_position = start;
            state.selection = None;
        } else if state.cursor_position > 0 {
            // Delete character before cursor
            state.text_value.remove(state.cursor_position - 1);
            state.cursor_position -= 1;
        }
    }
    
    fn handle_delete(&self, state: &mut NumberInputState) {
        if let Some((start, end)) = state.selection {
            // Delete selection
            state.text_value.drain(start..end);
            state.cursor_position = start;
            state.selection = None;
        } else if state.cursor_position < state.text_value.len() {
            // Delete character after cursor
            state.text_value.remove(state.cursor_position);
        }
    }
    
    fn move_cursor_left(&self, state: &mut NumberInputState, extend_selection: bool) {
        if state.cursor_position > 0 {
            if extend_selection {
                if let Some((start, end)) = state.selection {
                    if state.cursor_position == end {
                        state.selection = Some((start, state.cursor_position - 1));
                    } else {
                        state.selection = Some((start.saturating_sub(1), end));
                    }
                } else {
                    state.selection = Some((state.cursor_position - 1, state.cursor_position));
                }
            } else {
                state.selection = None;
            }
            state.cursor_position -= 1;
        }
    }
    
    fn move_cursor_right(&self, state: &mut NumberInputState, extend_selection: bool) {
        if state.cursor_position < state.text_value.len() {
            if extend_selection {
                if let Some((start, end)) = state.selection {
                    if state.cursor_position == start {
                        state.selection = Some((state.cursor_position + 1, end));
                    } else {
                        state.selection = Some((start, (end + 1).min(state.text_value.len())));
                    }
                } else {
                    state.selection = Some((state.cursor_position, state.cursor_position + 1));
                }
            } else {
                state.selection = None;
            }
            state.cursor_position += 1;
        }
    }
    
    fn point_in_rect(&self, point: Vec2, rect_pos: Vec2, rect_size: Vec2) -> bool {
        point.x >= rect_pos.x &&
        point.x <= rect_pos.x + rect_size.x &&
        point.y >= rect_pos.y &&
        point.y <= rect_pos.y + rect_size.y
    }
}

// Implement the factory trait
crate::impl_widget_factory!(NumberInputWidget);

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_number_input_creation() {
        let widget = NumberInputWidget::new();
        let state = widget.create_state();
        
        assert_eq!(state.value, 0.0);
        assert_eq!(state.text_value, "0");
        assert!(!state.is_focused);
    }
    
    #[test]
    fn test_number_formatting() {
        let widget = NumberInputWidget::new();
        let mut config = NumberInputConfig::default();
        
        // Basic formatting
        assert_eq!(widget.format_number(1234.5, &config), "1234.5");
        
        // With decimal places
        config.format.decimal_places = Some(2);
        assert_eq!(widget.format_number(1234.5, &config), "1234.50");
        
        // With thousands separator
        config.format.use_thousands_separator = true;
        assert_eq!(widget.format_number(12345.67, &config), "12,345.67");
        
        // As percentage
        config.format.is_percentage = true;
        assert_eq!(widget.format_number(0.1234, &config), "12.34%");
        
        // With currency
        config.format.is_percentage = false;
        config.format.currency_symbol = Some("$".to_string());
        assert_eq!(widget.format_number(1234.56, &config), "$1,234.56");
    }
    
    #[test]
    fn test_number_parsing() {
        let widget = NumberInputWidget::new();
        let config = NumberInputConfig::default();
        
        assert_eq!(widget.parse_text_value("123.45", &config).unwrap(), 123.45);
        assert_eq!(widget.parse_text_value("-67.89", &config).unwrap(), -67.89);
        assert!(widget.parse_text_value("invalid", &config).is_err());
    }
    
    #[test]
    fn test_number_validation() {
        let widget = NumberInputWidget::new();
        let mut config = NumberInputConfig::default();
        config.validation.min = Some(0.0);
        config.validation.max = Some(100.0);
        
        assert!(widget.validate_value(50.0, &config).is_none());
        assert!(widget.validate_value(-10.0, &config).is_some());
        assert!(widget.validate_value(150.0, &config).is_some());
        
        config.validation.integer_only = true;
        assert!(widget.validate_value(50.0, &config).is_none());
        assert!(widget.validate_value(50.5, &config).is_some());
    }
    
    #[test]
    fn test_thousands_separator() {
        let widget = NumberInputWidget::new();
        let format = NumberFormat {
            use_thousands_separator: true,
            thousands_separator: ",".to_string(),
            decimal_separator: ".".to_string(),
            ..Default::default()
        };
        
        assert_eq!(widget.add_thousands_separators("1234567", &format), "1,234,567");
        assert_eq!(widget.add_thousands_separators("1234.56", &format), "1,234.56");
        assert_eq!(widget.add_thousands_separators("-1234567", &format), "-1,234,567");
    }
}