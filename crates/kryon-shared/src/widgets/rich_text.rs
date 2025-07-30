use super::{KryonWidget, EventResult, LayoutConstraints, PropertyBag};
use crate::elements::ElementId;
use crate::types::{InputEvent, LayoutResult, KeyCode, MouseButton};
use crate::render::RenderCommand;
use glam::{Vec2, Vec4};
use serde::{Serialize, Deserialize};
use anyhow::Result;
use std::collections::HashMap;

/// Rich text editor widget with formatting, markdown support, and WYSIWYG editing
#[derive(Default)]
pub struct RichTextWidget;

impl RichTextWidget {
    pub fn new() -> Self {
        Self
    }
}

/// State for the rich text editor widget
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct RichTextState {
    /// Document content as rich text nodes
    pub document: RichTextDocument,
    /// Current cursor position in the document
    pub cursor: CursorPosition,
    /// Current text selection (if any)
    pub selection: Option<TextSelection>,
    /// Current formatting state for new text
    pub current_format: TextFormat,
    /// Editor mode (WYSIWYG or Markdown)
    pub editor_mode: EditorMode,
    /// Raw markdown content (when in markdown mode)
    pub markdown_content: String,
    /// Whether the editor is focused
    pub is_focused: bool,
    /// Whether the editor is in read-only mode
    pub is_readonly: bool,
    /// Undo/redo history
    pub history: EditHistory,
    /// Scroll position for large documents
    pub scroll_offset: Vec2,
    /// Whether to show formatting toolbar
    pub show_toolbar: bool,
    /// Current toolbar state
    pub toolbar_state: ToolbarState,
    /// Search state (if search is active)
    pub search_state: Option<SearchState>,
    /// Auto-save state
    pub auto_save_state: AutoSaveState,
}

impl Default for RichTextState {
    fn default() -> Self {
        Self {
            document: RichTextDocument::new(),
            cursor: CursorPosition::start(),
            selection: None,
            current_format: TextFormat::default(),
            editor_mode: EditorMode::WYSIWYG,
            markdown_content: String::new(),
            is_focused: false,
            is_readonly: false,
            history: EditHistory::new(),
            scroll_offset: Vec2::ZERO,
            show_toolbar: true,
            toolbar_state: ToolbarState::default(),
            search_state: None,
            auto_save_state: AutoSaveState::default(),
        }
    }
}

/// Rich text document structure
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct RichTextDocument {
    /// Document root node
    pub root: RichTextNode,
    /// Document metadata
    pub metadata: HashMap<String, serde_json::Value>,
}

impl RichTextDocument {
    pub fn new() -> Self {
        Self {
            root: RichTextNode::Block(BlockNode {
                block_type: BlockType::Document,
                attributes: HashMap::new(),
                children: vec![
                    RichTextNode::Block(BlockNode {
                        block_type: BlockType::Paragraph,
                        attributes: HashMap::new(),
                        children: vec![],
                    })
                ],
            }),
            metadata: HashMap::new(),
        }
    }
    
    pub fn from_markdown(markdown: &str) -> Self {
        // Simplified markdown parsing - in practice would use a proper markdown parser
        let mut document = Self::new();
        // Parse markdown and populate document structure
        document
    }
    
    pub fn to_markdown(&self) -> String {
        // Convert rich text document to markdown
        self.node_to_markdown(&self.root)
    }
    
    pub fn to_html(&self) -> String {
        // Convert rich text document to HTML
        self.node_to_html(&self.root)
    }
    
    fn node_to_markdown(&self, node: &RichTextNode) -> String {
        match node {
            RichTextNode::Block(block) => self.block_to_markdown(block),
            RichTextNode::Inline(inline) => self.inline_to_markdown(inline),
            RichTextNode::Text(text) => text.content.clone(),
        }
    }
    
    fn block_to_markdown(&self, block: &BlockNode) -> String {
        match block.block_type {
            BlockType::Paragraph => {
                let content: String = block.children.iter()
                    .map(|child| self.node_to_markdown(child))
                    .collect();
                format!("{}\n\n", content)
            }
            BlockType::Heading(level) => {
                let prefix = "#".repeat(level as usize);
                let content: String = block.children.iter()
                    .map(|child| self.node_to_markdown(child))
                    .collect();
                format!("{} {}\n\n", prefix, content)
            }
            BlockType::List(list_type) => {
                let mut result = String::new();
                for (i, child) in block.children.iter().enumerate() {
                    let prefix = match list_type {
                        ListType::Unordered => "- ",
                        ListType::Ordered => &format!("{}. ", i + 1),
                    };
                    result.push_str(&format!("{}{}\n", prefix, self.node_to_markdown(child)));
                }
                result.push('\n');
                result
            }
            BlockType::CodeBlock => {
                let language = block.attributes.get("language")
                    .and_then(|v| v.as_str())
                    .unwrap_or("");
                let content: String = block.children.iter()
                    .map(|child| self.node_to_markdown(child))
                    .collect();
                format!("```{}\n{}\n```\n\n", language, content)
            }
            BlockType::Quote => {
                let content: String = block.children.iter()
                    .map(|child| self.node_to_markdown(child))
                    .collect();
                format!("> {}\n\n", content)
            }
            _ => String::new(),
        }
    }
    
    fn inline_to_markdown(&self, inline: &InlineNode) -> String {
        let content: String = inline.children.iter()
            .map(|child| self.node_to_markdown(child))
            .collect();
        
        match inline.inline_type {
            InlineType::Bold => format!("**{}**", content),
            InlineType::Italic => format!("*{}*", content),
            InlineType::Code => format!("`{}`", content),
            InlineType::Link => {
                let url = inline.attributes.get("href")
                    .and_then(|v| v.as_str())
                    .unwrap_or("");
                format!("[{}]({})", content, url)
            }
            InlineType::Strikethrough => format!("~~{}~~", content),
            InlineType::Underline => content, // Markdown doesn't have native underline
            InlineType::Highlight => format!("=={}", content),
            InlineType::Superscript => format!("^{}", content),
            InlineType::Subscript => format!("_{}", content),
        }
    }
    
