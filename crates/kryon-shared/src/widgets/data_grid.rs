use super::{KryonWidget, EventResult, LayoutConstraints, PropertyBag};
use crate::elements::ElementId;
use crate::types::{InputEvent, LayoutResult, KeyCode, MouseButton};
use crate::render::RenderCommand;
use glam::{Vec2, Vec4};
use serde::{Serialize, Deserialize};
use anyhow::Result;
use std::collections::HashMap;

/// High-performance data grid widget with sorting, filtering, pagination, and virtual scrolling
#[derive(Default)]
pub struct DataGridWidget;

impl DataGridWidget {
    pub fn new() -> Self {
        Self
    }
}

/// State for the data grid widget
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct DataGridState {
    /// Column definitions
    pub columns: Vec<GridColumn>,
    /// All data rows
    pub rows: Vec<GridRow>,
    /// Filtered and sorted row indices
    pub filtered_rows: Vec<usize>,
    /// Currently selected row indices
    pub selected_rows: Vec<usize>,
    /// Current sort configuration
    pub sort_config: Option<SortConfig>,
    /// Active filters
    pub filters: HashMap<String, String>,
    /// Pagination state
    pub pagination: PaginationState,
    /// Virtual scrolling state
    pub scroll_state: ScrollState,
    /// Currently editing cell (row_index, column_id)
    pub editing_cell: Option<(usize, String)>,
    /// Temporary edit value
    pub edit_value: String,
    /// Row height in pixels
    pub row_height: f32,
    /// Header height in pixels
    pub header_height: f32,
    /// Whether the grid is in multi-select mode
    pub multi_select: bool,
    /// Loading state for async data
    pub is_loading: bool,
    /// Error message if loading failed
    pub error_message: Option<String>,
    /// Resize state for column resizing
    pub resize_state: Option<ResizeState>,
}

impl Default for DataGridState {
    fn default() -> Self {
        Self {
            columns: Vec::new(),
            rows: Vec::new(),
            filtered_rows: Vec::new(),
            selected_rows: Vec::new(),
            sort_config: None,
            filters: HashMap::new(),
            pagination: PaginationState::default(),
            scroll_state: ScrollState::default(),
            editing_cell: None,
            edit_value: String::new(),
            row_height: 32.0,
            header_height: 40.0,
            multi_select: false,
            is_loading: false,
            error_message: None,
            resize_state: None,
        }
    }
}

/// Column definition for the data grid
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct GridColumn {
    /// Unique identifier for the column
    pub id: String,
    /// Display title for the column header
    pub title: String,
    /// Column width in pixels
    pub width: f32,
    /// Minimum width for resizing
    pub min_width: f32,
    /// Maximum width for resizing
    pub max_width: Option<f32>,
    /// Whether this column is sortable
    pub sortable: bool,
    /// Whether this column is filterable
    pub filterable: bool,
    /// Whether this column is resizable
    pub resizable: bool,
    /// Whether this column is editable
    pub editable: bool,
    /// Data type for proper formatting and validation
    pub data_type: ColumnDataType,
    /// Text alignment
    pub alignment: TextAlignment,
    /// Whether column is currently visible
    pub visible: bool,
    /// Custom cell renderer function name
    pub renderer: Option<String>,
    /// Column-specific configuration
    pub config: HashMap<String, serde_json::Value>,
}

impl GridColumn {
    pub fn new(id: impl Into<String>, title: impl Into<String>, width: f32) -> Self {
        Self {
            id: id.into(),
            title: title.into(),
            width,
            min_width: 50.0,
            max_width: None,
            sortable: true,
            filterable: true,
            resizable: true,
            editable: false,
            data_type: ColumnDataType::Text,
            alignment: TextAlignment::Left,
            visible: true,
            renderer: None,
            config: HashMap::new(),
        }
    }
    
    pub fn sortable(mut self, sortable: bool) -> Self {
        self.sortable = sortable;
        self
    }
    
    pub fn editable(mut self, editable: bool) -> Self {
        self.editable = editable;
        self
    }
    
    pub fn data_type(mut self, data_type: ColumnDataType) -> Self {
        self.data_type = data_type;
        self
    }
    
    pub fn alignment(mut self, alignment: TextAlignment) -> Self {
        self.alignment = alignment;
        self
    }
    
    pub fn renderer(mut self, renderer: impl Into<String>) -> Self {
        self.renderer = Some(renderer.into());
        self
    }
}

/// Data types supported by grid columns
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum ColumnDataType {
    Text,
    Number,
    Date,
    Boolean,
    Currency,
    Percentage,
    Custom(String),
}

/// Text alignment options
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum TextAlignment {
    Left,
    Center,
    Right,
}

/// Data row in the grid
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct GridRow {
    /// Unique identifier for the row
    pub id: String,
    /// Cell data keyed by column ID
    pub cells: HashMap<String, CellValue>,
    /// Whether this row is selectable
    pub selectable: bool,
    /// Whether this row is editable
    pub editable: bool,
    /// Row-specific styling
    pub style_class: Option<String>,
    /// Additional row metadata
    pub metadata: HashMap<String, serde_json::Value>,
}

