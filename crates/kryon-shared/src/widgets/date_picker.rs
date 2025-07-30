use super::{KryonWidget, EventResult, LayoutConstraints, PropertyBag};
use crate::elements::ElementId;
use crate::types::{InputEvent, LayoutResult, KeyCode, MouseButton};
use crate::render::RenderCommand;
use glam::{Vec2, Vec4};
use serde::{Serialize, Deserialize};
use anyhow::Result;
use std::collections::HashMap;

/// Date picker widget with calendar interface and flexible date formatting
#[derive(Default)]
pub struct DatePickerWidget;

impl DatePickerWidget {
    pub fn new() -> Self {
        Self
    }
}

/// State for the date picker widget
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct DatePickerState {
    /// Currently selected date (None if no date selected)
    pub selected_date: Option<DateValue>,
    /// Date being viewed in calendar (for navigation)
    pub view_date: DateValue,
    /// Whether the calendar popup is open
    pub is_open: bool,
    /// Current input mode (calendar vs text input)
    pub input_mode: DateInputMode,
    /// Text input value when in text mode
    pub text_input: String,
    /// Cursor position in text input
    pub cursor_position: usize,
    /// Whether the input is currently being edited
    pub is_editing: bool,
    /// Validation error message
    pub error_message: Option<String>,
    /// Highlighted date in calendar (for keyboard navigation)
    pub highlighted_date: Option<DateValue>,
    /// Available preset ranges (Today, Yesterday, This Week, etc.)
    pub preset_ranges: Vec<DatePreset>,
    /// Whether to show preset ranges
    pub show_presets: bool,
    /// Time selection state (if time picking is enabled)
    pub time_state: Option<TimeState>,
}

impl Default for DatePickerState {
    fn default() -> Self {
        let today = DateValue::today();
        Self {
            selected_date: None,
            view_date: today.clone(),
            is_open: false,
            input_mode: DateInputMode::Calendar,
            text_input: String::new(),
            cursor_position: 0,
            is_editing: false,
            error_message: None,
            highlighted_date: Some(today),
            preset_ranges: DatePreset::default_presets(),
            show_presets: false,
            time_state: None,
        }
    }
}

/// Date value representation
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct DateValue {
    pub year: u32,
    pub month: u32,  // 1-12
    pub day: u32,    // 1-31
}

impl DateValue {
    pub fn new(year: u32, month: u32, day: u32) -> Option<Self> {
        if Self::is_valid_date(year, month, day) {
            Some(Self { year, month, day })
        } else {
            None
        }
    }
    
    pub fn today() -> Self {
        // In a real implementation, this would get the actual current date
        // For now, we'll use a placeholder
        Self { year: 2024, month: 1, day: 1 }
    }
    
    pub fn is_valid_date(year: u32, month: u32, day: u32) -> bool {
        if month == 0 || month > 12 || day == 0 {
            return false;
        }
        
        let days_in_month = match month {
            1 | 3 | 5 | 7 | 8 | 10 | 12 => 31,
            4 | 6 | 9 | 11 => 30,
            2 => if Self::is_leap_year(year) { 29 } else { 28 },
            _ => return false,
        };
        
        day <= days_in_month
    }
    
    pub fn is_leap_year(year: u32) -> bool {
        (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)
    }
    
    pub fn days_in_month(&self) -> u32 {
        match self.month {
            1 | 3 | 5 | 7 | 8 | 10 | 12 => 31,
            4 | 6 | 9 | 11 => 30,
            2 => if Self::is_leap_year(self.year) { 29 } else { 28 },
            _ => 30,
        }
    }
    
    pub fn first_day_of_month(&self) -> DateValue {
        DateValue { year: self.year, month: self.month, day: 1 }
    }
    
    pub fn last_day_of_month(&self) -> DateValue {
        DateValue { year: self.year, month: self.month, day: self.days_in_month() }
    }
    
    pub fn add_months(&self, months: i32) -> DateValue {
        let total_months = (self.year as i32 * 12 + self.month as i32 - 1) + months;
        let new_year = (total_months / 12) as u32;
        let new_month = (total_months % 12 + 1) as u32;
        
        // Adjust day if it doesn't exist in the new month
        let new_date = DateValue { year: new_year, month: new_month, day: self.day };
        let days_in_new_month = new_date.days_in_month();
        
        DateValue {
            year: new_year,
            month: new_month,
            day: self.day.min(days_in_new_month),
        }
    }
    
