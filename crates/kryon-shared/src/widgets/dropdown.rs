use super::{KryonWidget, EventResult, LayoutConstraints, PropertyBag};
use crate::elements::ElementId;
use crate::types::{InputEvent, LayoutResult, KeyCode, MouseButton};
use crate::render::RenderCommand;
use glam::{Vec2, Vec4};
use serde::{Serialize, Deserialize};
use anyhow::Result;

/// Enhanced dropdown widget with search, multi-select, and async loading capabilities
#[derive(Default)]
pub struct DropdownWidget;

impl DropdownWidget {
    pub fn new() -> Self {
        Self
    }
}

/// State for the dropdown widget
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct DropdownState {
    /// Available options in the dropdown
    pub options: Vec<DropdownOption>,
    /// Currently selected indices (supports multi-select)
    pub selected_indices: Vec<usize>,
    /// Whether the dropdown is currently open
    pub is_open: bool,
    /// Index of the currently highlighted option (for keyboard navigation)
    pub highlighted_index: Option<usize>,
    /// Current search query for filtering options
    pub search_query: String,
    /// Filtered options based on search query
    pub filtered_indices: Vec<usize>,
    /// Whether the dropdown is in search mode
    pub is_searching: bool,
    /// Loading state for async option loading
    pub is_loading: bool,
    /// Error message if loading failed
    pub error_message: Option<String>,
    /// Scroll position for large option lists
    pub scroll_offset: f32,
    /// Maximum visible options before scrolling
    pub max_visible_options: usize,
}

impl Default for DropdownState {
    fn default() -> Self {
        Self {
            options: Vec::new(),
            selected_indices: Vec::new(),
            is_open: false,
            highlighted_index: None,
            search_query: String::new(),
            filtered_indices: Vec::new(),
            is_searching: false,
            is_loading: false,
            error_message: None,
            scroll_offset: 0.0,
            max_visible_options: 8,
        }
    }
}

/// Individual option in the dropdown
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct DropdownOption {
    /// Display text for the option
    pub label: String,
    /// Underlying value for the option
    pub value: String,
    /// Whether this option is disabled
    pub disabled: bool,
    /// Optional icon for the option
    pub icon: Option<String>,
    /// Optional group/category for the option
    pub group: Option<String>,
}

impl DropdownOption {
    pub fn new(label: impl Into<String>, value: impl Into<String>) -> Self {
        Self {
            label: label.into(),
            value: value.into(),
            disabled: false,
            icon: None,
            group: None,
        }
    }
    
    pub fn with_icon(mut self, icon: impl Into<String>) -> Self {
        self.icon = Some(icon.into());
        self
    }
    
    pub fn with_group(mut self, group: impl Into<String>) -> Self {
        self.group = Some(group.into());
        self
    }
    
    pub fn disabled(mut self) -> Self {
        self.disabled = true;
        self
    }
}

/// Configuration for the dropdown widget
#[derive(Debug, Clone)]
pub struct DropdownConfig {
    /// Background color of the dropdown button
    pub background_color: Vec4,
    /// Text color
    pub text_color: Vec4,
    /// Border color
    pub border_color: Vec4,
    /// Hover background color
    pub hover_color: Vec4,
    /// Selected option background color
    pub selected_color: Vec4,
    /// Disabled option text color
    pub disabled_color: Vec4,
    /// Font size for dropdown text
    pub font_size: f32,
    /// Border radius for rounded corners
    pub border_radius: f32,
    /// Maximum height of dropdown list
    pub max_height: f32,
    /// Enable multi-select mode
    pub multi_select: bool,
    /// Enable search functionality
    pub searchable: bool,
    /// Placeholder text when no option is selected
    pub placeholder: String,
    /// Search placeholder text
    pub search_placeholder: String,
    /// Enable async loading of options
    pub async_loading: bool,
    /// Close dropdown after selection (ignored in multi-select mode)
    pub close_on_select: bool,
    /// Show option icons
    pub show_icons: bool,
    /// Group options by category
    pub group_options: bool,
    /// Allow clearing selection
    pub clearable: bool,
}

