// crates/kryon-core/src/elements.rs
use glam::Vec4;
use std::collections::HashMap;
use crate::{PropertyValue, LayoutSize, LayoutPosition, Transform};

/// Input element state management inspired by raygui patterns
#[derive(Debug, Clone, PartialEq)]
pub struct InputState {
    /// Current input mode (normal, focused, edit, disabled)
    pub input_mode: InputMode,
    /// Current text content
    pub text: String,
    /// Cursor position within the text (character index)
    pub cursor_position: usize,
    /// Selection start and end (for text selection)
    pub selection_start: Option<usize>,
    pub selection_end: Option<usize>,
    /// Whether the input is in edit mode (actively receiving text input)
    pub is_editing: bool,
    /// Text scroll offset for long text (horizontal scrolling)
    pub text_scroll_offset: f32,
    /// Maximum allowed characters (0 = unlimited)
    pub max_length: usize,
    /// Input validation state
    pub validation_state: InputValidationState,
    /// Placeholder text shown when empty
    pub placeholder: String,
    /// Current input type (text, password, email, etc.)
    pub input_type: InputType,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum InputMode {
    /// Normal state - not focused
    Normal = 0,
    /// Mouse is hovering over the input
    Hover = 1,
    /// Input has focus but not actively editing
    Focused = 2,
    /// Input is being actively edited (typing)
    Edit = 3,
    /// Input is disabled
    Disabled = 4,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum InputType {
    Text = 0,
    Password = 1,
    Email = 2,
    Number = 3,
    Search = 4,
    Tel = 5,
    Url = 6,
    Textarea = 7,
    Range = 8,
    Checkbox = 9,
    Radio = 10,
}

#[derive(Debug, Clone, PartialEq)]
pub enum InputValidationState {
    Valid,
    Invalid(String), // Error message
    Warning(String), // Warning message
}

impl Default for InputState {
    fn default() -> Self {
        Self {
            input_mode: InputMode::Normal,
            text: String::new(),
            cursor_position: 0,
            selection_start: None,
            selection_end: None,
            is_editing: false,
            text_scroll_offset: 0.0,
            max_length: 0,
            validation_state: InputValidationState::Valid,
            placeholder: String::new(),
            input_type: InputType::Text,
        }
    }
}

impl InputState {
    /// Create a new input state with specified type
    pub fn new(input_type: InputType) -> Self {
        Self {
            input_type,
            ..Default::default()
        }
    }
    
    /// Set the input mode and handle state transitions
    pub fn set_mode(&mut self, mode: InputMode) {
        match mode {
            InputMode::Edit => {
                self.is_editing = true;
                self.input_mode = mode;
            }
            InputMode::Focused => {
                // If we were editing, stop editing but keep focus
                if self.is_editing {
                    self.is_editing = false;
                }
                self.input_mode = mode;
            }
            _ => {
                self.is_editing = false;
                self.input_mode = mode;
            }
        }
    }
    
    /// Insert text at cursor position
    pub fn insert_text(&mut self, text: &str) {
        if self.max_length > 0 && self.text.len() + text.len() > self.max_length {
            return; // Reject if would exceed max length
        }
        
        // Clear selection if any
        if let (Some(start), Some(end)) = (self.selection_start, self.selection_end) {
            let actual_start = start.min(end);
            let actual_end = start.max(end);
            self.text.replace_range(actual_start..actual_end, text);
            self.cursor_position = actual_start + text.len();
            self.selection_start = None;
            self.selection_end = None;
        } else {
            self.text.insert_str(self.cursor_position, text);
            self.cursor_position += text.len();
        }
    }
    
    /// Delete character at cursor (backspace)
    pub fn delete_char_backward(&mut self) {
        if let (Some(start), Some(end)) = (self.selection_start, self.selection_end) {
            // Delete selection
            let actual_start = start.min(end);
            let actual_end = start.max(end);
            self.text.replace_range(actual_start..actual_end, "");
            self.cursor_position = actual_start;
            self.selection_start = None;
            self.selection_end = None;
        } else if self.cursor_position > 0 {
            // Delete single character
            self.cursor_position -= 1;
            self.text.remove(self.cursor_position);
        }
    }
    
    /// Delete character after cursor (delete key)
    pub fn delete_char_forward(&mut self) {
        if let (Some(start), Some(end)) = (self.selection_start, self.selection_end) {
            // Delete selection
            let actual_start = start.min(end);
            let actual_end = start.max(end);
            self.text.replace_range(actual_start..actual_end, "");
            self.cursor_position = actual_start;
            self.selection_start = None;
            self.selection_end = None;
        } else if self.cursor_position < self.text.len() {
            self.text.remove(self.cursor_position);
        }
    }
    
    /// Move cursor left
    pub fn move_cursor_left(&mut self, extend_selection: bool) {
        if extend_selection {
            if self.selection_start.is_none() {
                self.selection_start = Some(self.cursor_position);
            }
        } else {
            self.selection_start = None;
            self.selection_end = None;
        }
        
        if self.cursor_position > 0 {
            self.cursor_position -= 1;
        }
        
        if extend_selection {
            self.selection_end = Some(self.cursor_position);
        }
    }
    
    /// Move cursor right
    pub fn move_cursor_right(&mut self, extend_selection: bool) {
        if extend_selection {
            if self.selection_start.is_none() {
                self.selection_start = Some(self.cursor_position);
            }
        } else {
            self.selection_start = None;
            self.selection_end = None;
        }
        
        if self.cursor_position < self.text.len() {
            self.cursor_position += 1;
        }
        
        if extend_selection {
            self.selection_end = Some(self.cursor_position);
        }
    }
    
    /// Move cursor to start of text
    pub fn move_cursor_to_start(&mut self, extend_selection: bool) {
        if extend_selection {
            if self.selection_start.is_none() {
                self.selection_start = Some(self.cursor_position);
            }
            self.selection_end = Some(0);
        } else {
            self.selection_start = None;
            self.selection_end = None;
        }
        self.cursor_position = 0;
    }
    
    /// Move cursor to end of text
    pub fn move_cursor_to_end(&mut self, extend_selection: bool) {
        if extend_selection {
            if self.selection_start.is_none() {
                self.selection_start = Some(self.cursor_position);
            }
            self.selection_end = Some(self.text.len());
        } else {
            self.selection_start = None;
            self.selection_end = None;
        }
        self.cursor_position = self.text.len();
    }
    
    /// Get display text (masked for password inputs)
    pub fn get_display_text(&self) -> String {
        match self.input_type {
            InputType::Password => "*".repeat(self.text.len()),
            _ => self.text.clone(),
        }
    }
    
    /// Get text to display (either content or placeholder)
    pub fn get_rendered_text(&self) -> (&str, bool) {
        if self.text.is_empty() && !self.placeholder.is_empty() {
            (&self.placeholder, true) // (text, is_placeholder)
        } else {
            match self.input_type {
                InputType::Password => {
                    // For password, we need to return a temporary string
                    // This is a bit awkward - in practice, the renderer should handle this
                    (&self.text, false)
                }
                _ => (&self.text, false)
            }
        }
    }
}

/// Select/Dropdown element state
#[derive(Debug, Clone, PartialEq)]
pub struct SelectState {
    /// Current selected value(s)
    pub selected_values: Vec<String>,
    /// Available options
    pub options: Vec<SelectOption>,
    /// Whether dropdown is open
    pub is_open: bool,
    /// Multiple selection allowed
    pub multiple: bool,
    /// Maximum visible options (for scrolling)
    pub size: usize,
    /// Currently highlighted option index
    pub highlighted_index: Option<usize>,
}

/// Option within a select element
#[derive(Debug, Clone, PartialEq)]
pub struct SelectOption {
    pub value: String,
    pub text: String,
    pub selected: bool,
    pub disabled: bool,
    pub group: Option<String>,
}

impl Default for SelectState {
    fn default() -> Self {
        Self {
            selected_values: Vec::new(),
            options: Vec::new(),
            is_open: false,
            multiple: false,
            size: 1,
            highlighted_index: None,
        }
    }
}

/// Slider/Range element state
#[derive(Debug, Clone, PartialEq)]
pub struct SliderState {
    /// Current value
    pub value: f64,
    /// Minimum value
    pub min: f64,
    /// Maximum value
    pub max: f64,
    /// Step increment
    pub step: f64,
    /// Slider orientation
    pub orientation: SliderOrientation,
    /// Whether user is currently dragging
    pub is_dragging: bool,
    /// Thumb size in pixels
    pub thumb_size: f32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SliderOrientation {
    Horizontal = 0,
    Vertical = 1,
}

impl Default for SliderState {
    fn default() -> Self {
        Self {
            value: 0.0,
            min: 0.0,
            max: 100.0,
            step: 1.0,
            orientation: SliderOrientation::Horizontal,
            is_dragging: false,
            thumb_size: 20.0,
        }
    }
}

/// Progress bar element state
#[derive(Debug, Clone, PartialEq)]
pub struct ProgressState {
    /// Current progress value
    pub value: f64,
    /// Maximum value (for determinate progress)
    pub max: f64,
    /// Whether progress is indeterminate
    pub indeterminate: bool,
}

impl Default for ProgressState {
    fn default() -> Self {
        Self {
            value: 0.0,
            max: 100.0,
            indeterminate: false,
        }
    }
}

/// Checkbox element state (dedicated checkbox, not input type)
#[derive(Debug, Clone, PartialEq)]
pub struct CheckboxState {
    /// Whether checkbox is checked
    pub checked: bool,
    /// Indeterminate state (tri-state checkbox)
    pub indeterminate: bool,
    /// Associated value when checked
    pub value: String,
}

impl Default for CheckboxState {
    fn default() -> Self {
        Self {
            checked: false,
            indeterminate: false,
            value: "on".to_string(),
        }
    }
}

/// Radio group element state
#[derive(Debug, Clone, PartialEq)]
pub struct RadioGroupState {
    /// Currently selected radio button value
    pub selected_value: Option<String>,
    /// Radio button options
    pub options: Vec<RadioOption>,
    /// Group name for form submission
    pub name: String,
}

/// Individual radio button option
#[derive(Debug, Clone, PartialEq)]
pub struct RadioOption {
    pub value: String,
    pub text: String,
    pub selected: bool,
    pub disabled: bool,
}

impl Default for RadioGroupState {
    fn default() -> Self {
        Self {
            selected_value: None,
            options: Vec::new(),
            name: String::new(),
        }
    }
}

/// Toggle/Switch element state
#[derive(Debug, Clone, PartialEq)]
pub struct ToggleState {
    /// Whether toggle is on
    pub on: bool,
    /// Text when on
    pub on_text: String,
    /// Text when off
    pub off_text: String,
}

impl Default for ToggleState {
    fn default() -> Self {
        Self {
            on: false,
            on_text: "On".to_string(),
            off_text: "Off".to_string(),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum ElementType {
    App = 0x00,
    Container = 0x01,
    Text = 0x02,
    Link = 0x03,
    Image = 0x04,
    Canvas = 0x05, // Native 2D/3D drawing canvas
    Video = 0x06, // Native video playbook element
    EmbedView = 0x07, // For platform-breaking content (webview, wasm, native renderer emulation, etc.)
    Svg = 0x08, // SVG vector graphics element
    Button = 0x10,
    Input = 0x11,
    Select = 0x12,       // Dropdown/combobox
    Slider = 0x13,       // Range slider
    ProgressBar = 0x14,  // Progress indicator
    Checkbox = 0x15,     // Dedicated checkbox element
    RadioGroup = 0x16,   // Radio button group
    Toggle = 0x17,       // Switch/toggle
    DatePicker = 0x18,   // Date input
    FilePicker = 0x19,   // File upload
    Form = 0x1A,         // Form container
    
    // List elements
    List = 0x20,         // Ordered/unordered list
    ListItem = 0x21,     // List item
    
    // Table elements
    Table = 0x30,        // Table container
    TableRow = 0x31,     // Table row
    TableCell = 0x32,    // Table cell
    TableHeader = 0x33,  // Table header
    
    Custom(u8),
}

impl From<u8> for ElementType {
    fn from(value: u8) -> Self {
        match value {
            0x00 => ElementType::App,
            0x01 => ElementType::Container,
            0x02 => ElementType::Text,
            0x03 => ElementType::Link,
            0x04 => ElementType::Image,
            0x05 => ElementType::Canvas,
            0x06 => ElementType::Video,
            0x07 => ElementType::EmbedView,
            0x08 => ElementType::Svg,
            0x10 => ElementType::Button,
            0x11 => ElementType::Input,
            0x12 => ElementType::Select,
            0x13 => ElementType::Slider,
            0x14 => ElementType::ProgressBar,
            0x15 => ElementType::Checkbox,
            0x16 => ElementType::RadioGroup,
            0x17 => ElementType::Toggle,
            0x18 => ElementType::DatePicker,
            0x19 => ElementType::FilePicker,
            0x1A => ElementType::Form,
            0x20 => ElementType::List,
            0x21 => ElementType::ListItem,
            0x30 => ElementType::Table,
            0x31 => ElementType::TableRow,
            0x32 => ElementType::TableCell,
            0x33 => ElementType::TableHeader,
            other => ElementType::Custom(other),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum InteractionState {
    Normal = 0,
    Hover = 1,
    Active = 2,
    Focus = 4,
    Disabled = 8,
    Checked = 16,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OverflowType {
    Visible = 0,
    Hidden = 1,
    Scroll = 2,
    Auto = 3,
}

#[derive(Debug, Clone)]
pub struct Element {
    pub id: String,
    pub element_type: ElementType,
    pub parent: Option<ElementId>,
    pub children: Vec<ElementId>,
    
    pub style_id: u8,

    // Layout properties
    pub layout_position: LayoutPosition,  // Flexible position with percentage support
    pub layout_size: LayoutSize,          // Flexible size with percentage support
    pub layout_flags: u8,
    pub gap: f32,        // Gap between flex items
    
    // Overflow properties
    pub overflow_x: OverflowType,
    pub overflow_y: OverflowType,
    pub max_height: Option<f32>,
    pub max_width: Option<f32>,
    
    // Visual properties
    pub background_color: Vec4,
    pub text_color: Vec4,
    pub border_color: Vec4,
    pub border_width: f32,
    pub border_radius: f32,
    pub opacity: f32,
    pub visible: bool,
    pub z_index: i32,
    
    // Transform properties
    pub transform: Transform,
    
    // Text properties
    pub text: String,
    pub font_size: f32,
    pub font_weight: FontWeight,
    pub font_family: String,
    pub text_alignment: TextAlignment,
    
    // Interactive properties
    pub cursor: CursorType,
    pub disabled: bool,
    pub current_state: InteractionState,
    
    // Input-specific state (only used for Input elements)
    pub input_state: Option<InputState>,
    
    // Form element states (only used for respective element types)
    pub select_state: Option<SelectState>,
    pub slider_state: Option<SliderState>,
    pub progress_state: Option<ProgressState>,
    pub checkbox_state: Option<CheckboxState>,
    pub radio_group_state: Option<RadioGroupState>,
    pub toggle_state: Option<ToggleState>,
    
    // Custom properties (for components)
    pub custom_properties: HashMap<String, PropertyValue>,
    
    // State-based properties
    pub state_properties: HashMap<InteractionState, HashMap<String, PropertyValue>>,
    
    // Event handlers
    pub event_handlers: HashMap<EventType, String>,
    
    // Component-specific
    pub component_name: Option<String>,
    pub is_component_instance: bool,
    
    // EmbedView-specific (webview, wasm, native renderer emulation)
    pub embed_view_type: Option<String>,     // e.g., "webview", "wasm", "native_renderer", "iframe"
    pub embed_view_source: Option<String>,   // URI or function name
    pub embed_view_config: HashMap<String, PropertyValue>, // Type-specific config
    
    // Legacy compatibility fields (deprecated)
    pub native_backend: Option<String>,        // Mapped to embed_view_type
    pub native_render_script: Option<String>,  // Mapped to embed_view_source
    pub native_config: HashMap<String, PropertyValue>, // Mapped to embed_view_config
}

pub type ElementId = u32;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FontWeight {
    Normal = 400,
    Bold = 700,
    Light = 300,
    Heavy = 900,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TextAlignment {
    Start,
    Center,
    End,
    Justify,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CursorType {
    Default,
    Pointer,
    Text,
    Move,
    NotAllowed,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum EventType {
    Click,
    Press,
    Release,
    Hover,
    Focus,
    Blur,
    Change,
    Submit,
}

impl Default for Element {
    fn default() -> Self {
        Self {
            id: String::new(),
            element_type: ElementType::Container,
            parent: None,
            children: Vec::new(),
            style_id: 0, 
            layout_position: LayoutPosition::zero(),
            layout_size: LayoutSize::auto(),
            layout_flags: 0,
            gap: 0.0,
            overflow_x: OverflowType::Visible,
            overflow_y: OverflowType::Visible,
            max_height: None,
            max_width: None,
            background_color: Vec4::new(0.0, 0.0, 0.0, 0.0), // Transparent
            text_color: Vec4::new(0.0, 0.0, 0.0, 1.0), // Black
            border_color: Vec4::new(0.0, 0.0, 0.0, 0.0), // Transparent
            border_width: 0.0,
            border_radius: 0.0,
            opacity: 1.0,
            visible: true,
            z_index: 0,
            transform: Transform::default(),
            text: String::new(),
            font_size: 14.0,
            font_weight: FontWeight::Normal,
            font_family: "default".to_string(),
            text_alignment: TextAlignment::Start,
            cursor: CursorType::Default,
            disabled: false,
            current_state: InteractionState::Normal,
            input_state: None,
            select_state: None,
            slider_state: None,
            progress_state: None,
            checkbox_state: None,
            radio_group_state: None,
            toggle_state: None,
            custom_properties: HashMap::new(),
            state_properties: HashMap::new(),
            event_handlers: HashMap::new(),
            component_name: None,
            is_component_instance: false,
            embed_view_type: None,
            embed_view_source: None,
            embed_view_config: HashMap::new(),
            
            native_backend: None,
            native_render_script: None,
            native_config: HashMap::new(),
        }
    }
}

impl Element {
    /// Create a new element with the specified type
    pub fn new(element_type: ElementType) -> Self {
        Self {
            element_type,
            ..Default::default()
        }
    }
    
    /// Apply a transform to this element
    pub fn set_transform(&mut self, transform: Transform) {
        self.transform = transform;
    }
    
    /// Get the current transform of this element
    pub fn get_transform(&self) -> &Transform {
        &self.transform
    }
    
    /// Check if this element has a non-identity transform
    pub fn has_transform(&self) -> bool {
        !self.transform.is_identity()
    }
    
    /// Initialize input state for this element (should only be called for Input elements)
    pub fn initialize_input_state(&mut self, input_type: InputType) {
        if self.element_type == ElementType::Input {
            self.input_state = Some(InputState::new(input_type));
        }
    }
    
    /// Initialize select state for this element (should only be called for Select elements)
    pub fn initialize_select_state(&mut self) {
        if self.element_type == ElementType::Select {
            self.select_state = Some(SelectState::default());
        }
    }
    
    /// Initialize slider state for this element (should only be called for Slider elements)
    pub fn initialize_slider_state(&mut self) {
        if self.element_type == ElementType::Slider {
            self.slider_state = Some(SliderState::default());
        }
    }
    
    /// Initialize progress state for this element (should only be called for ProgressBar elements)
    pub fn initialize_progress_state(&mut self) {
        if self.element_type == ElementType::ProgressBar {
            self.progress_state = Some(ProgressState::default());
        }
    }
    
    /// Initialize checkbox state for this element (should only be called for Checkbox elements)
    pub fn initialize_checkbox_state(&mut self) {
        if self.element_type == ElementType::Checkbox {
            self.checkbox_state = Some(CheckboxState::default());
        }
    }
    
    /// Initialize radio group state for this element (should only be called for RadioGroup elements)
    pub fn initialize_radio_group_state(&mut self) {
        if self.element_type == ElementType::RadioGroup {
            self.radio_group_state = Some(RadioGroupState::default());
        }
    }
    
    /// Initialize toggle state for this element (should only be called for Toggle elements)
    pub fn initialize_toggle_state(&mut self) {
        if self.element_type == ElementType::Toggle {
            self.toggle_state = Some(ToggleState::default());
        }
    }
    
    /// Get mutable reference to input state (only for Input elements)
    pub fn get_input_state_mut(&mut self) -> Option<&mut InputState> {
        self.input_state.as_mut()
    }
    
    /// Get reference to input state (only for Input elements)
    pub fn get_input_state(&self) -> Option<&InputState> {
        self.input_state.as_ref()
    }
    
    /// Get mutable reference to select state (only for Select elements)
    pub fn get_select_state_mut(&mut self) -> Option<&mut SelectState> {
        self.select_state.as_mut()
    }
    
    /// Get reference to select state (only for Select elements)
    pub fn get_select_state(&self) -> Option<&SelectState> {
        self.select_state.as_ref()
    }
    
    /// Check if this element is an input that can receive focus
    pub fn can_receive_focus(&self) -> bool {
        match self.element_type {
            ElementType::Input 
            | ElementType::Button 
            | ElementType::Select 
            | ElementType::Slider 
            | ElementType::Checkbox 
            | ElementType::RadioGroup 
            | ElementType::Toggle => !self.disabled,
            _ => false,
        }
    }
    
    /// Set focus state for input elements
    pub fn set_focus(&mut self, focused: bool) {
        if let Some(input_state) = &mut self.input_state {
            if focused {
                input_state.set_mode(InputMode::Focused);
            } else {
                input_state.set_mode(InputMode::Normal);
            }
        }
        
        // Update general interaction state
        if focused {
            self.current_state = InteractionState::Focus;
        } else {
            self.current_state = InteractionState::Normal;
        }
    }
    
    /// Handle text input for input elements
    pub fn handle_text_input(&mut self, text: &str) -> bool {
        if let Some(input_state) = &mut self.input_state {
            if input_state.is_editing {
                input_state.insert_text(text);
                return true;
            }
        }
        false
    }
    
    /// Handle key press for input elements
    pub fn handle_key_press(&mut self, key: &str, shift_pressed: bool) -> bool {
        if let Some(input_state) = &mut self.input_state {
            match key {
                "Backspace" => {
                    input_state.delete_char_backward();
                    return true;
                }
                "Delete" => {
                    input_state.delete_char_forward();
                    return true;
                }
                "ArrowLeft" => {
                    input_state.move_cursor_left(shift_pressed);
                    return true;
                }
                "ArrowRight" => {
                    input_state.move_cursor_right(shift_pressed);
                    return true;
                }
                "Home" => {
                    input_state.move_cursor_to_start(shift_pressed);
                    return true;
                }
                "End" => {
                    input_state.move_cursor_to_end(shift_pressed);
                    return true;
                }
                "Enter" => {
                    // For single-line inputs, Enter typically confirms/submits
                    if input_state.input_type != InputType::Textarea {
                        input_state.set_mode(InputMode::Focused);
                        return true;
                    } else {
                        // For textarea, Enter adds a newline
                        input_state.insert_text("\n");
                        return true;
                    }
                }
                _ => {}
            }
        }
        false
    }
}