    pub fn add_days(&self, days: i32) -> DateValue {
        // Simplified implementation - in practice would use proper date arithmetic
        let mut result = self.clone();
        
        if days > 0 {
            for _ in 0..days {
                result = result.next_day();
            }
        } else {
            for _ in 0..(-days) {
                result = result.previous_day();
            }
        }
        
        result
    }
    
    pub fn next_day(&self) -> DateValue {
        if self.day < self.days_in_month() {
            DateValue { year: self.year, month: self.month, day: self.day + 1 }
        } else if self.month < 12 {
            DateValue { year: self.year, month: self.month + 1, day: 1 }
        } else {
            DateValue { year: self.year + 1, month: 1, day: 1 }
        }
    }
    
    pub fn previous_day(&self) -> DateValue {
        if self.day > 1 {
            DateValue { year: self.year, month: self.month, day: self.day - 1 }
        } else if self.month > 1 {
            let prev_month = DateValue { year: self.year, month: self.month - 1, day: 1 };
            DateValue { year: self.year, month: self.month - 1, day: prev_month.days_in_month() }
        } else {
            DateValue { year: self.year - 1, month: 12, day: 31 }
        }
    }
    
    pub fn weekday(&self) -> u32 {
        // Simplified Zeller's congruence - returns 0=Sunday, 1=Monday, etc.
        let mut year = self.year;
        let mut month = self.month;
        
        if month < 3 {
            month += 12;
            year -= 1;
        }
        
        let k = year % 100;
        let j = year / 100;
        
        let h = (self.day + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
        (h + 5) % 7 // Convert to 0=Sunday format
    }
    
    pub fn format(&self, format: &DateFormat) -> String {
        match format {
            DateFormat::YearMonthDay => format!("{:04}-{:02}-{:02}", self.year, self.month, self.day),
            DateFormat::MonthDayYear => format!("{:02}/{:02}/{:04}", self.month, self.day, self.year),
            DateFormat::DayMonthYear => format!("{:02}/{:02}/{:04}", self.day, self.month, self.year),
            DateFormat::Long => {
                let month_name = Self::month_name(self.month);
                format!("{} {}, {}", month_name, self.day, self.year)
            }
            DateFormat::Short => {
                let month_name = Self::month_abbrev(self.month);
                format!("{} {}, {}", month_name, self.day, self.year)
            }
            DateFormat::Custom(pattern) => {
                // Simplified custom format - in practice would support more patterns
                pattern
                    .replace("YYYY", &format!("{:04}", self.year))
                    .replace("MM", &format!("{:02}", self.month))
                    .replace("DD", &format!("{:02}", self.day))
            }
        }
    }
    
    pub fn parse(text: &str, format: &DateFormat) -> Option<Self> {
        // Simplified parsing - in practice would handle various formats properly
        match format {
            DateFormat::YearMonthDay => {
                let parts: Vec<&str> = text.split('-').collect();
                if parts.len() == 3 {
                    if let (Ok(year), Ok(month), Ok(day)) = (
                        parts[0].parse::<u32>(),
                        parts[1].parse::<u32>(),
                        parts[2].parse::<u32>(),
                    ) {
                        return Self::new(year, month, day);
                    }
                }
            }
            DateFormat::MonthDayYear | DateFormat::DayMonthYear => {
                let parts: Vec<&str> = text.split('/').collect();
                if parts.len() == 3 {
                    if let (Ok(first), Ok(second), Ok(year)) = (
                        parts[0].parse::<u32>(),
                        parts[1].parse::<u32>(),
                        parts[2].parse::<u32>(),
                    ) {
                        let (month, day) = if matches!(format, DateFormat::MonthDayYear) {
                            (first, second)
                        } else {
                            (second, first)
                        };
                        return Self::new(year, month, day);
                    }
                }
            }
            _ => {}
        }
        None
    }
    
    fn month_name(month: u32) -> &'static str {
        match month {
            1 => "January", 2 => "February", 3 => "March", 4 => "April",
            5 => "May", 6 => "June", 7 => "July", 8 => "August",
            9 => "September", 10 => "October", 11 => "November", 12 => "December",
            _ => "Unknown",
        }
    }
    
    fn month_abbrev(month: u32) -> &'static str {
        match month {
            1 => "Jan", 2 => "Feb", 3 => "Mar", 4 => "Apr",
            5 => "May", 6 => "Jun", 7 => "Jul", 8 => "Aug",
            9 => "Sep", 10 => "Oct", 11 => "Nov", 12 => "Dec",
            _ => "Unknown",
        }
    }
}