impl Default for DropdownConfig {
    fn default() -> Self {
        Self {
            background_color: Vec4::new(1.0, 1.0, 1.0, 1.0), // White
            text_color: Vec4::new(0.2, 0.2, 0.2, 1.0),       // Dark gray
            border_color: Vec4::new(0.7, 0.7, 0.7, 1.0),     // Light gray
            hover_color: Vec4::new(0.95, 0.95, 1.0, 1.0),    // Light blue
            selected_color: Vec4::new(0.85, 0.85, 1.0, 1.0), // Darker blue
            disabled_color: Vec4::new(0.6, 0.6, 0.6, 1.0),   // Gray
            font_size: 14.0,
            border_radius: 4.0,
            max_height: 200.0,
            multi_select: false,
            searchable: false,
            placeholder: "Select an option...".to_string(),
            search_placeholder: "Search options...".to_string(),
            async_loading: false,
            close_on_select: true,
            show_icons: false,
            group_options: false,
            clearable: false,
        }
    }
}

impl From<&PropertyBag> for DropdownConfig {
    fn from(props: &PropertyBag) -> Self {
        let mut config = Self::default();
        
        if let Some(bg) = props.get_f32("background_color") {
            config.background_color = Vec4::splat(bg);
        }
        
        if let Some(text) = props.get_f32("text_color") {
            config.text_color = Vec4::splat(text);
        }
        
        if let Some(size) = props.get_f32("font_size") {
            config.font_size = size;
        }
        
        if let Some(radius) = props.get_f32("border_radius") {
            config.border_radius = radius;
        }
        
        if let Some(height) = props.get_f32("max_height") {
            config.max_height = height;
        }
        
        if let Some(multi) = props.get_bool("multi_select") {
            config.multi_select = multi;
        }
        
        if let Some(searchable) = props.get_bool("searchable") {
            config.searchable = searchable;
        }
        
        if let Some(placeholder) = props.get_string("placeholder") {
            config.placeholder = placeholder;
        }
        
        if let Some(search_placeholder) = props.get_string("search_placeholder") {
            config.search_placeholder = search_placeholder;
        }
        
        if let Some(async_loading) = props.get_bool("async_loading") {
            config.async_loading = async_loading;
        }
        
        if let Some(close_on_select) = props.get_bool("close_on_select") {
            config.close_on_select = close_on_select;
        }
        
        if let Some(show_icons) = props.get_bool("show_icons") {
            config.show_icons = show_icons;
        }
        
        if let Some(clearable) = props.get_bool("clearable") {
            config.clearable = clearable;
        }
        
        config
    }
}

impl KryonWidget for DropdownWidget {
    const WIDGET_NAME: &'static str = "dropdown";
    type State = DropdownState;
    type Config = DropdownConfig;
    
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
        
        // Main dropdown button background
        commands.push(RenderCommand::DrawRect {
            position: layout.position,
            size: layout.size,
            color: config.background_color,
            border_radius: config.border_radius,
            border_width: 1.0,
            border_color: config.border_color,
        });
        
        // Selected text or placeholder
        let display_text = if state.selected_indices.is_empty() {
            config.placeholder.clone()
        } else if config.multi_select {
            if state.selected_indices.len() == 1 {
                state.options[state.selected_indices[0]].label.clone()
            } else {
                format!("{} items selected", state.selected_indices.len())
            }
        } else {
            state.options[state.selected_indices[0]].label.clone()
        };
        
        let text_color = if state.selected_indices.is_empty() {
            config.disabled_color
        } else {
            config.text_color
        };
        
        commands.push(RenderCommand::DrawText {
            text: display_text,
            position: layout.position + Vec2::new(12.0, (layout.size.y - config.font_size) / 2.0),
            font_size: config.font_size,
            color: text_color,
            max_width: Some(layout.size.x - 40.0),
        });
        