    fn node_to_html(&self, node: &RichTextNode) -> String {
        // Similar to markdown conversion but for HTML
        match node {
            RichTextNode::Text(text) => html_escape::encode_text(&text.content).to_string(),
            RichTextNode::Inline(inline) => self.inline_to_html(inline),
            RichTextNode::Block(block) => self.block_to_html(block),
        }
    }
    
    fn block_to_html(&self, block: &BlockNode) -> String {
        let content: String = block.children.iter()
            .map(|child| self.node_to_html(child))
            .collect();
        
        match block.block_type {
            BlockType::Paragraph => format!("<p>{}</p>", content),
            BlockType::Heading(level) => format!("<h{}>{}</h{}>", level, content, level),
            BlockType::List(ListType::Unordered) => format!("<ul>{}</ul>", content),
            BlockType::List(ListType::Ordered) => format!("<ol>{}</ol>", content),
            BlockType::ListItem => format!("<li>{}</li>", content),
            BlockType::CodeBlock => {
                let language = block.attributes.get("language")
                    .and_then(|v| v.as_str())
                    .unwrap_or("");
                format!("<pre><code class=\"language-{}\">{}</code></pre>", language, content)
            }
            BlockType::Quote => format!("<blockquote>{}</blockquote>", content),
            BlockType::Document => content,
        }
    }
    
    fn inline_to_html(&self, inline: &InlineNode) -> String {
        let content: String = inline.children.iter()
            .map(|child| self.node_to_html(child))
            .collect();
        
        match inline.inline_type {
            InlineType::Bold => format!("<strong>{}</strong>", content),
            InlineType::Italic => format!("<em>{}</em>", content),
            InlineType::Code => format!("<code>{}</code>", content),
            InlineType::Link => {
                let url = inline.attributes.get("href")
                    .and_then(|v| v.as_str())
                    .unwrap_or("");
                format!("<a href=\"{}\">{}</a>", url, content)
            }
            InlineType::Strikethrough => format!("<del>{}</del>", content),
            InlineType::Underline => format!("<u>{}</u>", content),
            InlineType::Highlight => format!("<mark>{}</mark>", content),
            InlineType::Superscript => format!("<sup>{}</sup>", content),
            InlineType::Subscript => format!("<sub>{}</sub>", content),
        }
    }
}

/// Rich text node types
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum RichTextNode {
    Block(BlockNode),
    Inline(InlineNode),
    Text(TextNode),
}