/// Date input modes
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum DateInputMode {
    Calendar,
    TextInput,
    Both,
}

/// Date format options
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum DateFormat {
    YearMonthDay,    // 2024-01-15
    MonthDayYear,    // 01/15/2024
    DayMonthYear,    // 15/01/2024
    Long,            // January 15, 2024
    Short,           // Jan 15, 2024
    Custom(String),  // Custom format pattern
}

/// Time selection state (for datetime pickers)
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct TimeState {
    pub hour: u32,    // 0-23
    pub minute: u32,  // 0-59
    pub second: u32,  // 0-59
    pub format_24h: bool,
}

impl Default for TimeState {
    fn default() -> Self {
        Self {
            hour: 12,
            minute: 0,
            second: 0,
            format_24h: false,
        }
    }
}

/// Preset date ranges
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct DatePreset {
    pub label: String,
    pub date: DateValue,
}

impl DatePreset {
    pub fn default_presets() -> Vec<Self> {
        let today = DateValue::today();
        vec![
            DatePreset { label: "Today".to_string(), date: today.clone() },
            DatePreset { label: "Yesterday".to_string(), date: today.add_days(-1) },
            DatePreset { label: "Tomorrow".to_string(), date: today.add_days(1) },
        ]
    }
}

/// Configuration for the date picker widget
#[derive(Debug, Clone)]
pub struct DatePickerConfig {
    /// Background color
    pub background_color: Vec4,
    /// Text color
    pub text_color: Vec4,
    /// Border color
    pub border_color: Vec4,
    /// Header background (calendar)
    pub header_background: Vec4,
    /// Selected date background
    pub selected_background: Vec4,
    /// Today's date background
    pub today_background: Vec4,
    /// Hover background
    pub hover_background: Vec4,
    /// Disabled date color
    pub disabled_color: Vec4,
    /// Font size
    pub font_size: f32,
    /// Border radius
    pub border_radius: f32,
    /// Date format for display and parsing
    pub date_format: DateFormat,
    /// Placeholder text
    pub placeholder: String,
    /// Minimum selectable date
    pub min_date: Option<DateValue>,
    /// Maximum selectable date
    pub max_date: Option<DateValue>,
    /// Disabled dates
    pub disabled_dates: Vec<DateValue>,
    /// Show week numbers
    pub show_week_numbers: bool,
    /// First day of week (0=Sunday, 1=Monday)
    pub first_day_of_week: u32,
    /// Enable time selection
    pub include_time: bool,
    /// Show preset date ranges
    pub show_presets: bool,
    /// Allow clearing the selection
    pub clearable: bool,
    /// Input mode
    pub input_mode: DateInputMode,
    /// Calendar popup width
    pub calendar_width: f32,
    /// Calendar popup height
    pub calendar_height: f32,
}

impl Default for DatePickerConfig {
    fn default() -> Self {
        Self {
            background_color: Vec4::new(1.0, 1.0, 1.0, 1.0),
            text_color: Vec4::new(0.2, 0.2, 0.2, 1.0),
            border_color: Vec4::new(0.7, 0.7, 0.7, 1.0),
            header_background: Vec4::new(0.95, 0.95, 0.95, 1.0),
            selected_background: Vec4::new(0.2, 0.6, 1.0, 1.0),
            today_background: Vec4::new(1.0, 0.9, 0.6, 1.0),
            hover_background: Vec4::new(0.9, 0.9, 1.0, 1.0),
            disabled_color: Vec4::new(0.6, 0.6, 0.6, 1.0),
            font_size: 14.0,
            border_radius: 4.0,
            date_format: DateFormat::YearMonthDay,
            placeholder: "Select a date...".to_string(),
            min_date: None,
            max_date: None,
            disabled_dates: Vec::new(),
            show_week_numbers: false,
            first_day_of_week: 0, // Sunday
            include_time: false,
            show_presets: false,
            clearable: true,
            input_mode: DateInputMode::Calendar,
            calendar_width: 280.0,
            calendar_height: 320.0,
        }
    }
}