        // Clear button (if clearable and has selection)
        if config.clearable && !state.selected_indices.is_empty() {
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
        
        // Dropdown arrow
        let arrow_x = layout.position.x + layout.size.x - 20.0;
        let arrow_y = layout.position.y + layout.size.y / 2.0;
        let arrow_points = if state.is_open {
            // Up arrow
            vec![
                Vec2::new(arrow_x - 4.0, arrow_y + 2.0),
                Vec2::new(arrow_x + 4.0, arrow_y + 2.0),
                Vec2::new(arrow_x, arrow_y - 2.0),
            ]
        } else {
            // Down arrow
            vec![
                Vec2::new(arrow_x - 4.0, arrow_y - 2.0),
                Vec2::new(arrow_x + 4.0, arrow_y - 2.0),
                Vec2::new(arrow_x, arrow_y + 2.0),
            ]
        };
        
        commands.push(RenderCommand::DrawTriangle {
            points: arrow_points,
            color: config.text_color,
        });
        
        // Dropdown list (if open)
        if state.is_open {
            self.render_dropdown_list(&mut commands, state, config, layout)?;
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
                // Check if click is on main dropdown button
                if self.point_in_rect(*position, layout.position, layout.size) {
                    // Check for clear button click
                    if config.clearable && !state.selected_indices.is_empty() {
                        let clear_x = layout.position.x + layout.size.x - 40.0;
                        let clear_rect = Vec2::new(20.0, layout.size.y);
                        if self.point_in_rect(*position, Vec2::new(clear_x, layout.position.y), clear_rect) {
                            state.selected_indices.clear();
                            return Ok(EventResult::EmitEvent {
                                event_name: "selection_changed".to_string(),
                                data: serde_json::json!([]),
                            });
                        }
                    }
                    
                    // Toggle dropdown
                    state.is_open = !state.is_open;
                    if state.is_open {
                        self.filter_options(state);
                        state.highlighted_index = if state.selected_indices.is_empty() {
                            Some(0)
                        } else {
                            Some(state.selected_indices[0])
                        };
                    }
                    return Ok(EventResult::HandledWithRedraw);
                }
                
                // Check if click is on dropdown options
                if state.is_open {
                    if let Some(option_index) = self.get_clicked_option(state, config, layout, *position) {
                        return self.select_option(state, config, option_index);
                    } else {
                        // Click outside dropdown - close it
                        state.is_open = false;
                        return Ok(EventResult::HandledWithRedraw);
                    }
                }
                
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::MouseMove { position } => {
                if state.is_open {
                    let old_highlighted = state.highlighted_index;
                    state.highlighted_index = self.get_hovered_option(state, config, layout, *position);
                    if old_highlighted != state.highlighted_index {
                        return Ok(EventResult::HandledWithRedraw);
                    }
                }
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::KeyPress { key, .. } => {
                if !state.is_open {
                    match key {
                        KeyCode::ArrowDown | KeyCode::ArrowUp | KeyCode::Enter | KeyCode::Space => {
                            state.is_open = true;
                            self.filter_options(state);
                            state.highlighted_index = if state.selected_indices.is_empty() {
                                Some(0)
                            } else {
                                Some(state.selected_indices[0])
                            };
                            return Ok(EventResult::HandledWithRedraw);
                        }
                        _ => return Ok(EventResult::NotHandled),
                    }
                } else {
                    match key {
                        KeyCode::ArrowDown => {
                            self.move_highlight_down(state);
                            return Ok(EventResult::HandledWithRedraw);
                        }
                        KeyCode::ArrowUp => {
                            self.move_highlight_up(state);
                            return Ok(EventResult::HandledWithRedraw);
                        }
                        KeyCode::Enter => {
                            if let Some(index) = state.highlighted_index {
                                return self.select_option(state, config, index);
                            }
                            return Ok(EventResult::NotHandled);
                        }
                        KeyCode::Escape => {
                            state.is_open = false;
                            state.is_searching = false;
                            state.search_query.clear();
                            return Ok(EventResult::HandledWithRedraw);
                        }
                        _ => {
                            if config.searchable {
                                return self.handle_search_input(state, key);
                            }
                            return Ok(EventResult::NotHandled);
                        }
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
            min_width: Some(120.0),
            max_width: None,
            min_height: Some(32.0),
            max_height: None,
            aspect_ratio: None,
        })
    }
}

impl DropdownWidget {
    fn render_dropdown_list(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &DropdownState,
        config: &DropdownConfig,
        layout: &LayoutResult,
    ) -> Result<()> {
        let list_y = layout.position.y + layout.size.y + 2.0;
        let item_height = config.font_size + 12.0;
        let visible_options = state.max_visible_options.min(state.filtered_indices.len());
        let list_height = (visible_options as f32 * item_height).min(config.max_height);
        
        // Dropdown list background
        commands.push(RenderCommand::DrawRect {
            position: Vec2::new(layout.position.x, list_y),
            size: Vec2::new(layout.size.x, list_height),
            color: config.background_color,
            border_radius: config.border_radius,
            border_width: 1.0,
            border_color: config.border_color,
        });
        
        // Search box (if searchable)
        if config.searchable {
            let search_height = item_height;
            commands.push(RenderCommand::DrawRect {
                position: Vec2::new(layout.position.x + 2.0, list_y + 2.0),
                size: Vec2::new(layout.size.x - 4.0, search_height - 4.0),
                color: Vec4::new(0.98, 0.98, 0.98, 1.0),
                border_radius: 2.0,
                border_width: 1.0,
                border_color: config.border_color,
            });
            
            let search_text = if state.search_query.is_empty() {
                config.search_placeholder.clone()
            } else {
                state.search_query.clone()
            };
            
            let search_color = if state.search_query.is_empty() {
                config.disabled_color
            } else {
                config.text_color
            };
            
            commands.push(RenderCommand::DrawText {
                text: search_text,
                position: Vec2::new(layout.position.x + 8.0, list_y + 8.0),
                font_size: config.font_size,
                color: search_color,
                max_width: Some(layout.size.x - 16.0),
            });
        }
        
        // Option items
        let options_start_y = if config.searchable { list_y + item_height } else { list_y };
        let scroll_start = (state.scroll_offset / item_height) as usize;
        let scroll_end = (scroll_start + visible_options).min(state.filtered_indices.len());
        
        for (i, &option_index) in state.filtered_indices[scroll_start..scroll_end].iter().enumerate() {
            let option = &state.options[option_index];
            let item_y = options_start_y + (i as f32 * item_height);
            let is_selected = state.selected_indices.contains(&option_index);
            let is_highlighted = state.highlighted_index == Some(option_index);
            
            // Item background
            let bg_color = if is_highlighted {
                config.hover_color
            } else if is_selected {
                config.selected_color
            } else {
                config.background_color
            };
            
            commands.push(RenderCommand::DrawRect {
                position: Vec2::new(layout.position.x, item_y),
                size: Vec2::new(layout.size.x, item_height),
                color: bg_color,
                border_radius: 0.0,
                border_width: 0.0,
                border_color: Vec4::ZERO,
            });
            
            // Option icon (if enabled and present)
            let mut text_x = layout.position.x + 12.0;
            if config.show_icons {
                if let Some(icon) = &option.icon {
                    commands.push(RenderCommand::DrawText {
                        text: icon.clone(),
                        position: Vec2::new(text_x, item_y + 4.0),
                        font_size: config.font_size,
                        color: if option.disabled { config.disabled_color } else { config.text_color },
                        max_width: None,
                    });
                    text_x += 24.0;
                }
            }
            
            // Option text
            let text_color = if option.disabled {
                config.disabled_color
            } else {
                config.text_color
            };
            
            commands.push(RenderCommand::DrawText {
                text: option.label.clone(),
                position: Vec2::new(text_x, item_y + 6.0),
                font_size: config.font_size,
                color: text_color,
                max_width: Some(layout.size.x - text_x + layout.position.x - 12.0),
            });
            
            // Multi-select checkbox
            if config.multi_select {
                let checkbox_x = layout.position.x + layout.size.x - 24.0;
                let checkbox_y = item_y + 6.0;
                
                commands.push(RenderCommand::DrawRect {
                    position: Vec2::new(checkbox_x, checkbox_y),
                    size: Vec2::new(12.0, 12.0),
                    color: if is_selected { config.selected_color } else { config.background_color },
                    border_radius: 2.0,
                    border_width: 1.0,
                    border_color: config.border_color,
                });
                
                if is_selected {
                    commands.push(RenderCommand::DrawText {
                        text: "✓".to_string(),
                        position: Vec2::new(checkbox_x + 2.0, checkbox_y),
                        font_size: 10.0,
                        color: config.text_color,
                        max_width: None,
                    });
                }
            }
        }
        
        // Scrollbar (if needed)
        if state.filtered_indices.len() > visible_options {
            let scrollbar_x = layout.position.x + layout.size.x - 8.0;
            let scrollbar_height = list_height - if config.searchable { item_height } else { 0.0 };
            let scroll_ratio = state.scroll_offset / ((state.filtered_indices.len() - visible_options) as f32 * item_height);
            let thumb_height = (scrollbar_height * visible_options as f32 / state.filtered_indices.len() as f32).max(20.0);
            let thumb_y = options_start_y + (scrollbar_height - thumb_height) * scroll_ratio;
            
            // Scrollbar track
            commands.push(RenderCommand::DrawRect {
                position: Vec2::new(scrollbar_x, options_start_y),
                size: Vec2::new(6.0, scrollbar_height),
                color: Vec4::new(0.9, 0.9, 0.9, 1.0),
                border_radius: 3.0,
                border_width: 0.0,
                border_color: Vec4::ZERO,
            });
            
            // Scrollbar thumb
            commands.push(RenderCommand::DrawRect {
                position: Vec2::new(scrollbar_x + 1.0, thumb_y),
                size: Vec2::new(4.0, thumb_height),
                color: Vec4::new(0.6, 0.6, 0.6, 1.0),
                border_radius: 2.0,
                border_width: 0.0,
                border_color: Vec4::ZERO,
            });
        }
        
        Ok(())
    }
    
    fn filter_options(&self, state: &mut DropdownState) {
        if state.search_query.is_empty() {
            state.filtered_indices = (0..state.options.len()).collect();
        } else {
            let query = state.search_query.to_lowercase();
            state.filtered_indices = state
                .options
                .iter()
                .enumerate()
                .filter(|(_, option)| {
                    option.label.to_lowercase().contains(&query) ||
                    option.value.to_lowercase().contains(&query)
                })
                .map(|(i, _)| i)
                .collect();
        }
    }
    
    fn select_option(
        &self,
        state: &mut DropdownState,
        config: &DropdownConfig,
        option_index: usize,
    ) -> Result<EventResult> {
        if option_index >= state.options.len() || state.options[option_index].disabled {
            return Ok(EventResult::NotHandled);
        }
        
        if config.multi_select {
            if let Some(pos) = state.selected_indices.iter().position(|&x| x == option_index) {
                state.selected_indices.remove(pos);
            } else {
                state.selected_indices.push(option_index);
            }
        } else {
            state.selected_indices = vec![option_index];
            if config.close_on_select {
                state.is_open = false;
            }
        }
        
        let selected_values: Vec<String> = state
            .selected_indices
            .iter()
            .map(|&i| state.options[i].value.clone())
            .collect();
        
        Ok(EventResult::EmitEvent {
            event_name: "selection_changed".to_string(),
            data: serde_json::json!(selected_values),
        })
    }
    
    fn move_highlight_down(&self, state: &mut DropdownState) {
        if state.filtered_indices.is_empty() {
            return;
        }
        
        let current_pos = state.highlighted_index
            .and_then(|idx| state.filtered_indices.iter().position(|&x| x == idx))
            .unwrap_or(0);
        
        let new_pos = (current_pos + 1) % state.filtered_indices.len();
        state.highlighted_index = Some(state.filtered_indices[new_pos]);
        
        // Adjust scroll if needed
        self.ensure_highlighted_visible(state);
    }
    
    fn move_highlight_up(&self, state: &mut DropdownState) {
        if state.filtered_indices.is_empty() {
            return;
        }
        
        let current_pos = state.highlighted_index
            .and_then(|idx| state.filtered_indices.iter().position(|&x| x == idx))
            .unwrap_or(0);
        
        let new_pos = if current_pos == 0 {
            state.filtered_indices.len() - 1
        } else {
            current_pos - 1
        };
        
        state.highlighted_index = Some(state.filtered_indices[new_pos]);
        
        // Adjust scroll if needed
        self.ensure_highlighted_visible(state);
    }
    
    fn ensure_highlighted_visible(&self, state: &mut DropdownState) {
        if let Some(highlighted) = state.highlighted_index {
            if let Some(pos) = state.filtered_indices.iter().position(|&x| x == highlighted) {
                let item_height = 32.0; // Approximate item height
                let visible_start = (state.scroll_offset / item_height) as usize;
                let visible_end = visible_start + state.max_visible_options;
                
                if pos < visible_start {
                    state.scroll_offset = pos as f32 * item_height;
                } else if pos >= visible_end {
                    state.scroll_offset = (pos as f32 - state.max_visible_options as f32 + 1.0) * item_height;
                }
            }
        }
    }
    
    fn handle_search_input(&self, state: &mut DropdownState, key: &KeyCode) -> Result<EventResult> {
        match key {
            KeyCode::Backspace => {
                if !state.search_query.is_empty() {
                    state.search_query.pop();
                    self.filter_options(state);
                    state.highlighted_index = state.filtered_indices.first().copied();
                    return Ok(EventResult::HandledWithRedraw);
                }
            }
            _ => {
                // Handle character input (simplified - in real implementation would need proper text input handling)
                // This would be handled by TextInput events in a real implementation
            }
        }
        Ok(EventResult::NotHandled)
    }
    
    fn point_in_rect(&self, point: Vec2, rect_pos: Vec2, rect_size: Vec2) -> bool {
        point.x >= rect_pos.x &&
        point.x <= rect_pos.x + rect_size.x &&
        point.y >= rect_pos.y &&
        point.y <= rect_pos.y + rect_size.y
    }
    
    fn get_clicked_option(
        &self,
        state: &DropdownState,
        config: &DropdownConfig,
        layout: &LayoutResult,
        position: Vec2,
    ) -> Option<usize> {
        let list_y = layout.position.y + layout.size.y + 2.0;
        let item_height = config.font_size + 12.0;
        let options_start_y = if config.searchable { list_y + item_height } else { list_y };
        
        if position.x < layout.position.x || position.x > layout.position.x + layout.size.x {
            return None;
        }
        
        if position.y < options_start_y {
            return None;
        }
        
        let item_index = ((position.y - options_start_y) / item_height) as usize;
        let scroll_start = (state.scroll_offset / item_height) as usize;
        let actual_index = scroll_start + item_index;
        
        if actual_index < state.filtered_indices.len() {
            Some(state.filtered_indices[actual_index])
        } else {
            None
        }
    }
    
    fn get_hovered_option(
        &self,
        state: &DropdownState,
        config: &DropdownConfig,
        layout: &LayoutResult,
        position: Vec2,
    ) -> Option<usize> {
        self.get_clicked_option(state, config, layout, position)
    }
}

// Implement the factory trait
crate::impl_widget_factory!(DropdownWidget);

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_dropdown_creation() {
        let widget = DropdownWidget::new();
        let state = widget.create_state();
        
        assert_eq!(state.options.len(), 0);
        assert_eq!(state.selected_indices.len(), 0);
        assert!(!state.is_open);
    }
    
    #[test]
    fn test_dropdown_option_creation() {
        let option = DropdownOption::new("Label", "value")
            .with_icon("🎯")
            .with_group("Group 1");
        
        assert_eq!(option.label, "Label");
        assert_eq!(option.value, "value");
        assert_eq!(option.icon, Some("🎯".to_string()));
        assert_eq!(option.group, Some("Group 1".to_string()));
        assert!(!option.disabled);
    }
    
    #[test]
    fn test_config_from_properties() {
        let mut props = PropertyBag::new();
        props.set("multi_select".to_string(), crate::types::PropertyValue::Bool(true));
        props.set("searchable".to_string(), crate::types::PropertyValue::Bool(true));
        props.set("font_size".to_string(), crate::types::PropertyValue::Float(16.0));
        
        let config = DropdownConfig::from(&props);
        
        assert!(config.multi_select);
        assert!(config.searchable);
        assert_eq!(config.font_size, 16.0);
    }
}