/// Block-level nodes (paragraphs, headings, lists, etc.)
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct BlockNode {
    pub block_type: BlockType,
    pub attributes: HashMap<String, serde_json::Value>,
    pub children: Vec<RichTextNode>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum BlockType {
    Document,
    Paragraph,
    Heading(u8), // 1-6
    List(ListType),
    ListItem,
    CodeBlock,
    Quote,
    Table,
    TableRow,
    TableCell,
    HorizontalRule,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum ListType {
    Unordered,
    Ordered,
}

/// Inline nodes (bold, italic, links, etc.)
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct InlineNode {
    pub inline_type: InlineType,
    pub attributes: HashMap<String, serde_json::Value>,
    pub children: Vec<RichTextNode>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum InlineType {
    Bold,
    Italic,
    Code,
    Link,
    Strikethrough,
    Underline,
    Highlight,
    Superscript,
    Subscript,
}

/// Text leaf nodes
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct TextNode {
    pub content: String,
    pub format: TextFormat,
}

/// Text formatting options
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct TextFormat {
    pub bold: bool,
    pub italic: bool,
    pub underline: bool,
    pub strikethrough: bool,
    pub font_size: Option<f32>,
    pub font_family: Option<String>,
    pub color: Option<Vec4>,
    pub background_color: Option<Vec4>,
}

impl Default for TextFormat {
    fn default() -> Self {
        Self {
            bold: false,
            italic: false,
            underline: false,
            strikethrough: false,
            font_size: None,
            font_family: None,
            color: None,
            background_color: None,
        }
    }
}

/// Cursor position in the document
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct CursorPosition {
    /// Path to the node containing the cursor
    pub node_path: Vec<usize>,
    /// Character offset within the text node
    pub offset: usize,
}

impl CursorPosition {
    pub fn start() -> Self {
        Self {
            node_path: vec![0, 0], // Document -> First paragraph
            offset: 0,
        }
    }
}

/// Text selection range
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct TextSelection {
    pub start: CursorPosition,
    pub end: CursorPosition,
}

/// Editor modes
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum EditorMode {
    WYSIWYG,
    Markdown,
    SourceCode,
}

/// Edit history for undo/redo
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct EditHistory {
    pub operations: Vec<EditOperation>,
    pub current_index: usize,
    pub max_history: usize,
}

impl EditHistory {
    pub fn new() -> Self {
        Self {
            operations: Vec::new(),
            current_index: 0,
            max_history: 100,
        }
    }
    
    pub fn add_operation(&mut self, operation: EditOperation) {
        // Remove any operations after current index
        self.operations.truncate(self.current_index);
        
        // Add new operation
        self.operations.push(operation);
        self.current_index = self.operations.len();
        
        // Limit history size
        if self.operations.len() > self.max_history {
            self.operations.remove(0);
            self.current_index -= 1;
        }
    }
    
    pub fn can_undo(&self) -> bool {
        self.current_index > 0
    }
    
    pub fn can_redo(&self) -> bool {
        self.current_index < self.operations.len()
    }
}

/// Edit operations for undo/redo
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum EditOperation {
    Insert {
        position: CursorPosition,
        content: String,
        format: TextFormat,
    },
    Delete {
        position: CursorPosition,
        content: String,
        length: usize,
    },
    Format {
        selection: TextSelection,
        old_format: TextFormat,
        new_format: TextFormat,
    },
    ReplaceNode {
        node_path: Vec<usize>,
        old_node: RichTextNode,
        new_node: RichTextNode,
    },
}

/// Toolbar state
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct ToolbarState {
    pub visible_tools: Vec<ToolbarTool>,
    pub active_tools: Vec<ToolbarTool>,
    pub dropdown_open: Option<ToolbarTool>,
}

impl Default for ToolbarState {
    fn default() -> Self {
        Self {
            visible_tools: vec![
                ToolbarTool::Bold,
                ToolbarTool::Italic,
                ToolbarTool::Underline,
                ToolbarTool::Strikethrough,
                ToolbarTool::Separator,
                ToolbarTool::Heading,
                ToolbarTool::List,
                ToolbarTool::Link,
                ToolbarTool::Separator,
                ToolbarTool::Undo,
                ToolbarTool::Redo,
            ],
            active_tools: Vec::new(),
            dropdown_open: None,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum ToolbarTool {
    Bold,
    Italic,
    Underline,
    Strikethrough,
    Code,
    Heading,
    List,
    Link,
    Image,
    Quote,
    CodeBlock,
    Table,
    HorizontalRule,
    TextColor,
    BackgroundColor,
    FontSize,
    FontFamily,
    Undo,
    Redo,
    Search,
    Separator,
}

/// Search state
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct SearchState {
    pub query: String,
    pub replace_text: String,
    pub case_sensitive: bool,
    pub whole_words: bool,
    pub regex: bool,
    pub current_match: usize,
    pub total_matches: usize,
    pub matches: Vec<TextSelection>,
}

/// Auto-save state
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct AutoSaveState {
    pub enabled: bool,
    pub interval_ms: u64,
    pub last_save_time: u64,
    pub has_unsaved_changes: bool,
}

impl Default for AutoSaveState {
    fn default() -> Self {
        Self {
            enabled: false,
            interval_ms: 30000, // 30 seconds
            last_save_time: 0,
            has_unsaved_changes: false,
        }
    }
}

/// Configuration for the rich text editor widget
#[derive(Debug, Clone)]
pub struct RichTextConfig {
    /// Background color
    pub background_color: Vec4,
    /// Text color
    pub text_color: Vec4,
    /// Selection background color
    pub selection_background: Vec4,
    /// Cursor color
    pub cursor_color: Vec4,
    /// Border color
    pub border_color: Vec4,
    /// Toolbar background color
    pub toolbar_background: Vec4,
    /// Font family
    pub font_family: String,
    /// Base font size
    pub font_size: f32,
    /// Line height multiplier
    pub line_height: f32,
    /// Paragraph spacing
    pub paragraph_spacing: f32,
    /// Editor padding
    pub padding: f32,
    /// Whether to show the toolbar
    pub show_toolbar: bool,
    /// Toolbar height
    pub toolbar_height: f32,
    /// Whether the editor is resizable
    pub resizable: bool,
    /// Minimum editor height
    pub min_height: f32,
    /// Maximum editor height (0 = unlimited)
    pub max_height: f32,
    /// Enable spell checking
    pub spell_check: bool,
    /// Enable auto-save
    pub auto_save: bool,
    /// Auto-save interval in milliseconds
    pub auto_save_interval: u64,
    /// Read-only mode
    pub readonly: bool,
    /// Placeholder text when empty
    pub placeholder: String,
    /// Maximum document length (0 = unlimited)
    pub max_length: usize,
    /// Allowed formatting options
    pub allowed_formats: Vec<InlineType>,
    /// Allowed block types
    pub allowed_blocks: Vec<BlockType>,
    /// Enable markdown shortcuts
    pub markdown_shortcuts: bool,
    /// Enable drag and drop
    pub drag_drop: bool,
    /// Enable image upload
    pub image_upload: bool,
    /// Image upload handler URL
    pub image_upload_url: Option<String>,
}

impl Default for RichTextConfig {
    fn default() -> Self {
        Self {
            background_color: Vec4::new(1.0, 1.0, 1.0, 1.0),
            text_color: Vec4::new(0.2, 0.2, 0.2, 1.0),
            selection_background: Vec4::new(0.7, 0.8, 1.0, 0.5),
            cursor_color: Vec4::new(0.0, 0.0, 0.0, 1.0),
            border_color: Vec4::new(0.8, 0.8, 0.8, 1.0),
            toolbar_background: Vec4::new(0.95, 0.95, 0.95, 1.0),
            font_family: "sans-serif".to_string(),
            font_size: 14.0,
            line_height: 1.5,
            paragraph_spacing: 12.0,
            padding: 12.0,
            show_toolbar: true,
            toolbar_height: 40.0,
            resizable: false,
            min_height: 100.0,
            max_height: 0.0,
            spell_check: true,
            auto_save: false,
            auto_save_interval: 30000,
            readonly: false,
            placeholder: "Start typing...".to_string(),
            max_length: 0,
            allowed_formats: vec![
                InlineType::Bold,
                InlineType::Italic,
                InlineType::Underline,
                InlineType::Strikethrough,
                InlineType::Code,
                InlineType::Link,
            ],
            allowed_blocks: vec![
                BlockType::Paragraph,
                BlockType::Heading(1),
                BlockType::Heading(2),
                BlockType::Heading(3),
                BlockType::List(ListType::Unordered),
                BlockType::List(ListType::Ordered),
                BlockType::Quote,
                BlockType::CodeBlock,
            ],
            markdown_shortcuts: true,
            drag_drop: true,
            image_upload: false,
            image_upload_url: None,
        }
    }
}

impl From<&PropertyBag> for RichTextConfig {
    fn from(props: &PropertyBag) -> Self {
        let mut config = Self::default();
        
        if let Some(size) = props.get_f32("font_size") {
            config.font_size = size;
        }
        
        if let Some(family) = props.get_string("font_family") {
            config.font_family = family;
        }
        
        if let Some(height) = props.get_f32("line_height") {
            config.line_height = height;
        }
        
        if let Some(padding) = props.get_f32("padding") {
            config.padding = padding;
        }
        
        if let Some(show_toolbar) = props.get_bool("show_toolbar") {
            config.show_toolbar = show_toolbar;
        }
        
        if let Some(readonly) = props.get_bool("readonly") {
            config.readonly = readonly;
        }
        
        if let Some(placeholder) = props.get_string("placeholder") {
            config.placeholder = placeholder;
        }
        
        if let Some(max_length) = props.get_i32("max_length") {
            config.max_length = max_length.max(0) as usize;
        }
        
        if let Some(spell_check) = props.get_bool("spell_check") {
            config.spell_check = spell_check;
        }
        
        if let Some(auto_save) = props.get_bool("auto_save") {
            config.auto_save = auto_save;
        }
        
        if let Some(markdown_shortcuts) = props.get_bool("markdown_shortcuts") {
            config.markdown_shortcuts = markdown_shortcuts;
        }
        
        if let Some(min_height) = props.get_f32("min_height") {
            config.min_height = min_height;
        }
        
        if let Some(max_height) = props.get_f32("max_height") {
            config.max_height = max_height;
        }
        
        config
    }
}

impl KryonWidget for RichTextWidget {
    const WIDGET_NAME: &'static str = "rich-text";
    type State = RichTextState;
    type Config = RichTextConfig;
    
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
        
        // Editor background
        commands.push(RenderCommand::DrawRect {
            position: layout.position,
            size: layout.size,
            color: config.background_color,
            border_radius: 4.0,
            border_width: 1.0,
            border_color: config.border_color,
        });
        
        let mut content_y = layout.position.y;
        
        // Toolbar
        if config.show_toolbar && state.show_toolbar {
            self.render_toolbar(&mut commands, state, config, layout, content_y)?;
            content_y += config.toolbar_height;
        }
        
        // Editor content area
        let content_area = Vec2::new(
            layout.size.x,
            layout.size.y - (content_y - layout.position.y),
        );
        
        // Content background
        commands.push(RenderCommand::DrawRect {
            position: Vec2::new(layout.position.x, content_y),
            size: content_area,
            color: config.background_color,
            border_radius: 0.0,
            border_width: 0.0,
            border_color: Vec4::ZERO,
        });
        
        // Render document content
        self.render_document(&mut commands, state, config, Vec2::new(layout.position.x, content_y), content_area)?;
        
        // Cursor (if focused)
        if state.is_focused && !config.readonly {
            self.render_cursor(&mut commands, state, config, Vec2::new(layout.position.x, content_y))?;
        }
        
        // Selection (if any)
        if let Some(selection) = &state.selection {
            self.render_selection(&mut commands, state, config, selection, Vec2::new(layout.position.x, content_y))?;
        }
        
        // Search highlights
        if let Some(search_state) = &state.search_state {
            self.render_search_highlights(&mut commands, state, config, search_state, Vec2::new(layout.position.x, content_y))?;
        }
        
        // Placeholder text (if document is empty)
        if self.is_document_empty(&state.document) && !config.placeholder.is_empty() {
            commands.push(RenderCommand::DrawText {
                text: config.placeholder.clone(),
                position: Vec2::new(layout.position.x + config.padding, content_y + config.padding),
                font_size: config.font_size,
                color: Vec4::new(0.6, 0.6, 0.6, 1.0),
                max_width: Some(content_area.x - 2.0 * config.padding),
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
                let content_y = layout.position.y + if config.show_toolbar { config.toolbar_height } else { 0.0 };
                
                // Check toolbar clicks
                if config.show_toolbar && position.y < content_y {
                    return self.handle_toolbar_click(state, config, *position, layout);
                }
                
                // Set focus
                state.is_focused = true;
                
                // Position cursor
                if let Some(cursor_pos) = self.position_to_cursor(*position, state, config, layout) {
                    state.cursor = cursor_pos;
                    state.selection = None;
                }
                
                Ok(EventResult::HandledWithRedraw)
            }
            
            InputEvent::MouseDrag { position, .. } => {
                if state.is_focused {
                    // Create selection
                    if let Some(end_pos) = self.position_to_cursor(*position, state, config, layout) {
                        state.selection = Some(TextSelection {
                            start: state.cursor.clone(),
                            end: end_pos,
                        });
                    }
                    return Ok(EventResult::HandledWithRedraw);
                }
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::KeyPress { key, modifiers } => {
                if !state.is_focused || config.readonly {
                    return Ok(EventResult::NotHandled);
                }
                
                match key {
                    KeyCode::Backspace => {
                        self.handle_backspace(state);
                        Ok(EventResult::HandledWithRedraw)
                    }
                    KeyCode::Delete => {
                        self.handle_delete(state);
                        Ok(EventResult::HandledWithRedraw)
                    }
                    KeyCode::Enter => {
                        self.handle_enter(state, config);
                        Ok(EventResult::HandledWithRedraw)
                    }
                    KeyCode::Tab => {
                        if !config.readonly {
                            self.handle_tab(state, modifiers.shift);
                            Ok(EventResult::HandledWithRedraw)
                        } else {
                            Ok(EventResult::NotHandled)
                        }
                    }
                    KeyCode::ArrowLeft => {
                        self.move_cursor_left(state, modifiers.shift);
                        Ok(EventResult::HandledWithRedraw)
                    }
                    KeyCode::ArrowRight => {
                        self.move_cursor_right(state, modifiers.shift);
                        Ok(EventResult::HandledWithRedraw)
                    }
                    KeyCode::ArrowUp => {
                        self.move_cursor_up(state, modifiers.shift);
                        Ok(EventResult::HandledWithRedraw)
                    }
                    KeyCode::ArrowDown => {
                        self.move_cursor_down(state, modifiers.shift);
                        Ok(EventResult::HandledWithRedraw)
                    }
                    KeyCode::Home => {
                        self.move_cursor_home(state, modifiers.shift, modifiers.ctrl);
                        Ok(EventResult::HandledWithRedraw)
                    }
                    KeyCode::End => {
                        self.move_cursor_end(state, modifiers.shift, modifiers.ctrl);
                        Ok(EventResult::HandledWithRedraw)
                    }
                    _ => {
                        // Handle keyboard shortcuts
                        if modifiers.ctrl {
                            match key {
                                KeyCode::A => {
                                    self.select_all(state);
                                    Ok(EventResult::HandledWithRedraw)
                                }
                                KeyCode::C => {
                                    self.copy_selection(state);
                                    Ok(EventResult::Handled)
                                }
                                KeyCode::X => {
                                    self.cut_selection(state);
                                    Ok(EventResult::HandledWithRedraw)
                                }
                                KeyCode::V => {
                                    // In a real implementation, would paste from clipboard
                                    Ok(EventResult::HandledWithRedraw)
                                }
                                KeyCode::Z => {
                                    if modifiers.shift {
                                        self.redo(state);
                                    } else {
                                        self.undo(state);
                                    }
                                    Ok(EventResult::HandledWithRedraw)
                                }
                                KeyCode::B => {
                                    self.toggle_format(state, InlineType::Bold);
                                    Ok(EventResult::HandledWithRedraw)
                                }
                                KeyCode::I => {
                                    self.toggle_format(state, InlineType::Italic);
                                    Ok(EventResult::HandledWithRedraw)
                                }
                                KeyCode::U => {
                                    self.toggle_format(state, InlineType::Underline);
                                    Ok(EventResult::HandledWithRedraw)
                                }
                                _ => Ok(EventResult::NotHandled),
                            }
                        } else {
                            Ok(EventResult::NotHandled)
                        }
                    }
                }
            }
            
            InputEvent::TextInput { text } => {
                if state.is_focused && !config.readonly {
                    self.insert_text(state, text);
                    return Ok(EventResult::HandledWithRedraw);
                }
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::Focus => {
                state.is_focused = true;
                Ok(EventResult::HandledWithRedraw)
            }
            
            InputEvent::Blur => {
                state.is_focused = false;
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
            min_width: Some(200.0),
            max_width: None,
            min_height: Some(config.min_height),
            max_height: if config.max_height > 0.0 { Some(config.max_height) } else { None },
            aspect_ratio: None,
        })
    }
}

impl RichTextWidget {
    fn render_toolbar(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &RichTextState,
        config: &RichTextConfig,
        layout: &LayoutResult,
        y: f32,
    ) -> Result<()> {
        // Toolbar background
        commands.push(RenderCommand::DrawRect {
            position: Vec2::new(layout.position.x, y),
            size: Vec2::new(layout.size.x, config.toolbar_height),
            color: config.toolbar_background,
            border_radius: 0.0,
            border_width: 0.0,
            border_color: Vec4::ZERO,
        });
        
        let mut x = layout.position.x + 8.0;
        let button_size = 32.0;
        let button_spacing = 4.0;
        
        for tool in &state.toolbar_state.visible_tools {
            match tool {
                ToolbarTool::Separator => {
                    // Draw separator line
                    commands.push(RenderCommand::DrawLine {
                        start: Vec2::new(x + 4.0, y + 8.0),
                        end: Vec2::new(x + 4.0, y + config.toolbar_height - 8.0),
                        color: config.border_color,
                        width: 1.0,
                    });
                    x += 12.0;
                }
                _ => {
                    let is_active = state.toolbar_state.active_tools.contains(tool);
                    
                    // Button background
                    if is_active {
                        commands.push(RenderCommand::DrawRect {
                            position: Vec2::new(x, y + 4.0),
                            size: Vec2::new(button_size, button_size),
                            color: Vec4::new(0.8, 0.8, 0.9, 1.0),
                            border_radius: 4.0,
                            border_width: 1.0,
                            border_color: config.border_color,
                        });
                    }
                    
                    // Button icon/text
                    let icon = self.get_tool_icon(tool);
                    commands.push(RenderCommand::DrawText {
                        text: icon,
                        position: Vec2::new(x + 8.0, y + 12.0),
                        font_size: 16.0,
                        color: config.text_color,
                        max_width: None,
                    });
                    
                    x += button_size + button_spacing;
                }
            }
        }
        
        Ok(())
    }
    
    fn render_document(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &RichTextState,
        config: &RichTextConfig,
        position: Vec2,
        size: Vec2,
    ) -> Result<()> {
        let content_pos = position + Vec2::new(config.padding, config.padding);
        let content_size = size - Vec2::new(2.0 * config.padding, 2.0 * config.padding);
        
        // Render document nodes
        self.render_node(commands, &state.document.root, state, config, content_pos, content_size, 0.0)?;
        
        Ok(())
    }
    
    fn render_node(
        &self,
        commands: &mut Vec<RenderCommand>,
        node: &RichTextNode,
        state: &RichTextState,
        config: &RichTextConfig,
        position: Vec2,
        _size: Vec2,
        mut y_offset: f32,
    ) -> Result<f32> {
        match node {
            RichTextNode::Block(block) => {
                y_offset = self.render_block_node(commands, block, state, config, position, y_offset)?;
            }
            RichTextNode::Inline(inline) => {
                y_offset = self.render_inline_node(commands, inline, state, config, position, y_offset)?;
            }
            RichTextNode::Text(text) => {
                y_offset = self.render_text_node(commands, text, state, config, position, y_offset)?;
            }
        }
        
        Ok(y_offset)
    }
    
    fn render_block_node(
        &self,
        commands: &mut Vec<RenderCommand>,
        block: &BlockNode,
        state: &RichTextState,
        config: &RichTextConfig,
        position: Vec2,
        mut y_offset: f32,
    ) -> Result<f32> {
        match block.block_type {
            BlockType::Paragraph => {
                for child in &block.children {
                    y_offset = self.render_node(commands, child, state, config, position, Vec2::ZERO, y_offset)?;
                }
                y_offset += config.paragraph_spacing;
            }
            BlockType::Heading(level) => {
                let font_size = config.font_size * (2.0 - (level as f32 * 0.2));
                
                for child in &block.children {
                    // Render with larger font size for headings
                    if let RichTextNode::Text(text) = child {
                        commands.push(RenderCommand::DrawText {
                            text: text.content.clone(),
                            position: Vec2::new(position.x, position.y + y_offset),
                            font_size,
                            color: config.text_color,
                            max_width: None,
                        });
                        y_offset += font_size * config.line_height;
                    }
                }
                y_offset += config.paragraph_spacing;
            }
            BlockType::List(_list_type) => {
                for (i, child) in block.children.iter().enumerate() {
                    // List item marker
                    let marker = format!("{}. ", i + 1); // Simplified - would handle unordered lists too
                    commands.push(RenderCommand::DrawText {
                        text: marker,
                        position: Vec2::new(position.x, position.y + y_offset),
                        font_size: config.font_size,
                        color: config.text_color,
                        max_width: None,
                    });
                    
                    // List item content
                    y_offset = self.render_node(commands, child, state, config, 
                        Vec2::new(position.x + 30.0, position.y), Vec2::ZERO, y_offset)?;
                }
            }
            BlockType::CodeBlock => {
                // Code block background
                let block_height = block.children.len() as f32 * config.font_size * config.line_height;
                commands.push(RenderCommand::DrawRect {
                    position: Vec2::new(position.x - 8.0, position.y + y_offset - 4.0),
                    size: Vec2::new(400.0, block_height + 8.0), // Simplified width
                    color: Vec4::new(0.95, 0.95, 0.95, 1.0),
                    border_radius: 4.0,
                    border_width: 1.0,
                    border_color: config.border_color,
                });
                
                for child in &block.children {
                    if let RichTextNode::Text(text) = child {
                        commands.push(RenderCommand::DrawText {
                            text: text.content.clone(),
                            position: Vec2::new(position.x, position.y + y_offset),
                            font_size: config.font_size,
                            color: Vec4::new(0.2, 0.4, 0.2, 1.0), // Code color
                            max_width: None,
                        });
                        y_offset += config.font_size * config.line_height;
                    }
                }
                y_offset += config.paragraph_spacing;
            }
            BlockType::Quote => {
                // Quote bar
                commands.push(RenderCommand::DrawRect {
                    position: Vec2::new(position.x - 8.0, position.y + y_offset),
                    size: Vec2::new(4.0, 50.0), // Simplified height
                    color: Vec4::new(0.8, 0.8, 0.8, 1.0),
                    border_radius: 2.0,
                    border_width: 0.0,
                    border_color: Vec4::ZERO,
                });
                
                for child in &block.children {
                    y_offset = self.render_node(commands, child, state, config, 
                        Vec2::new(position.x + 12.0, position.y), Vec2::ZERO, y_offset)?;
                }
            }
            _ => {
                // Default block rendering
                for child in &block.children {
                    y_offset = self.render_node(commands, child, state, config, position, Vec2::ZERO, y_offset)?;
                }
            }
        }
        
        Ok(y_offset)
    }
    
    fn render_inline_node(
        &self,
        commands: &mut Vec<RenderCommand>,
        _inline: &InlineNode,
        state: &RichTextState,
        config: &RichTextConfig,
        position: Vec2,
        y_offset: f32,
    ) -> Result<f32> {
        // Inline rendering would modify text appearance but not layout
        // For now, just pass through to children
        Ok(y_offset)
    }
    
    fn render_text_node(
        &self,
        commands: &mut Vec<RenderCommand>,
        text: &TextNode,
        state: &RichTextState,
        config: &RichTextConfig,
        position: Vec2,
        y_offset: f32,
    ) -> Result<f32> {
        let font_size = text.format.font_size.unwrap_or(config.font_size);
        let color = text.format.color.unwrap_or(config.text_color);
        
        commands.push(RenderCommand::DrawText {
            text: text.content.clone(),
            position: Vec2::new(position.x, position.y + y_offset),
            font_size,
            color,
            max_width: None,
        });
        
        Ok(y_offset + font_size * config.line_height)
    }
    
    fn render_cursor(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &RichTextState,
        config: &RichTextConfig,
        position: Vec2,
    ) -> Result<()> {
        // Simplified cursor rendering - would need proper text measurement
        let cursor_x = position.x + config.padding + (state.cursor.offset as f32 * 8.0);
        let cursor_y = position.y + config.padding;
        
        commands.push(RenderCommand::DrawRect {
            position: Vec2::new(cursor_x, cursor_y),
            size: Vec2::new(2.0, config.font_size * config.line_height),
            color: config.cursor_color,
            border_radius: 0.0,
            border_width: 0.0,
            border_color: Vec4::ZERO,
        });
        
        Ok(())
    }
    
    fn render_selection(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &RichTextState,
        config: &RichTextConfig,
        selection: &TextSelection,
        position: Vec2,
    ) -> Result<()> {
        // Simplified selection rendering
        let start_x = position.x + config.padding + (selection.start.offset as f32 * 8.0);
        let end_x = position.x + config.padding + (selection.end.offset as f32 * 8.0);
        let selection_y = position.y + config.padding;
        
        commands.push(RenderCommand::DrawRect {
            position: Vec2::new(start_x, selection_y),
            size: Vec2::new(end_x - start_x, config.font_size * config.line_height),
            color: config.selection_background,
            border_radius: 0.0,
            border_width: 0.0,
            border_color: Vec4::ZERO,
        });
        
        Ok(())
    }
    
    fn render_search_highlights(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &RichTextState,
        config: &RichTextConfig,
        search_state: &SearchState,
        position: Vec2,
    ) -> Result<()> {
        // Render search matches
        for (i, selection) in search_state.matches.iter().enumerate() {
            let highlight_color = if i == search_state.current_match {
                Vec4::new(1.0, 0.8, 0.0, 0.8) // Current match - yellow
            } else {
                Vec4::new(1.0, 1.0, 0.0, 0.5) // Other matches - light yellow
            };
            
            let start_x = position.x + config.padding + (selection.start.offset as f32 * 8.0);
            let end_x = position.x + config.padding + (selection.end.offset as f32 * 8.0);
            let highlight_y = position.y + config.padding;
            
            commands.push(RenderCommand::DrawRect {
                position: Vec2::new(start_x, highlight_y),
                size: Vec2::new(end_x - start_x, config.font_size * config.line_height),
                color: highlight_color,
                border_radius: 2.0,
                border_width: 0.0,
                border_color: Vec4::ZERO,
            });
        }
        
        Ok(())
    }
    
    fn get_tool_icon(&self, tool: &ToolbarTool) -> String {
        match tool {
            ToolbarTool::Bold => "B".to_string(),
            ToolbarTool::Italic => "I".to_string(),
            ToolbarTool::Underline => "U".to_string(),
            ToolbarTool::Strikethrough => "S".to_string(),
            ToolbarTool::Code => "{ }".to_string(),
            ToolbarTool::Heading => "H".to_string(),
            ToolbarTool::List => "•".to_string(),
            ToolbarTool::Link => "🔗".to_string(),
            ToolbarTool::Image => "🖼".to_string(),
            ToolbarTool::Quote => """.to_string(),
            ToolbarTool::CodeBlock => "< />".to_string(),
            ToolbarTool::Table => "⊞".to_string(),
            ToolbarTool::HorizontalRule => "―".to_string(),
            ToolbarTool::TextColor => "A".to_string(),
            ToolbarTool::BackgroundColor => "⬛".to_string(),
            ToolbarTool::FontSize => "Tt".to_string(),
            ToolbarTool::FontFamily => "Font".to_string(),
            ToolbarTool::Undo => "↶".to_string(),
            ToolbarTool::Redo => "↷".to_string(),
            ToolbarTool::Search => "🔍".to_string(),
            ToolbarTool::Separator => "|".to_string(),
        }
    }
    
    fn handle_toolbar_click(
        &self,
        state: &mut RichTextState,
        config: &RichTextConfig,
        position: Vec2,
        layout: &LayoutResult,
    ) -> Result<EventResult> {
        // Simplified toolbar click handling
        let x_offset = position.x - layout.position.x - 8.0;
        let button_size = 32.0 + 4.0; // Size + spacing
        let button_index = (x_offset / button_size) as usize;
        
        if button_index < state.toolbar_state.visible_tools.len() {
            if let Some(tool) = state.toolbar_state.visible_tools.get(button_index) {
                return self.execute_toolbar_action(state, config, tool.clone());
            }
        }
        
        Ok(EventResult::NotHandled)
    }
    
    fn execute_toolbar_action(
        &self,
        state: &mut RichTextState,
        config: &RichTextConfig,
        tool: ToolbarTool,
    ) -> Result<EventResult> {
        match tool {
            ToolbarTool::Bold => {
                self.toggle_format(state, InlineType::Bold);
                Ok(EventResult::HandledWithRedraw)
            }
            ToolbarTool::Italic => {
                self.toggle_format(state, InlineType::Italic);
                Ok(EventResult::HandledWithRedraw)
            }
            ToolbarTool::Underline => {
                self.toggle_format(state, InlineType::Underline);
                Ok(EventResult::HandledWithRedraw)
            }
            ToolbarTool::Undo => {
                self.undo(state);
                Ok(EventResult::HandledWithRedraw)
            }
            ToolbarTool::Redo => {
                self.redo(state);
                Ok(EventResult::HandledWithRedraw)
            }
            _ => Ok(EventResult::NotHandled),
        }
    }
    
    // Text editing operations
    fn insert_text(&self, state: &mut RichTextState, text: &str) {
        // Simplified text insertion
        state.auto_save_state.has_unsaved_changes = true;
        
        // Add to history
        let operation = EditOperation::Insert {
            position: state.cursor.clone(),
            content: text.to_string(),
            format: state.current_format.clone(),
        };
        state.history.add_operation(operation);
    }
    
    fn handle_backspace(&self, state: &mut RichTextState) {
        if state.cursor.offset > 0 {
            state.cursor.offset -= 1;
            state.auto_save_state.has_unsaved_changes = true;
        }
    }
    
    fn handle_delete(&self, state: &mut RichTextState) {
        // Delete character at cursor position
        state.auto_save_state.has_unsaved_changes = true;
    }
    
    fn handle_enter(&self, state: &mut RichTextState, config: &RichTextConfig) {
        // Create new paragraph
        state.auto_save_state.has_unsaved_changes = true;
    }
    
    fn handle_tab(&self, state: &mut RichTextState, shift: bool) {
        if shift {
            // Decrease indentation
        } else {
            // Increase indentation or insert tab
            self.insert_text(state, "\t");
        }
    }
    
    // Cursor movement
    fn move_cursor_left(&self, state: &mut RichTextState, extend_selection: bool) {
        if state.cursor.offset > 0 {
            state.cursor.offset -= 1;
        }
        
        if !extend_selection {
            state.selection = None;
        }
    }
    
    fn move_cursor_right(&self, state: &mut RichTextState, extend_selection: bool) {
        state.cursor.offset += 1;
        
        if !extend_selection {
            state.selection = None;
        }
    }
    
    fn move_cursor_up(&self, state: &mut RichTextState, extend_selection: bool) {
        // Move cursor to line above
        if !extend_selection {
            state.selection = None;
        }
    }
    
    fn move_cursor_down(&self, state: &mut RichTextState, extend_selection: bool) {
        // Move cursor to line below
        if !extend_selection {
            state.selection = None;
        }
    }
    
    fn move_cursor_home(&self, state: &mut RichTextState, extend_selection: bool, ctrl: bool) {
        if ctrl {
            // Move to document start
            state.cursor = CursorPosition::start();
        } else {
            // Move to line start
            state.cursor.offset = 0;
        }
        
        if !extend_selection {
            state.selection = None;
        }
    }
    
    fn move_cursor_end(&self, state: &mut RichTextState, extend_selection: bool, ctrl: bool) {
        if ctrl {
            // Move to document end
        } else {
            // Move to line end
        }
        
        if !extend_selection {
            state.selection = None;
        }
    }
    
    // Selection operations
    fn select_all(&self, state: &mut RichTextState) {
        state.selection = Some(TextSelection {
            start: CursorPosition::start(),
            end: state.cursor.clone(), // Simplified - would be document end
        });
    }
    
    fn copy_selection(&self, state: &RichTextState) {
        // Copy selected text to clipboard
    }
    
    fn cut_selection(&self, state: &mut RichTextState) {
        // Cut selected text to clipboard
        if state.selection.is_some() {
            state.auto_save_state.has_unsaved_changes = true;
        }
    }
    
    // Formatting operations
    fn toggle_format(&self, state: &mut RichTextState, format_type: InlineType) {
        match format_type {
            InlineType::Bold => state.current_format.bold = !state.current_format.bold,
            InlineType::Italic => state.current_format.italic = !state.current_format.italic,
            InlineType::Underline => state.current_format.underline = !state.current_format.underline,
            InlineType::Strikethrough => state.current_format.strikethrough = !state.current_format.strikethrough,
            _ => {}
        }
        
        // Update toolbar state
        let tool = match format_type {
            InlineType::Bold => ToolbarTool::Bold,
            InlineType::Italic => ToolbarTool::Italic,
            InlineType::Underline => ToolbarTool::Underline,
            InlineType::Strikethrough => ToolbarTool::Strikethrough,
            _ => return,
        };
        
        if state.toolbar_state.active_tools.contains(&tool) {
            state.toolbar_state.active_tools.retain(|t| t != &tool);
        } else {
            state.toolbar_state.active_tools.push(tool);
        }
    }
    
    // History operations
    fn undo(&self, state: &mut RichTextState) {
        if state.history.can_undo() {
            state.history.current_index -= 1;
            // Apply inverse of operation
            state.auto_save_state.has_unsaved_changes = true;
        }
    }
    
    fn redo(&self, state: &mut RichTextState) {
        if state.history.can_redo() {
            // Apply operation
            state.history.current_index += 1;
            state.auto_save_state.has_unsaved_changes = true;
        }
    }
    
    // Utility functions
    fn position_to_cursor(
        &self,
        position: Vec2,
        state: &RichTextState,
        config: &RichTextConfig,
        layout: &LayoutResult,
    ) -> Option<CursorPosition> {
        // Simplified position to cursor conversion
        let content_y = layout.position.y + if config.show_toolbar { config.toolbar_height } else { 0.0 };
        let relative_x = position.x - layout.position.x - config.padding;
        let relative_y = position.y - content_y - config.padding;
        
        // Calculate approximate character offset
        let char_width = 8.0; // Simplified - would use proper text measurement
        let offset = (relative_x / char_width) as usize;
        
        Some(CursorPosition {
            node_path: vec![0, 0], // Simplified
            offset,
        })
    }
    
    fn is_document_empty(&self, document: &RichTextDocument) -> bool {
        // Check if document has any text content
        self.node_has_content(&document.root)
    }
    
    fn node_has_content(&self, node: &RichTextNode) -> bool {
        match node {
            RichTextNode::Text(text) => !text.content.trim().is_empty(),
            RichTextNode::Block(block) => block.children.iter().any(|child| self.node_has_content(child)),
            RichTextNode::Inline(inline) => inline.children.iter().any(|child| self.node_has_content(child)),
        }
    }
}

// Implement the factory trait
crate::impl_widget_factory!(RichTextWidget);

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_rich_text_creation() {
        let widget = RichTextWidget::new();
        let state = widget.create_state();
        
        assert!(!state.is_focused);
        assert!(state.selection.is_none());
        assert_eq!(state.editor_mode, EditorMode::WYSIWYG);
    }
    
    #[test]
    fn test_document_creation() {
        let doc = RichTextDocument::new();
        assert!(matches!(doc.root, RichTextNode::Block(_)));
    }
    
    #[test]
    fn test_text_format() {
        let mut format = TextFormat::default();
        assert!(!format.bold);
        assert!(!format.italic);
        
        format.bold = true;
        assert!(format.bold);
    }
    
    #[test]
    fn test_cursor_position() {
        let cursor = CursorPosition::start();
        assert_eq!(cursor.node_path, vec![0, 0]);
        assert_eq!(cursor.offset, 0);
    }
    
    #[test]
    fn test_edit_history() {
        let mut history = EditHistory::new();
        assert!(!history.can_undo());
        assert!(!history.can_redo());
        
        let operation = EditOperation::Insert {
            position: CursorPosition::start(),
            content: "test".to_string(),
            format: TextFormat::default(),
        };
        
        history.add_operation(operation);
        assert!(history.can_undo());
        assert!(!history.can_redo());
    }
    
    #[test]
    fn test_markdown_conversion() {
        let doc = RichTextDocument::new();
        let markdown = doc.to_markdown();
        assert!(markdown.contains('\n'));
        
        let html = doc.to_html();
        assert!(html.contains('<') || html.is_empty());
    }
}