impl From<&PropertyBag> for DatePickerConfig {
    fn from(props: &PropertyBag) -> Self {
        let mut config = Self::default();
        
        if let Some(size) = props.get_f32("font_size") {
            config.font_size = size;
        }
        
        if let Some(radius) = props.get_f32("border_radius") {
            config.border_radius = radius;
        }
        
        if let Some(placeholder) = props.get_string("placeholder") {
            config.placeholder = placeholder;
        }
        
        if let Some(show_week_numbers) = props.get_bool("show_week_numbers") {
            config.show_week_numbers = show_week_numbers;
        }
        
        if let Some(first_day) = props.get_i32("first_day_of_week") {
            config.first_day_of_week = first_day.max(0).min(6) as u32;
        }
        
        if let Some(include_time) = props.get_bool("include_time") {
            config.include_time = include_time;
        }
        
        if let Some(show_presets) = props.get_bool("show_presets") {
            config.show_presets = show_presets;
        }
        
        if let Some(clearable) = props.get_bool("clearable") {
            config.clearable = clearable;
        }
        
        if let Some(format_str) = props.get_string("date_format") {
            config.date_format = match format_str.as_str() {
                "YYYY-MM-DD" => DateFormat::YearMonthDay,
                "MM/DD/YYYY" => DateFormat::MonthDayYear,
                "DD/MM/YYYY" => DateFormat::DayMonthYear,
                "long" => DateFormat::Long,
                "short" => DateFormat::Short,
                custom => DateFormat::Custom(custom.to_string()),
            };
        }
        
        if let Some(input_mode) = props.get_string("input_mode") {
            config.input_mode = match input_mode.as_str() {
                "calendar" => DateInputMode::Calendar,
                "text" => DateInputMode::TextInput,
                "both" => DateInputMode::Both,
                _ => DateInputMode::Calendar,
            };
        }
        
        config
    }
}

impl KryonWidget for DatePickerWidget {
    const WIDGET_NAME: &'static str = "date-picker";
    type State = DatePickerState;
    type Config = DatePickerConfig;
    
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
        
        // Main input field
        commands.push(RenderCommand::DrawRect {
            position: layout.position,
            size: layout.size,
            color: config.background_color,
            border_radius: config.border_radius,
            border_width: 1.0,
            border_color: config.border_color,
        });
        
        // Display text
        let display_text = if let Some(date) = &state.selected_date {
            date.format(&config.date_format)
        } else {
            config.placeholder.clone()
        };
        
        let text_color = if state.selected_date.is_some() {
            config.text_color
        } else {
            config.disabled_color
        };
        
        commands.push(RenderCommand::DrawText {
            text: display_text,
            position: layout.position + Vec2::new(12.0, (layout.size.y - config.font_size) / 2.0),
            font_size: config.font_size,
            color: text_color,
            max_width: Some(layout.size.x - 50.0),
        });
        
        // Clear button
        if config.clearable && state.selected_date.is_some() {
            let clear_x = layout.position.x + layout.size.x - 40.0;
            let clear_y = layout.position.y + (layout.size.y - 16.0) / 2.0;
            
            commands.push(RenderCommand::DrawText {
                text: "×".to_string(),
                position: Vec2::new(clear_x, clear_y),
                font_size: 16.0,
                color: config.text_color,
                max_width: None,
            });
        }
        
        // Calendar icon
        let icon_x = layout.position.x + layout.size.x - 20.0;
        let icon_y = layout.position.y + (layout.size.y - 16.0) / 2.0;
        
        commands.push(RenderCommand::DrawText {
            text: "📅".to_string(),
            position: Vec2::new(icon_x, icon_y),
            font_size: 16.0,
            color: config.text_color,
            max_width: None,
        });
        
        // Calendar popup
        if state.is_open {
            self.render_calendar(&mut commands, state, config, layout)?;
        }
        