impl GridRow {
    pub fn new(id: impl Into<String>) -> Self {
        Self {
            id: id.into(),
            cells: HashMap::new(),
            selectable: true,
            editable: true,
            style_class: None,
            metadata: HashMap::new(),
        }
    }
    
    pub fn cell(mut self, column_id: impl Into<String>, value: CellValue) -> Self {
        self.cells.insert(column_id.into(), value);
        self
    }
    
    pub fn style_class(mut self, class: impl Into<String>) -> Self {
        self.style_class = Some(class.into());
        self
    }
}

/// Cell value types
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum CellValue {
    Text(String),
    Number(f64),
    Integer(i64),
    Boolean(bool),
    Date(String), // ISO date string
    Null,
    Custom(serde_json::Value),
}

impl CellValue {
    pub fn as_string(&self) -> String {
        match self {
            CellValue::Text(s) => s.clone(),
            CellValue::Number(n) => n.to_string(),
            CellValue::Integer(i) => i.to_string(),
            CellValue::Boolean(b) => b.to_string(),
            CellValue::Date(d) => d.clone(),
            CellValue::Null => String::new(),
            CellValue::Custom(v) => v.to_string(),
        }
    }
    
    pub fn as_number(&self) -> Option<f64> {
        match self {
            CellValue::Number(n) => Some(*n),
            CellValue::Integer(i) => Some(*i as f64),
            CellValue::Text(s) => s.parse().ok(),
            _ => None,
        }
    }
}

/// Sort configuration
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct SortConfig {
    pub column_id: String,
    pub direction: SortDirection,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum SortDirection {
    Ascending,
    Descending,
}

/// Pagination state
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct PaginationState {
    pub current_page: usize,
    pub page_size: usize,
    pub total_rows: usize,
    pub enabled: bool,
}

impl Default for PaginationState {
    fn default() -> Self {
        Self {
            current_page: 0,
            page_size: 50,
            total_rows: 0,
            enabled: false,
        }
    }
}

/// Virtual scrolling state
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct ScrollState {
    pub scroll_top: f32,
    pub scroll_left: f32,
    pub visible_start_row: usize,
    pub visible_end_row: usize,
    pub virtual_scrolling: bool,
}

impl Default for ScrollState {
    fn default() -> Self {
        Self {
            scroll_top: 0.0,
            scroll_left: 0.0,
            visible_start_row: 0,
            visible_end_row: 0,
            virtual_scrolling: true,
        }
    }
}

/// Column resize state
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct ResizeState {
    pub column_id: String,
    pub initial_width: f32,
    pub initial_mouse_x: f32,
}

/// Configuration for the data grid widget
#[derive(Debug, Clone)]
pub struct DataGridConfig {
    /// Grid background color
    pub background_color: Vec4,
    /// Header background color
    pub header_background: Vec4,
    /// Row background color (alternating)
    pub row_background: Vec4,
    /// Alternate row background color
    pub alt_row_background: Vec4,
    /// Selected row background color
    pub selected_background: Vec4,
    /// Hover row background color
    pub hover_background: Vec4,
    /// Text color
    pub text_color: Vec4,
    /// Header text color
    pub header_text_color: Vec4,
    /// Border color
    pub border_color: Vec4,
    /// Font size
    pub font_size: f32,
    /// Header font size
    pub header_font_size: f32,
    /// Cell padding
    pub cell_padding: f32,
    /// Enable row selection
    pub selectable: bool,
    /// Enable multi-row selection
    pub multi_select: bool,
    /// Enable column sorting
    pub sortable: bool,
    /// Enable column filtering
    pub filterable: bool,
    /// Enable column resizing
    pub resizable: bool,
    /// Enable cell editing
    pub editable: bool,
    /// Enable pagination
    pub paginated: bool,
    /// Enable virtual scrolling for large datasets
    pub virtual_scrolling: bool,
    /// Show row numbers
    pub show_row_numbers: bool,
    /// Show column headers
    pub show_headers: bool,
    /// Stripe alternate rows
    pub striped: bool,
    /// Show grid lines
    pub show_grid_lines: bool,
    /// Minimum column width
    pub min_column_width: f32,
}

impl Default for DataGridConfig {
    fn default() -> Self {
        Self {
            background_color: Vec4::new(1.0, 1.0, 1.0, 1.0),      // White
            header_background: Vec4::new(0.95, 0.95, 0.95, 1.0),  // Light gray
            row_background: Vec4::new(1.0, 1.0, 1.0, 1.0),        // White
            alt_row_background: Vec4::new(0.98, 0.98, 0.98, 1.0), // Very light gray
            selected_background: Vec4::new(0.85, 0.85, 1.0, 1.0), // Light blue
            hover_background: Vec4::new(0.92, 0.92, 1.0, 1.0),    // Very light blue
            text_color: Vec4::new(0.2, 0.2, 0.2, 1.0),            // Dark gray
            header_text_color: Vec4::new(0.1, 0.1, 0.1, 1.0),     // Darker gray
            border_color: Vec4::new(0.8, 0.8, 0.8, 1.0),          // Light gray
            font_size: 13.0,
            header_font_size: 14.0,
            cell_padding: 8.0,
            selectable: true,
            multi_select: false,
            sortable: true,
            filterable: false,
            resizable: true,
            editable: false,
            paginated: false,
            virtual_scrolling: true,
            show_row_numbers: false,
            show_headers: true,
            striped: true,
            show_grid_lines: true,
            min_column_width: 50.0,
        }
    }
}

impl From<&PropertyBag> for DataGridConfig {
    fn from(props: &PropertyBag) -> Self {
        let mut config = Self::default();
        
        if let Some(size) = props.get_f32("font_size") {
            config.font_size = size;
        }
        
        if let Some(size) = props.get_f32("header_font_size") {
            config.header_font_size = size;
        }
        
        if let Some(padding) = props.get_f32("cell_padding") {
            config.cell_padding = padding;
        }
        
        if let Some(selectable) = props.get_bool("selectable") {
            config.selectable = selectable;
        }
        
        if let Some(multi) = props.get_bool("multi_select") {
            config.multi_select = multi;
        }
        
        if let Some(sortable) = props.get_bool("sortable") {
            config.sortable = sortable;
        }
        
        if let Some(filterable) = props.get_bool("filterable") {
            config.filterable = filterable;
        }
        
        if let Some(resizable) = props.get_bool("resizable") {
            config.resizable = resizable;
        }
        
        if let Some(editable) = props.get_bool("editable") {
            config.editable = editable;
        }
        
        if let Some(paginated) = props.get_bool("paginated") {
            config.paginated = paginated;
        }
        
        if let Some(virtual_scrolling) = props.get_bool("virtual_scrolling") {
            config.virtual_scrolling = virtual_scrolling;
        }
        
        if let Some(show_row_numbers) = props.get_bool("show_row_numbers") {
            config.show_row_numbers = show_row_numbers;
        }
        
        if let Some(striped) = props.get_bool("striped") {
            config.striped = striped;
        }
        
        if let Some(show_grid_lines) = props.get_bool("show_grid_lines") {
            config.show_grid_lines = show_grid_lines;
        }
        
        config
    }
}

impl KryonWidget for DataGridWidget {
    const WIDGET_NAME: &'static str = "data-grid";
    type State = DataGridState;
    type Config = DataGridConfig;
    
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
        
        // Grid background
        commands.push(RenderCommand::DrawRect {
            position: layout.position,
            size: layout.size,
            color: config.background_color,
            border_radius: 0.0,
            border_width: if config.show_grid_lines { 1.0 } else { 0.0 },
            border_color: config.border_color,
        });
        
        let mut current_y = layout.position.y;
        
        // Render headers
        if config.show_headers {
            self.render_headers(&mut commands, state, config, layout, current_y)?;
            current_y += state.header_height;
        }
        
        // Render rows
        self.render_rows(&mut commands, state, config, layout, current_y)?;
        
        // Render scrollbars if needed
        if self.needs_vertical_scrollbar(state, config, layout) {
            self.render_vertical_scrollbar(&mut commands, state, config, layout)?;
        }
        
        if self.needs_horizontal_scrollbar(state, config, layout) {
            self.render_horizontal_scrollbar(&mut commands, state, config, layout)?;
        }
        
        // Render loading overlay
        if state.is_loading {
            self.render_loading_overlay(&mut commands, config, layout)?;
        }
        
        // Render error message
        if let Some(error) = &state.error_message {
            self.render_error_overlay(&mut commands, config, layout, error)?;
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
                // Check for header clicks (sorting, resizing)
                if config.show_headers && position.y >= layout.position.y && 
                   position.y <= layout.position.y + state.header_height {
                    return self.handle_header_click(state, config, layout, *position);
                }
                
                // Check for row clicks (selection, editing)
                if let Some(row_index) = self.get_clicked_row(state, config, layout, *position) {
                    return self.handle_row_click(state, config, row_index, *position);
                }
                
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::MouseMove { position } => {
                // Handle column resizing
                if let Some(resize_state) = &state.resize_state {
                    let delta = position.x - resize_state.initial_mouse_x;
                    let new_width = (resize_state.initial_width + delta).max(config.min_column_width);
                    
                    if let Some(column) = state.columns.iter_mut().find(|c| c.id == resize_state.column_id) {
                        if let Some(max_width) = column.max_width {
                            column.width = new_width.min(max_width);
                        } else {
                            column.width = new_width;
                        }
                    }
                    
                    return Ok(EventResult::HandledWithRedraw);
                }
                
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::MouseRelease { button: MouseButton::Left, .. } => {
                if state.resize_state.is_some() {
                    state.resize_state = None;
                    return Ok(EventResult::HandledWithRedraw);
                }
                
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::KeyPress { key, .. } => {
                match key {
                    KeyCode::ArrowUp => {
                        self.move_selection_up(state);
                        Ok(EventResult::HandledWithRedraw)
                    }
                    KeyCode::ArrowDown => {
                        self.move_selection_down(state);
                        Ok(EventResult::HandledWithRedraw)
                    }
                    KeyCode::Enter => {
                        if let Some(&selected_row) = state.selected_rows.first() {
                            return Ok(EventResult::EmitEvent {
                                event_name: "row_activated".to_string(),
                                data: serde_json::json!({
                                    "row_id": state.rows[selected_row].id,
                                    "row_index": selected_row
                                }),
                            });
                        }
                        Ok(EventResult::NotHandled)
                    }
                    KeyCode::Delete => {
                        if !state.selected_rows.is_empty() {
                            return Ok(EventResult::EmitEvent {
                                event_name: "rows_delete_requested".to_string(),
                                data: serde_json::json!({
                                    "row_indices": state.selected_rows
                                }),
                            });
                        }
                        Ok(EventResult::NotHandled)
                    }
                    _ => Ok(EventResult::NotHandled),
                }
            }
            
            InputEvent::Scroll { delta, position } => {
                if self.point_in_rect(*position, layout.position, layout.size) {
                    state.scroll_state.scroll_top = (state.scroll_state.scroll_top - delta.y * 3.0)
                        .max(0.0)
                        .min(self.max_scroll_top(state, config, layout));
                    
                    self.update_visible_rows(state, config, layout);
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
            min_width: Some(200.0),
            max_width: None,
            min_height: Some(100.0),
            max_height: None,
            aspect_ratio: None,
        })
    }
}

impl DataGridWidget {
    fn render_headers(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &DataGridState,
        config: &DataGridConfig,
        layout: &LayoutResult,
        y: f32,
    ) -> Result<()> {
        let mut x = layout.position.x - state.scroll_state.scroll_left;
        
        // Header background
        commands.push(RenderCommand::DrawRect {
            position: Vec2::new(layout.position.x, y),
            size: Vec2::new(layout.size.x, state.header_height),
            color: config.header_background,
            border_radius: 0.0,
            border_width: if config.show_grid_lines { 1.0 } else { 0.0 },
            border_color: config.border_color,
        });
        
        // Row number column header
        if config.show_row_numbers {
            let row_num_width = 60.0;
            
            commands.push(RenderCommand::DrawText {
                text: "#".to_string(),
                position: Vec2::new(x + config.cell_padding, y + config.cell_padding),
                font_size: config.header_font_size,
                color: config.header_text_color,
                max_width: Some(row_num_width - 2.0 * config.cell_padding),
            });
            
            if config.show_grid_lines {
                commands.push(RenderCommand::DrawLine {
                    start: Vec2::new(x + row_num_width, y),
                    end: Vec2::new(x + row_num_width, y + state.header_height),
                    color: config.border_color,
                    width: 1.0,
                });
            }
            
            x += row_num_width;
        }
        
        // Column headers
        for column in &state.columns {
            if !column.visible || x >= layout.position.x + layout.size.x {
                x += column.width;
                continue;
            }
            
            if x + column.width < layout.position.x {
                x += column.width;
                continue;
            }
            
            // Column header background
            let header_bg = if let Some(sort_config) = &state.sort_config {
                if sort_config.column_id == column.id {
                    Vec4::new(0.9, 0.9, 0.95, 1.0) // Slightly different for sorted column
                } else {
                    config.header_background
                }
            } else {
                config.header_background
            };
            
            commands.push(RenderCommand::DrawRect {
                position: Vec2::new(x, y),
                size: Vec2::new(column.width, state.header_height),
                color: header_bg,
                border_radius: 0.0,
                border_width: 0.0,
                border_color: Vec4::ZERO,
            });
            
            // Column title
            let text_x = match column.alignment {
                TextAlignment::Left => x + config.cell_padding,
                TextAlignment::Center => x + column.width / 2.0,
                TextAlignment::Right => x + column.width - config.cell_padding,
            };
            
            commands.push(RenderCommand::DrawText {
                text: column.title.clone(),
                position: Vec2::new(text_x, y + config.cell_padding),
                font_size: config.header_font_size,
                color: config.header_text_color,
                max_width: Some(column.width - 2.0 * config.cell_padding - 20.0), // Leave space for sort indicator
            });
            
            // Sort indicator
            if column.sortable && config.sortable {
                if let Some(sort_config) = &state.sort_config {
                    if sort_config.column_id == column.id {
                        let sort_symbol = match sort_config.direction {
                            SortDirection::Ascending => "▲",
                            SortDirection::Descending => "▼",
                        };
                        
                        commands.push(RenderCommand::DrawText {
                            text: sort_symbol.to_string(),
                            position: Vec2::new(x + column.width - 15.0, y + config.cell_padding),
                            font_size: config.header_font_size * 0.8,
                            color: config.header_text_color,
                            max_width: None,
                        });
                    }
                }
            }
            
            // Resize handle
            if column.resizable && config.resizable {
                commands.push(RenderCommand::DrawRect {
                    position: Vec2::new(x + column.width - 2.0, y),
                    size: Vec2::new(4.0, state.header_height),
                    color: Vec4::new(0.7, 0.7, 0.7, 0.5),
                    border_radius: 0.0,
                    border_width: 0.0,
                    border_color: Vec4::ZERO,
                });
            }
            
            // Column border
            if config.show_grid_lines {
                commands.push(RenderCommand::DrawLine {
                    start: Vec2::new(x + column.width, y),
                    end: Vec2::new(x + column.width, y + state.header_height),
                    color: config.border_color,
                    width: 1.0,
                });
            }
            
            x += column.width;
        }
        
        Ok(())
    }
    
    fn render_rows(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &DataGridState,
        config: &DataGridConfig,
        layout: &LayoutResult,
        start_y: f32,
    ) -> Result<()> {
        let available_height = layout.size.y - (start_y - layout.position.y);
        let visible_rows = (available_height / state.row_height).ceil() as usize + 1;
        
        let start_row = if config.virtual_scrolling {
            state.scroll_state.visible_start_row
        } else {
            0
        };
        
        let end_row = if config.virtual_scrolling {
            (start_row + visible_rows).min(state.filtered_rows.len())
        } else {
            state.filtered_rows.len()
        };
        
        for i in start_row..end_row {
            if i >= state.filtered_rows.len() {
                break;
            }
            
            let row_index = state.filtered_rows[i];
            let row = &state.rows[row_index];
            let y = start_y + ((i - start_row) as f32 * state.row_height);
            
            // Skip if row is not visible
            if y >= layout.position.y + layout.size.y || y + state.row_height < start_y {
                continue;
            }
            
            self.render_row(commands, state, config, layout, row, row_index, i, y)?;
        }
        
        Ok(())
    }
    
    fn render_row(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &DataGridState,
        config: &DataGridConfig,
        layout: &LayoutResult,
        row: &GridRow,
        row_index: usize,
        display_index: usize,
        y: f32,
    ) -> Result<()> {
        let is_selected = state.selected_rows.contains(&row_index);
        let is_odd = display_index % 2 == 1;
        
        // Row background
        let bg_color = if is_selected {
            config.selected_background
        } else if config.striped && is_odd {
            config.alt_row_background
        } else {
            config.row_background
        };
        
        commands.push(RenderCommand::DrawRect {
            position: Vec2::new(layout.position.x, y),
            size: Vec2::new(layout.size.x, state.row_height),
            color: bg_color,
            border_radius: 0.0,
            border_width: 0.0,
            border_color: Vec4::ZERO,
        });
        
        let mut x = layout.position.x - state.scroll_state.scroll_left;
        
        // Row number
        if config.show_row_numbers {
            let row_num_width = 60.0;
            
            commands.push(RenderCommand::DrawText {
                text: (display_index + 1).to_string(),
                position: Vec2::new(x + config.cell_padding, y + config.cell_padding),
                font_size: config.font_size,
                color: config.text_color,
                max_width: Some(row_num_width - 2.0 * config.cell_padding),
            });
            
            if config.show_grid_lines {
                commands.push(RenderCommand::DrawLine {
                    start: Vec2::new(x + row_num_width, y),
                    end: Vec2::new(x + row_num_width, y + state.row_height),
                    color: config.border_color,
                    width: 1.0,
                });
            }
            
            x += row_num_width;
        }
        
        // Cells
        for column in &state.columns {
            if !column.visible || x >= layout.position.x + layout.size.x {
                x += column.width;
                continue;
            }
            
            if x + column.width < layout.position.x {
                x += column.width;
                continue;
            }
            
            // Cell value
            let cell_value = row.cells.get(&column.id)
                .map(|v| v.as_string())
                .unwrap_or_default();
            
            // Cell text position
            let text_x = match column.alignment {
                TextAlignment::Left => x + config.cell_padding,
                TextAlignment::Center => x + column.width / 2.0,
                TextAlignment::Right => x + column.width - config.cell_padding,
            };
            
            // Editing cell
            let display_text = if state.editing_cell == Some((row_index, column.id.clone())) {
                state.edit_value.clone()
            } else {
                self.format_cell_value(&cell_value, &column.data_type)
            };
            
            commands.push(RenderCommand::DrawText {
                text: display_text,
                position: Vec2::new(text_x, y + config.cell_padding),
                font_size: config.font_size,
                color: config.text_color,
                max_width: Some(column.width - 2.0 * config.cell_padding),
            });
            
            // Cell border
            if config.show_grid_lines {
                commands.push(RenderCommand::DrawLine {
                    start: Vec2::new(x + column.width, y),
                    end: Vec2::new(x + column.width, y + state.row_height),
                    color: config.border_color,
                    width: 1.0,
                });
            }
            
            x += column.width;
        }
        
        // Row border
        if config.show_grid_lines {
            commands.push(RenderCommand::DrawLine {
                start: Vec2::new(layout.position.x, y + state.row_height),
                end: Vec2::new(layout.position.x + layout.size.x, y + state.row_height),
                color: config.border_color,
                width: 1.0,
            });
        }
        
        Ok(())
    }
    
    fn render_loading_overlay(
        &self,
        commands: &mut Vec<RenderCommand>,
        config: &DataGridConfig,
        layout: &LayoutResult,
    ) -> Result<()> {
        // Semi-transparent overlay
        commands.push(RenderCommand::DrawRect {
            position: layout.position,
            size: layout.size,
            color: Vec4::new(1.0, 1.0, 1.0, 0.8),
            border_radius: 0.0,
            border_width: 0.0,
            border_color: Vec4::ZERO,
        });
        
        // Loading text
        commands.push(RenderCommand::DrawText {
            text: "Loading...".to_string(),
            position: layout.position + layout.size / 2.0 - Vec2::new(30.0, 10.0),
            font_size: config.font_size * 1.2,
            color: config.text_color,
            max_width: None,
        });
        
        Ok(())
    }
    
    fn render_error_overlay(
        &self,
        commands: &mut Vec<RenderCommand>,
        config: &DataGridConfig,
        layout: &LayoutResult,
        error: &str,
    ) -> Result<()> {
        // Error background
        commands.push(RenderCommand::DrawRect {
            position: layout.position,
            size: layout.size,
            color: Vec4::new(1.0, 0.9, 0.9, 0.95),
            border_radius: 4.0,
            border_width: 2.0,
            border_color: Vec4::new(0.8, 0.2, 0.2, 1.0),
        });
        
        // Error text
        commands.push(RenderCommand::DrawText {
            text: format!("Error: {}", error),
            position: layout.position + Vec2::new(20.0, layout.size.y / 2.0 - 10.0),
            font_size: config.font_size,
            color: Vec4::new(0.6, 0.1, 0.1, 1.0),
            max_width: Some(layout.size.x - 40.0),
        });
        
        Ok(())
    }
    
    fn render_vertical_scrollbar(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &DataGridState,
        config: &DataGridConfig,
        layout: &LayoutResult,
    ) -> Result<()> {
        let scrollbar_width = 12.0;
        let scrollbar_x = layout.position.x + layout.size.x - scrollbar_width;
        let scrollbar_height = layout.size.y;
        
        // Scrollbar track
        commands.push(RenderCommand::DrawRect {
            position: Vec2::new(scrollbar_x, layout.position.y),
            size: Vec2::new(scrollbar_width, scrollbar_height),
            color: Vec4::new(0.95, 0.95, 0.95, 1.0),
            border_radius: 0.0,
            border_width: 0.0,
            border_color: Vec4::ZERO,
        });
        
        // Scrollbar thumb
        let content_height = state.filtered_rows.len() as f32 * state.row_height;
        let thumb_height = (scrollbar_height * scrollbar_height / content_height).min(scrollbar_height);
        let thumb_y = layout.position.y + (state.scroll_state.scroll_top / content_height) * (scrollbar_height - thumb_height);
        
        commands.push(RenderCommand::DrawRect {
            position: Vec2::new(scrollbar_x + 2.0, thumb_y),
            size: Vec2::new(scrollbar_width - 4.0, thumb_height),
            color: Vec4::new(0.6, 0.6, 0.6, 1.0),
            border_radius: 4.0,
            border_width: 0.0,
            border_color: Vec4::ZERO,
        });
        
        Ok(())
    }
    
    fn render_horizontal_scrollbar(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &DataGridState,
        config: &DataGridConfig,
        layout: &LayoutResult,
    ) -> Result<()> {
        // Implementation for horizontal scrollbar
        Ok(())
    }
    
    fn format_cell_value(&self, value: &str, data_type: &ColumnDataType) -> String {
        match data_type {
            ColumnDataType::Text => value.to_string(),
            ColumnDataType::Number => {
                if let Ok(num) = value.parse::<f64>() {
                    format!("{:.2}", num)
                } else {
                    value.to_string()
                }
            }
            ColumnDataType::Currency => {
                if let Ok(num) = value.parse::<f64>() {
                    format!("${:.2}", num)
                } else {
                    value.to_string()
                }
            }
            ColumnDataType::Percentage => {
                if let Ok(num) = value.parse::<f64>() {
                    format!("{:.1}%", num * 100.0)
                } else {
                    value.to_string()
                }
            }
            ColumnDataType::Boolean => {
                match value.to_lowercase().as_str() {
                    "true" | "1" | "yes" => "✓".to_string(),
                    "false" | "0" | "no" => "✗".to_string(),
                    _ => value.to_string(),
                }
            }
            ColumnDataType::Date => {
                // In a real implementation, would parse and format dates properly
                value.to_string()
            }
            ColumnDataType::Custom(_) => value.to_string(),
        }
    }
    
    fn handle_header_click(
        &self,
        state: &mut DataGridState,
        config: &DataGridConfig,
        layout: &LayoutResult,
        position: Vec2,
    ) -> Result<EventResult> {
        let mut x = layout.position.x - state.scroll_state.scroll_left;
        
        if config.show_row_numbers {
            x += 60.0;
        }
        
        for column in &state.columns {
            if !column.visible {
                continue;
            }
            
            if position.x >= x && position.x <= x + column.width {
                // Check for resize handle
                if column.resizable && config.resizable && position.x >= x + column.width - 6.0 {
                    state.resize_state = Some(ResizeState {
                        column_id: column.id.clone(),
                        initial_width: column.width,
                        initial_mouse_x: position.x,
                    });
                    return Ok(EventResult::HandledWithRedraw);
                }
                
                // Handle sorting
                if column.sortable && config.sortable {
                    let new_direction = if let Some(sort_config) = &state.sort_config {
                        if sort_config.column_id == column.id {
                            match sort_config.direction {
                                SortDirection::Ascending => SortDirection::Descending,
                                SortDirection::Descending => SortDirection::Ascending,
                            }
                        } else {
                            SortDirection::Ascending
                        }
                    } else {
                        SortDirection::Ascending
                    };
                    
                    state.sort_config = Some(SortConfig {
                        column_id: column.id.clone(),
                        direction: new_direction,
                    });
                    
                    self.sort_rows(state);
                    
                    return Ok(EventResult::EmitEvent {
                        event_name: "column_sorted".to_string(),
                        data: serde_json::json!({
                            "column_id": column.id,
                            "direction": match new_direction {
                                SortDirection::Ascending => "asc",
                                SortDirection::Descending => "desc",
                            }
                        }),
                    });
                }
                
                break;
            }
            
            x += column.width;
        }
        
        Ok(EventResult::NotHandled)
    }
    
    fn handle_row_click(
        &self,
        state: &mut DataGridState,
        config: &DataGridConfig,
        row_index: usize,
        position: Vec2,
    ) -> Result<EventResult> {
        if config.selectable {
            if config.multi_select {
                if let Some(pos) = state.selected_rows.iter().position(|&x| x == row_index) {
                    state.selected_rows.remove(pos);
                } else {
                    state.selected_rows.push(row_index);
                }
            } else {
                state.selected_rows = vec![row_index];
            }
            
            return Ok(EventResult::EmitEvent {
                event_name: "selection_changed".to_string(),
                data: serde_json::json!({
                    "selected_rows": state.selected_rows,
                    "selected_row_ids": state.selected_rows.iter()
                        .map(|&i| &state.rows[i].id)
                        .collect::<Vec<_>>()
                }),
            });
        }
        
        Ok(EventResult::NotHandled)
    }
    
    fn get_clicked_row(
        &self,
        state: &DataGridState,
        config: &DataGridConfig,
        layout: &LayoutResult,
        position: Vec2,
    ) -> Option<usize> {
        let header_height = if config.show_headers { state.header_height } else { 0.0 };
        let rows_start_y = layout.position.y + header_height;
        
        if position.y < rows_start_y {
            return None;
        }
        
        let row_offset = ((position.y - rows_start_y) / state.row_height) as usize;
        let start_row = if config.virtual_scrolling {
            state.scroll_state.visible_start_row
        } else {
            0
        };
        
        let actual_row_index = start_row + row_offset;
        
        if actual_row_index < state.filtered_rows.len() {
            Some(state.filtered_rows[actual_row_index])
        } else {
            None
        }
    }
    
    fn sort_rows(&self, state: &mut DataGridState) {
        if let Some(sort_config) = &state.sort_config {
            state.filtered_rows.sort_by(|&a, &b| {
                let row_a = &state.rows[a];
                let row_b = &state.rows[b];
                
                let value_a = row_a.cells.get(&sort_config.column_id);
                let value_b = row_b.cells.get(&sort_config.column_id);
                
                let cmp = match (value_a, value_b) {
                    (Some(a), Some(b)) => self.compare_cell_values(a, b),
                    (Some(_), None) => std::cmp::Ordering::Greater,
                    (None, Some(_)) => std::cmp::Ordering::Less,
                    (None, None) => std::cmp::Ordering::Equal,
                };
                
                match sort_config.direction {
                    SortDirection::Ascending => cmp,
                    SortDirection::Descending => cmp.reverse(),
                }
            });
        }
    }
    
    fn compare_cell_values(&self, a: &CellValue, b: &CellValue) -> std::cmp::Ordering {
        match (a, b) {
            (CellValue::Number(a), CellValue::Number(b)) => a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal),
            (CellValue::Integer(a), CellValue::Integer(b)) => a.cmp(b),
            (CellValue::Boolean(a), CellValue::Boolean(b)) => a.cmp(b),
            (CellValue::Text(a), CellValue::Text(b)) => a.cmp(b),
            (CellValue::Date(a), CellValue::Date(b)) => a.cmp(b),
            _ => a.as_string().cmp(&b.as_string()),
        }
    }
    
    fn move_selection_up(&self, state: &mut DataGridState) {
        if let Some(&current) = state.selected_rows.first() {
            if let Some(pos) = state.filtered_rows.iter().position(|&x| x == current) {
                if pos > 0 {
                    let new_selection = state.filtered_rows[pos - 1];
                    state.selected_rows = vec![new_selection];
                }
            }
        } else if !state.filtered_rows.is_empty() {
            state.selected_rows = vec![state.filtered_rows[0]];
        }
    }
    
    fn move_selection_down(&self, state: &mut DataGridState) {
        if let Some(&current) = state.selected_rows.first() {
            if let Some(pos) = state.filtered_rows.iter().position(|&x| x == current) {
                if pos < state.filtered_rows.len() - 1 {
                    let new_selection = state.filtered_rows[pos + 1];
                    state.selected_rows = vec![new_selection];
                }
            }
        } else if !state.filtered_rows.is_empty() {
            state.selected_rows = vec![state.filtered_rows[0]];
        }
    }
    
    fn update_visible_rows(&self, state: &mut DataGridState, config: &DataGridConfig, layout: &LayoutResult) {
        if config.virtual_scrolling {
            let start_row = (state.scroll_state.scroll_top / state.row_height) as usize;
            let visible_count = (layout.size.y / state.row_height).ceil() as usize + 2;
            
            state.scroll_state.visible_start_row = start_row;
            state.scroll_state.visible_end_row = (start_row + visible_count).min(state.filtered_rows.len());
        }
    }
    
    fn needs_vertical_scrollbar(&self, state: &DataGridState, config: &DataGridConfig, layout: &LayoutResult) -> bool {
        let content_height = state.filtered_rows.len() as f32 * state.row_height;
        content_height > layout.size.y
    }
    
    fn needs_horizontal_scrollbar(&self, state: &DataGridState, config: &DataGridConfig, layout: &LayoutResult) -> bool {
        let total_width: f32 = state.columns.iter()
            .filter(|c| c.visible)
            .map(|c| c.width)
            .sum();
        
        let row_num_width = if config.show_row_numbers { 60.0 } else { 0.0 };
        total_width + row_num_width > layout.size.x
    }
    
    fn max_scroll_top(&self, state: &DataGridState, config: &DataGridConfig, layout: &LayoutResult) -> f32 {
        let content_height = state.filtered_rows.len() as f32 * state.row_height;
        let header_height = if config.show_headers { state.header_height } else { 0.0 };
        (content_height - (layout.size.y - header_height)).max(0.0)
    }
    
    fn point_in_rect(&self, point: Vec2, rect_pos: Vec2, rect_size: Vec2) -> bool {
        point.x >= rect_pos.x &&
        point.x <= rect_pos.x + rect_size.x &&
        point.y >= rect_pos.y &&
        point.y <= rect_pos.y + rect_size.y
    }
}

// Implement the factory trait
crate::impl_widget_factory!(DataGridWidget);

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_data_grid_creation() {
        let widget = DataGridWidget::new();
        let state = widget.create_state();
        
        assert_eq!(state.columns.len(), 0);
        assert_eq!(state.rows.len(), 0);
        assert_eq!(state.selected_rows.len(), 0);
    }
    
    #[test]
    fn test_grid_column_creation() {
        let column = GridColumn::new("id", "Name", 150.0)
            .sortable(true)
            .editable(true)
            .data_type(ColumnDataType::Text)
            .alignment(TextAlignment::Left);
        
        assert_eq!(column.id, "id");
        assert_eq!(column.title, "Name");
        assert_eq!(column.width, 150.0);
        assert!(column.sortable);
        assert!(column.editable);
    }
    
    #[test]
    fn test_grid_row_creation() {
        let row = GridRow::new("row1")
            .cell("name", CellValue::Text("John Doe".to_string()))
            .cell("age", CellValue::Integer(30))
            .cell("active", CellValue::Boolean(true));
        
        assert_eq!(row.id, "row1");
        assert_eq!(row.cells.len(), 3);
        
        if let Some(CellValue::Text(name)) = row.cells.get("name") {
            assert_eq!(name, "John Doe");
        } else {
            panic!("Expected text cell");
        }
    }
    
    #[test]
    fn test_cell_value_formatting() {
        let widget = DataGridWidget::new();
        
        assert_eq!(widget.format_cell_value("123.456", &ColumnDataType::Number), "123.46");
        assert_eq!(widget.format_cell_value("123.456", &ColumnDataType::Currency), "$123.46");
        assert_eq!(widget.format_cell_value("0.75", &ColumnDataType::Percentage), "75.0%");
        assert_eq!(widget.format_cell_value("true", &ColumnDataType::Boolean), "✓");
        assert_eq!(widget.format_cell_value("false", &ColumnDataType::Boolean), "✗");
    }
}