        // Error message
        if let Some(error) = &state.error_message {
            commands.push(RenderCommand::DrawText {
                text: error.clone(),
                position: layout.position + Vec2::new(0.0, layout.size.y + 4.0),
                font_size: config.font_size * 0.9,
                color: Vec4::new(0.8, 0.2, 0.2, 1.0),
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
        match event {
            InputEvent::MousePress { position, button: MouseButton::Left } => {
                // Check if click is on main input
                if self.point_in_rect(*position, layout.position, layout.size) {
                    // Check for clear button
                    if config.clearable && state.selected_date.is_some() {
                        let clear_x = layout.position.x + layout.size.x - 40.0;
                        let clear_rect = Vec2::new(20.0, layout.size.y);
                        if self.point_in_rect(*position, Vec2::new(clear_x, layout.position.y), clear_rect) {
                            state.selected_date = None;
                            state.error_message = None;
                            return Ok(EventResult::EmitEvent {
                                event_name: "date_cleared".to_string(),
                                data: serde_json::json!(null),
                            });
                        }
                    }
                    
                    // Toggle calendar
                    state.is_open = !state.is_open;
                    if state.is_open {
                        state.highlighted_date = state.selected_date.clone()
                            .or_else(|| Some(DateValue::today()));
                        state.view_date = state.highlighted_date.clone().unwrap_or_else(DateValue::today);
                    }
                    return Ok(EventResult::HandledWithRedraw);
                }
                
                // Check if click is on calendar
                if state.is_open {
                    if let Some(clicked_date) = self.get_clicked_date(state, config, layout, *position) {
                        return self.select_date(state, config, clicked_date);
                    } else if !self.point_in_calendar_popup(state, config, layout, *position) {
                        // Click outside calendar - close it
                        state.is_open = false;
                        return Ok(EventResult::HandledWithRedraw);
                    }
                }
                
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::KeyPress { key, .. } => {
                if state.is_open {
                    match key {
                        KeyCode::Escape => {
                            state.is_open = false;
                            Ok(EventResult::HandledWithRedraw)
                        }
                        KeyCode::Enter => {
                            if let Some(date) = &state.highlighted_date {
                                return self.select_date(state, config, date.clone());
                            }
                            Ok(EventResult::NotHandled)
                        }
                        KeyCode::ArrowLeft => {
                            if let Some(highlighted) = &state.highlighted_date {
                                state.highlighted_date = Some(highlighted.previous_day());
                                self.ensure_date_visible(state);
                            }
                            Ok(EventResult::HandledWithRedraw)
                        }
                        KeyCode::ArrowRight => {
                            if let Some(highlighted) = &state.highlighted_date {
                                state.highlighted_date = Some(highlighted.next_day());
                                self.ensure_date_visible(state);
                            }
                            Ok(EventResult::HandledWithRedraw)
                        }
                        KeyCode::ArrowUp => {
                            if let Some(highlighted) = &state.highlighted_date {
                                state.highlighted_date = Some(highlighted.add_days(-7));
                                self.ensure_date_visible(state);
                            }
                            Ok(EventResult::HandledWithRedraw)
                        }
                        KeyCode::ArrowDown => {
                            if let Some(highlighted) = &state.highlighted_date {
                                state.highlighted_date = Some(highlighted.add_days(7));
                                self.ensure_date_visible(state);
                            }
                            Ok(EventResult::HandledWithRedraw)
                        }
                        _ => Ok(EventResult::NotHandled),
                    }
                } else {
                    match key {
                        KeyCode::Enter | KeyCode::Space => {
                            state.is_open = true;
                            state.highlighted_date = state.selected_date.clone()
                                .or_else(|| Some(DateValue::today()));
                            Ok(EventResult::HandledWithRedraw)
                        }
                        _ => Ok(EventResult::NotHandled),
                    }
                }
            }
            
            _ => Ok(EventResult::NotHandled),
        }
    }
    
    fn can_focus(&self) -> bool {
        true
    }
    
    fn layout_constraints(&self, config: &Self::Config) -> Option<LayoutConstraints> {
        Some(LayoutConstraints {
            min_width: Some(150.0),
            max_width: None,
            min_height: Some(32.0),
            max_height: None,
            aspect_ratio: None,
        })
    }
}

impl DatePickerWidget {
    fn render_calendar(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &DatePickerState,
        config: &DatePickerConfig,
        layout: &LayoutResult,
    ) -> Result<()> {
        let calendar_x = layout.position.x;
        let calendar_y = layout.position.y + layout.size.y + 4.0;
        let calendar_pos = Vec2::new(calendar_x, calendar_y);
        let calendar_size = Vec2::new(config.calendar_width, config.calendar_height);
        
        // Calendar background
        commands.push(RenderCommand::DrawRect {
            position: calendar_pos,
            size: calendar_size,
            color: config.background_color,
            border_radius: config.border_radius,
            border_width: 1.0,
            border_color: config.border_color,
        });
        
        // Calendar header
        self.render_calendar_header(commands, state, config, calendar_pos)?;
        
        // Calendar grid
        self.render_calendar_grid(commands, state, config, calendar_pos)?;
        
        // Presets (if enabled)
        if config.show_presets {
            self.render_date_presets(commands, state, config, calendar_pos)?;
        }
        
        Ok(())
    }
    
    fn render_calendar_header(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &DatePickerState,
        config: &DatePickerConfig,
        calendar_pos: Vec2,
    ) -> Result<()> {
        let header_height = 40.0;
        
        // Header background
        commands.push(RenderCommand::DrawRect {
            position: calendar_pos,
            size: Vec2::new(config.calendar_width, header_height),
            color: config.header_background,
            border_radius: config.border_radius,
            border_width: 0.0,
            border_color: Vec4::ZERO,
        });
        
        // Previous month button
        commands.push(RenderCommand::DrawText {
            text: "‹".to_string(),
            position: calendar_pos + Vec2::new(10.0, 12.0),
            font_size: 20.0,
            color: config.text_color,
            max_width: None,
        });
        
        // Month/year display
        let month_year = format!("{} {}", 
            DateValue::month_name(state.view_date.month), 
            state.view_date.year
        );
        
        commands.push(RenderCommand::DrawText {
            text: month_year,
            position: calendar_pos + Vec2::new(config.calendar_width / 2.0 - 50.0, 12.0),
            font_size: config.font_size,
            color: config.text_color,
            max_width: None,
        });
        
        // Next month button
        commands.push(RenderCommand::DrawText {
            text: "›".to_string(),
            position: calendar_pos + Vec2::new(config.calendar_width - 25.0, 12.0),
            font_size: 20.0,
            color: config.text_color,
            max_width: None,
        });
        
        Ok(())
    }
    
    fn render_calendar_grid(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &DatePickerState,
        config: &DatePickerConfig,
        calendar_pos: Vec2,
    ) -> Result<()> {
        let grid_start_y = calendar_pos.y + 50.0;
        let cell_size = 32.0;
        let grid_width = 7.0 * cell_size;
        let grid_start_x = calendar_pos.x + (config.calendar_width - grid_width) / 2.0;
        
        // Day headers
        let day_names = ["S", "M", "T", "W", "T", "F", "S"];
        for (i, &day_name) in day_names.iter().enumerate() {
            let x = grid_start_x + (i as f32 * cell_size) + cell_size / 2.0 - 4.0;
            commands.push(RenderCommand::DrawText {
                text: day_name.to_string(),
                position: Vec2::new(x, grid_start_y),
                font_size: config.font_size * 0.9,
                color: config.disabled_color,
                max_width: None,
            });
        }
        
        // Calendar days
        let first_day = state.view_date.first_day_of_month();
        let first_weekday = first_day.weekday();
        let days_in_month = state.view_date.days_in_month();
        let today = DateValue::today();
        
        // Previous month's trailing days
        let prev_month = state.view_date.add_months(-1);
        let prev_month_days = prev_month.days_in_month();
        
        for i in 0..first_weekday {
            let day = prev_month_days - first_weekday + i + 1;
            let date = DateValue::new(prev_month.year, prev_month.month, day).unwrap();
            let x = grid_start_x + (i as f32 * cell_size);
            let y = grid_start_y + 25.0;
            
            self.render_calendar_day(commands, state, config, &date, Vec2::new(x, y), cell_size, true)?;
        }
        
        // Current month days
        for day in 1..=days_in_month {
            let date = DateValue::new(state.view_date.year, state.view_date.month, day).unwrap();
            let total_days = first_weekday + day - 1;
            let week = total_days / 7;
            let weekday = total_days % 7;
            
            let x = grid_start_x + (weekday as f32 * cell_size);
            let y = grid_start_y + 25.0 + (week as f32 * cell_size);
            
            self.render_calendar_day(commands, state, config, &date, Vec2::new(x, y), cell_size, false)?;
        }
        
        // Next month's leading days
        let next_month = state.view_date.add_months(1);
        let total_cells = ((first_weekday + days_in_month - 1) / 7 + 1) * 7;
        let remaining_cells = total_cells - first_weekday - days_in_month;
        
        for day in 1..=remaining_cells {
            let date = DateValue::new(next_month.year, next_month.month, day).unwrap();
            let total_days = first_weekday + days_in_month + day - 1;
            let week = total_days / 7;
            let weekday = total_days % 7;
            
            let x = grid_start_x + (weekday as f32 * cell_size);
            let y = grid_start_y + 25.0 + (week as f32 * cell_size);
            
            self.render_calendar_day(commands, state, config, &date, Vec2::new(x, y), cell_size, true)?;
        }
        
        Ok(())
    }
    
    fn render_calendar_day(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &DatePickerState,
        config: &DatePickerConfig,
        date: &DateValue,
        position: Vec2,
        size: f32,
        is_other_month: bool,
    ) -> Result<()> {
        let today = DateValue::today();
        let is_today = date == &today;
        let is_selected = state.selected_date.as_ref() == Some(date);
        let is_highlighted = state.highlighted_date.as_ref() == Some(date);
        let is_disabled = self.is_date_disabled(date, config);
        
        // Day background
        let bg_color = if is_selected {
            config.selected_background
        } else if is_highlighted {
            config.hover_background
        } else if is_today {
            config.today_background
        } else {
            Vec4::ZERO // Transparent
        };
        
        if bg_color != Vec4::ZERO {
            commands.push(RenderCommand::DrawRect {
                position: position + Vec2::new(2.0, 2.0),
                size: Vec2::new(size - 4.0, size - 4.0),
                color: bg_color,
                border_radius: size / 2.0,
                border_width: 0.0,
                border_color: Vec4::ZERO,
            });
        }
        
        // Day text
        let text_color = if is_disabled {
            config.disabled_color
        } else if is_other_month {
            config.disabled_color * 0.6
        } else if is_selected {
            Vec4::new(1.0, 1.0, 1.0, 1.0) // White for selected
        } else {
            config.text_color
        };
        
        commands.push(RenderCommand::DrawText {
            text: date.day.to_string(),
            position: position + Vec2::new(size / 2.0 - 6.0, size / 2.0 - 6.0),
            font_size: config.font_size,
            color: text_color,
            max_width: None,
        });
        
        Ok(())
    }
    
    fn render_date_presets(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &DatePickerState,
        config: &DatePickerConfig,
        calendar_pos: Vec2,
    ) -> Result<()> {
        let presets_x = calendar_pos.x + 10.0;
        let presets_y = calendar_pos.y + config.calendar_height - 80.0;
        
        for (i, preset) in state.preset_ranges.iter().enumerate() {
            let y = presets_y + (i as f32 * 25.0);
            
            commands.push(RenderCommand::DrawText {
                text: preset.label.clone(),
                position: Vec2::new(presets_x, y),
                font_size: config.font_size * 0.9,
                color: config.text_color,
                max_width: None,
            });
        }
        
        Ok(())
    }
    
    fn select_date(
        &self,
        state: &mut DatePickerState,
        config: &DatePickerConfig,
        date: DateValue,
    ) -> Result<EventResult> {
        if self.is_date_disabled(&date, config) {
            return Ok(EventResult::NotHandled);
        }
        
        state.selected_date = Some(date.clone());
        state.is_open = false;
        state.error_message = None;
        
        Ok(EventResult::EmitEvent {
            event_name: "date_selected".to_string(),
            data: serde_json::json!({
                "date": date.format(&config.date_format),
                "year": date.year,
                "month": date.month,
                "day": date.day
            }),
        })
    }
    
    fn get_clicked_date(
        &self,
        state: &DatePickerState,
        config: &DatePickerConfig,
        layout: &LayoutResult,
        position: Vec2,
    ) -> Option<DateValue> {
        let calendar_pos = Vec2::new(layout.position.x, layout.position.y + layout.size.y + 4.0);
        let grid_start_y = calendar_pos.y + 75.0;
        let cell_size = 32.0;
        let grid_width = 7.0 * cell_size;
        let grid_start_x = calendar_pos.x + (config.calendar_width - grid_width) / 2.0;
        
        if position.x < grid_start_x || position.x > grid_start_x + grid_width {
            return None;
        }
        
        if position.y < grid_start_y {
            return None;
        }
        
        let col = ((position.x - grid_start_x) / cell_size) as u32;
        let row = ((position.y - grid_start_y) / cell_size) as u32;
        
        if col >= 7 {
            return None;
        }
        
        // Calculate the actual date based on grid position
        let first_day = state.view_date.first_day_of_month();
        let first_weekday = first_day.weekday();
        let cell_index = row * 7 + col;
        
        if cell_index < first_weekday {
            // Previous month
            let prev_month = state.view_date.add_months(-1);
            let prev_month_days = prev_month.days_in_month();
            let day = prev_month_days - first_weekday + cell_index + 1;
            DateValue::new(prev_month.year, prev_month.month, day)
        } else {
            let day_in_month = cell_index - first_weekday + 1;
            if day_in_month <= state.view_date.days_in_month() {
                // Current month
                DateValue::new(state.view_date.year, state.view_date.month, day_in_month)
            } else {
                // Next month
                let next_month = state.view_date.add_months(1);
                let day = day_in_month - state.view_date.days_in_month();
                DateValue::new(next_month.year, next_month.month, day)
            }
        }
    }
    
    fn point_in_calendar_popup(
        &self,
        state: &DatePickerState,
        config: &DatePickerConfig,
        layout: &LayoutResult,
        position: Vec2,
    ) -> bool {
        let calendar_pos = Vec2::new(layout.position.x, layout.position.y + layout.size.y + 4.0);
        let calendar_size = Vec2::new(config.calendar_width, config.calendar_height);
        
        self.point_in_rect(position, calendar_pos, calendar_size)
    }
    
    fn is_date_disabled(&self, date: &DateValue, config: &DatePickerConfig) -> bool {
        if let Some(min_date) = &config.min_date {
            if date < min_date {
                return true;
            }
        }
        
        if let Some(max_date) = &config.max_date {
            if date > max_date {
                return true;
            }
        }
        
        config.disabled_dates.contains(date)
    }
    
    fn ensure_date_visible(&self, state: &mut DatePickerState) {
        if let Some(highlighted) = &state.highlighted_date {
            if highlighted.year != state.view_date.year || highlighted.month != state.view_date.month {
                state.view_date = DateValue {
                    year: highlighted.year,
                    month: highlighted.month,
                    day: 1,
                };
            }
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
crate::impl_widget_factory!(DatePickerWidget);

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_date_picker_creation() {
        let widget = DatePickerWidget::new();
        let state = widget.create_state();
        
        assert_eq!(state.selected_date, None);
        assert!(!state.is_open);
    }
    
    #[test]
    fn test_date_value_creation() {
        let date = DateValue::new(2024, 2, 29).unwrap(); // Leap year
        assert_eq!(date.year, 2024);
        assert_eq!(date.month, 2);
        assert_eq!(date.day, 29);
        
        // Invalid date
        assert_eq!(DateValue::new(2023, 2, 29), None); // Not a leap year
        assert_eq!(DateValue::new(2024, 13, 1), None); // Invalid month
        assert_eq!(DateValue::new(2024, 1, 32), None); // Invalid day
    }
    
    #[test]
    fn test_date_formatting() {
        let date = DateValue::new(2024, 1, 15).unwrap();
        
        assert_eq!(date.format(&DateFormat::YearMonthDay), "2024-01-15");
        assert_eq!(date.format(&DateFormat::MonthDayYear), "01/15/2024");
        assert_eq!(date.format(&DateFormat::DayMonthYear), "15/01/2024");
        assert_eq!(date.format(&DateFormat::Long), "January 15, 2024");
        assert_eq!(date.format(&DateFormat::Short), "Jan 15, 2024");
    }
    
    #[test]
    fn test_date_parsing() {
        assert_eq!(
            DateValue::parse("2024-01-15", &DateFormat::YearMonthDay),
            DateValue::new(2024, 1, 15)
        );
        
        assert_eq!(
            DateValue::parse("01/15/2024", &DateFormat::MonthDayYear),
            DateValue::new(2024, 1, 15)
        );
        
        assert_eq!(
            DateValue::parse("15/01/2024", &DateFormat::DayMonthYear),
            DateValue::new(2024, 1, 15)
        );
    }
    
    #[test]
    fn test_date_arithmetic() {
        let date = DateValue::new(2024, 1, 15).unwrap();
        
        let next_day = date.next_day();
        assert_eq!(next_day, DateValue::new(2024, 1, 16).unwrap());
        
        let prev_day = date.previous_day();
        assert_eq!(prev_day, DateValue::new(2024, 1, 14).unwrap());
        
        let next_month = date.add_months(1);
        assert_eq!(next_month, DateValue::new(2024, 2, 15).unwrap());
        
        let next_week = date.add_days(7);
        assert_eq!(next_week, DateValue::new(2024, 1, 22).unwrap());
    }
    
    #[test]
    fn test_leap_year() {
        assert!(DateValue::is_leap_year(2024));
        assert!(!DateValue::is_leap_year(2023));
        assert!(DateValue::is_leap_year(2000));
        assert!(!DateValue::is_leap_year(1900));
    }
}