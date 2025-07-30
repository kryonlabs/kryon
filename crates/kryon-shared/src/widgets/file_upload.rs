use super::{KryonWidget, EventResult, LayoutConstraints, PropertyBag};
use crate::elements::ElementId;
use crate::types::{InputEvent, LayoutResult, KeyCode, MouseButton};
use crate::render::RenderCommand;
use glam::{Vec2, Vec4};
use serde::{Serialize, Deserialize};
use anyhow::Result;
use std::collections::HashMap;

/// File upload widget with drag-and-drop, progress tracking, and validation
#[derive(Default)]
pub struct FileUploadWidget;

impl FileUploadWidget {
    pub fn new() -> Self {
        Self
    }
}

/// State for the file upload widget
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct FileUploadState {
    /// Currently selected/uploaded files
    pub files: Vec<UploadFile>,
    /// Whether drag-and-drop is currently active
    pub is_dragging: bool,
    /// Whether the widget is disabled
    pub is_disabled: bool,
    /// Current upload progress (0.0 to 1.0)
    pub upload_progress: f32,
    /// Upload status
    pub upload_status: UploadStatus,
    /// Error message if upload failed
    pub error_message: Option<String>,
    /// Maximum number of files allowed
    pub max_files: usize,
    /// Total size of all selected files
    pub total_size: u64,
    /// Whether currently hovering over drop area
    pub is_hover: bool,
    /// Preview mode for images
    pub show_previews: bool,
    /// Validation errors for individual files
    pub file_errors: HashMap<String, String>,
}

impl Default for FileUploadState {
    fn default() -> Self {
        Self {
            files: Vec::new(),
            is_dragging: false,
            is_disabled: false,
            upload_progress: 0.0,
            upload_status: UploadStatus::Idle,
            error_message: None,
            max_files: 10,
            total_size: 0,
            is_hover: false,
            show_previews: true,
            file_errors: HashMap::new(),
        }
    }
}

/// Individual file information
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct UploadFile {
    /// Original filename
    pub name: String,
    /// File size in bytes
    pub size: u64,
    /// MIME type
    pub mime_type: String,
    /// File extension
    pub extension: String,
    /// Upload progress for this file (0.0 to 1.0)
    pub progress: f32,
    /// Upload status for this file
    pub status: FileUploadStatus,
    /// Preview data URL (for images)
    pub preview_url: Option<String>,
    /// Unique file identifier
    pub id: String,
    /// Last modified timestamp
    pub last_modified: u64,
    /// Whether this file is valid
    pub is_valid: bool,
    /// Validation error message
    pub error: Option<String>,
}

impl UploadFile {
    pub fn new(name: String, size: u64, mime_type: String) -> Self {
        let extension = name.split('.').last().unwrap_or("").to_lowercase();
        let id = format!("{}-{}", name, chrono::Utc::now().timestamp_millis());
        
        Self {
            name,
            size,
            mime_type,
            extension,
            progress: 0.0,
            status: FileUploadStatus::Pending,
            preview_url: None,
            id,
            last_modified: 0,
            is_valid: true,
            error: None,
        }
    }
    
    pub fn is_image(&self) -> bool {
        self.mime_type.starts_with("image/")
    }
    
    pub fn is_video(&self) -> bool {
        self.mime_type.starts_with("video/")
    }
    
    pub fn is_audio(&self) -> bool {
        self.mime_type.starts_with("audio/")
    }
    
    pub fn format_size(&self) -> String {
        let size = self.size as f64;
        if size < 1024.0 {
            format!("{} B", size)
        } else if size < 1024.0 * 1024.0 {
            format!("{:.1} KB", size / 1024.0)
        } else if size < 1024.0 * 1024.0 * 1024.0 {
            format!("{:.1} MB", size / (1024.0 * 1024.0))
        } else {
            format!("{:.1} GB", size / (1024.0 * 1024.0 * 1024.0))
        }
    }
}

/// Overall upload status
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum UploadStatus {
    Idle,
    Uploading,
    Success,
    Error,
    Cancelled,
}

/// Individual file upload status
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum FileUploadStatus {
    Pending,
    Uploading,
    Success,
    Error,
    Cancelled,
}

/// File validation rules
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct FileValidation {
    /// Maximum file size in bytes
    pub max_size: Option<u64>,
    /// Minimum file size in bytes
    pub min_size: Option<u64>,
    /// Allowed file extensions
    pub allowed_extensions: Vec<String>,
    /// Allowed MIME types
    pub allowed_mime_types: Vec<String>,
    /// Maximum total size of all files
    pub max_total_size: Option<u64>,
    /// Custom validation function name
    pub custom_validator: Option<String>,
}

impl Default for FileValidation {
    fn default() -> Self {
        Self {
            max_size: Some(10 * 1024 * 1024), // 10MB
            min_size: None,
            allowed_extensions: Vec::new(),
            allowed_mime_types: Vec::new(),
            max_total_size: Some(100 * 1024 * 1024), // 100MB
            custom_validator: None,
        }
    }
}

/// Configuration for the file upload widget
#[derive(Debug, Clone)]
pub struct FileUploadConfig {
    /// Background color
    pub background_color: Vec4,
    /// Text color
    pub text_color: Vec4,
    /// Border color
    pub border_color: Vec4,
    /// Drop zone border color when dragging
    pub drag_border_color: Vec4,
    /// Drop zone background when dragging
    pub drag_background_color: Vec4,
    /// Progress bar color
    pub progress_color: Vec4,
    /// Error color
    pub error_color: Vec4,
    /// Success color
    pub success_color: Vec4,
    /// Font size
    pub font_size: f32,
    /// Border radius
    pub border_radius: f32,
    /// Drop zone text
    pub drop_text: String,
    /// Browse button text
    pub browse_text: String,
    /// Multiple file selection
    pub multiple: bool,
    /// Enable drag-and-drop
    pub drag_drop: bool,
    /// Show file previews
    pub show_previews: bool,
    /// Show upload progress
    pub show_progress: bool,
    /// Auto-upload on file selection
    pub auto_upload: bool,
    /// File validation rules
    pub validation: FileValidation,
    /// Upload endpoint URL
    pub upload_url: Option<String>,
    /// Additional upload parameters
    pub upload_params: HashMap<String, String>,
    /// Upload timeout in seconds
    pub upload_timeout: u64,
    /// Chunk size for large file uploads
    pub chunk_size: Option<u64>,
    /// Preview image size
    pub preview_size: f32,
    /// Maximum number of files to display
    pub max_display_files: usize,
}

impl Default for FileUploadConfig {
    fn default() -> Self {
        Self {
            background_color: Vec4::new(0.98, 0.98, 0.98, 1.0),
            text_color: Vec4::new(0.3, 0.3, 0.3, 1.0),
            border_color: Vec4::new(0.8, 0.8, 0.8, 1.0),
            drag_border_color: Vec4::new(0.2, 0.6, 1.0, 1.0),
            drag_background_color: Vec4::new(0.9, 0.95, 1.0, 1.0),
            progress_color: Vec4::new(0.2, 0.8, 0.2, 1.0),
            error_color: Vec4::new(0.8, 0.2, 0.2, 1.0),
            success_color: Vec4::new(0.2, 0.8, 0.2, 1.0),
            font_size: 14.0,
            border_radius: 6.0,
            drop_text: "Drop files here or click to browse".to_string(),
            browse_text: "Browse Files".to_string(),
            multiple: true,
            drag_drop: true,
            show_previews: true,
            show_progress: true,
            auto_upload: false,
            validation: FileValidation::default(),
            upload_url: None,
            upload_params: HashMap::new(),
            upload_timeout: 30,
            chunk_size: None,
            preview_size: 60.0,
            max_display_files: 20,
        }
    }
}

impl From<&PropertyBag> for FileUploadConfig {
    fn from(props: &PropertyBag) -> Self {
        let mut config = Self::default();
        
        if let Some(size) = props.get_f32("font_size") {
            config.font_size = size;
        }
        
        if let Some(radius) = props.get_f32("border_radius") {
            config.border_radius = radius;
        }
        
        if let Some(text) = props.get_string("drop_text") {
            config.drop_text = text;
        }
        
        if let Some(text) = props.get_string("browse_text") {
            config.browse_text = text;
        }
        
        if let Some(multiple) = props.get_bool("multiple") {
            config.multiple = multiple;
        }
        
        if let Some(drag_drop) = props.get_bool("drag_drop") {
            config.drag_drop = drag_drop;
        }
        
        if let Some(show_previews) = props.get_bool("show_previews") {
            config.show_previews = show_previews;
        }
        
        if let Some(show_progress) = props.get_bool("show_progress") {
            config.show_progress = show_progress;
        }
        
        if let Some(auto_upload) = props.get_bool("auto_upload") {
            config.auto_upload = auto_upload;
        }
        
        if let Some(url) = props.get_string("upload_url") {
            config.upload_url = Some(url);
        }
        
        if let Some(timeout) = props.get_i32("upload_timeout") {
            config.upload_timeout = timeout.max(1) as u64;
        }
        
        if let Some(max_size) = props.get_i32("max_file_size") {
            config.validation.max_size = Some(max_size.max(0) as u64);
        }
        
        if let Some(extensions) = props.get_vec_string("allowed_extensions") {
            config.validation.allowed_extensions = extensions;
        }
        
        config
    }
}

impl KryonWidget for FileUploadWidget {
    const WIDGET_NAME: &'static str = "file-upload";
    type State = FileUploadState;
    type Config = FileUploadConfig;
    
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
        
        // Drop zone background
        let bg_color = if state.is_dragging {
            config.drag_background_color
        } else {
            config.background_color
        };
        
        let border_color = if state.is_dragging {
            config.drag_border_color
        } else {
            config.border_color
        };
        
        commands.push(RenderCommand::DrawRect {
            position: layout.position,
            size: layout.size,
            color: bg_color,
            border_radius: config.border_radius,
            border_width: if state.is_dragging { 2.0 } else { 1.0 },
            border_color,
        });
        
        if state.files.is_empty() {
            // Empty state - show drop zone
            self.render_drop_zone(&mut commands, state, config, layout)?;
        } else {
            // Show file list
            self.render_file_list(&mut commands, state, config, layout)?;
        }
        
        // Upload progress (if uploading)
        if state.upload_status == UploadStatus::Uploading && config.show_progress {
            self.render_progress_bar(&mut commands, state, config, layout)?;
        }
        
        // Error message
        if let Some(error) = &state.error_message {
            self.render_error_message(&mut commands, config, layout, error)?;
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
                if self.point_in_rect(*position, layout.position, layout.size) {
                    // Handle click - open file dialog
                    return Ok(EventResult::EmitEvent {
                        event_name: "file_dialog_open".to_string(),
                        data: serde_json::json!({
                            "multiple": config.multiple,
                            "accept": self.get_accept_string(config)
                        }),
                    });
                }
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::DragEnter { .. } => {
                if config.drag_drop {
                    state.is_dragging = true;
                    return Ok(EventResult::HandledWithRedraw);
                }
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::DragLeave { .. } => {
                if config.drag_drop {
                    state.is_dragging = false;
                    return Ok(EventResult::HandledWithRedraw);
                }
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::DragOver { position } => {
                if config.drag_drop && self.point_in_rect(*position, layout.position, layout.size) {
                    state.is_hover = true;
                    return Ok(EventResult::HandledWithRedraw);
                }
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::Drop { position, files } => {
                if config.drag_drop && self.point_in_rect(*position, layout.position, layout.size) {
                    state.is_dragging = false;
                    state.is_hover = false;
                    
                    // Process dropped files
                    self.process_files(state, config, files.clone())?;
                    
                    return Ok(EventResult::EmitEvent {
                        event_name: "files_selected".to_string(),
                        data: serde_json::json!({
                            "files": state.files.iter().map(|f| {
                                serde_json::json!({
                                    "name": f.name,
                                    "size": f.size,
                                    "type": f.mime_type,
                                    "id": f.id
                                })
                            }).collect::<Vec<_>>()
                        }),
                    });
                }
                Ok(EventResult::NotHandled)
            }
            
            InputEvent::KeyPress { key: KeyCode::Delete, .. } => {
                // Remove selected files (simplified - would need selection state)
                if !state.files.is_empty() {
                    state.files.clear();
                    state.total_size = 0;
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

impl FileUploadWidget {
    fn render_drop_zone(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &FileUploadState,
        config: &FileUploadConfig,
        layout: &LayoutResult,
    ) -> Result<()> {
        let center = layout.position + layout.size / 2.0;
        
        // Upload icon (simplified)
        commands.push(RenderCommand::DrawText {
            text: "📁".to_string(),
            position: center + Vec2::new(-20.0, -40.0),
            font_size: 32.0,
            color: config.text_color * 0.7,
            max_width: None,
        });
        
        // Drop text
        commands.push(RenderCommand::DrawText {
            text: config.drop_text.clone(),
            position: center + Vec2::new(-80.0, -10.0),
            font_size: config.font_size,
            color: config.text_color,
            max_width: Some(160.0),
        });
        
        // Browse button
        let button_width = 120.0;
        let button_height = 32.0;
        let button_pos = center + Vec2::new(-button_width / 2.0, 20.0);
        
        commands.push(RenderCommand::DrawRect {
            position: button_pos,
            size: Vec2::new(button_width, button_height),
            color: Vec4::new(0.2, 0.6, 1.0, 1.0),
            border_radius: 4.0,
            border_width: 0.0,
            border_color: Vec4::ZERO,
        });
        
        commands.push(RenderCommand::DrawText {
            text: config.browse_text.clone(),
            position: button_pos + Vec2::new(button_width / 2.0 - 35.0, 8.0),
            font_size: config.font_size,
            color: Vec4::new(1.0, 1.0, 1.0, 1.0),
            max_width: None,
        });
        
        Ok(())
    }
    
    fn render_file_list(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &FileUploadState,
        config: &FileUploadConfig,
        layout: &LayoutResult,
    ) -> Result<()> {
        let mut y_offset = 12.0;
        let item_height = if config.show_previews { config.preview_size + 12.0 } else { 40.0 };
        
        // Header
        commands.push(RenderCommand::DrawText {
            text: format!("{} file(s) selected", state.files.len()),
            position: layout.position + Vec2::new(12.0, y_offset),
            font_size: config.font_size,
            color: config.text_color,
            max_width: None,
        });
        
        y_offset += 30.0;
        
        // File items
        let visible_files = state.files.iter().take(config.max_display_files);
        
        for (i, file) in visible_files.enumerate() {
            let item_y = layout.position.y + y_offset + (i as f32 * item_height);
            
            if item_y > layout.position.y + layout.size.y {
                break; // Outside visible area
            }
            
            self.render_file_item(commands, file, config, 
                Vec2::new(layout.position.x + 12.0, item_y), 
                layout.size.x - 24.0)?;
        }
        
        // Show more indicator
        if state.files.len() > config.max_display_files {
            let remaining = state.files.len() - config.max_display_files;
            commands.push(RenderCommand::DrawText {
                text: format!("... and {} more files", remaining),
                position: layout.position + Vec2::new(12.0, y_offset + (config.max_display_files as f32 * item_height) + 10.0),
                font_size: config.font_size * 0.9,
                color: config.text_color * 0.7,
                max_width: None,
            });
        }
        
        Ok(())
    }
    
    fn render_file_item(
        &self,
        commands: &mut Vec<RenderCommand>,
        file: &UploadFile,
        config: &FileUploadConfig,
        position: Vec2,
        width: f32,
    ) -> Result<()> {
        let mut x_offset = 0.0;
        
        // File preview/icon
        if config.show_previews {
            let preview_size = config.preview_size;
            
            if let Some(preview_url) = &file.preview_url {
                // Image preview
                commands.push(RenderCommand::DrawImage {
                    position,
                    size: Vec2::new(preview_size, preview_size),
                    source: preview_url.clone(),
                    opacity: 1.0,
                    transform: None,
                });
            } else {
                // File type icon
                let icon = self.get_file_icon(file);
                commands.push(RenderCommand::DrawText {
                    text: icon,
                    position: position + Vec2::new(preview_size / 2.0 - 12.0, preview_size / 2.0 - 12.0),
                    font_size: 24.0,
                    color: config.text_color,
                    max_width: None,
                });
            }
            
            // Preview border
            commands.push(RenderCommand::DrawRect {
                position,
                size: Vec2::new(preview_size, preview_size),
                color: Vec4::ZERO,
                border_radius: 4.0,
                border_width: 1.0,
                border_color: config.border_color,
            });
            
            x_offset += preview_size + 12.0;
        }
        
        // File info
        let info_x = position.x + x_offset;
        
        // File name
        commands.push(RenderCommand::DrawText {
            text: file.name.clone(),
            position: Vec2::new(info_x, position.y + 4.0),
            font_size: config.font_size,
            color: config.text_color,
            max_width: Some(width - x_offset - 60.0),
        });
        
        // File size and type
        let details = format!("{} • {}", file.format_size(), file.extension.to_uppercase());
        commands.push(RenderCommand::DrawText {
            text: details,
            position: Vec2::new(info_x, position.y + 20.0),
            font_size: config.font_size * 0.9,
            color: config.text_color * 0.7,
            max_width: Some(width - x_offset - 60.0),
        });
        
        // Status indicator
        let status_color = match file.status {
            FileUploadStatus::Success => config.success_color,
            FileUploadStatus::Error => config.error_color,
            FileUploadStatus::Uploading => config.progress_color,
            _ => config.text_color * 0.5,
        };
        
        let status_text = match file.status {
            FileUploadStatus::Pending => "⏳",
            FileUploadStatus::Uploading => "⬆️",
            FileUploadStatus::Success => "✅",
            FileUploadStatus::Error => "❌",
            FileUploadStatus::Cancelled => "🚫",
        };
        
        commands.push(RenderCommand::DrawText {
            text: status_text.to_string(),
            position: Vec2::new(position.x + width - 40.0, position.y + 8.0),
            font_size: 16.0,
            color: status_color,
            max_width: None,
        });
        
        // Progress bar (if uploading)
        if file.status == FileUploadStatus::Uploading && file.progress > 0.0 {
            let progress_y = position.y + 40.0;
            let progress_width = width - x_offset - 60.0;
            
            // Progress background
            commands.push(RenderCommand::DrawRect {
                position: Vec2::new(info_x, progress_y),
                size: Vec2::new(progress_width, 4.0),
                color: config.border_color,
                border_radius: 2.0,
                border_width: 0.0,
                border_color: Vec4::ZERO,
            });
            
            // Progress fill
            commands.push(RenderCommand::DrawRect {
                position: Vec2::new(info_x, progress_y),
                size: Vec2::new(progress_width * file.progress, 4.0),
                color: config.progress_color,
                border_radius: 2.0,
                border_width: 0.0,
                border_color: Vec4::ZERO,
            });
        }
        
        // Error message
        if let Some(error) = &file.error {
            commands.push(RenderCommand::DrawText {
                text: error.clone(),
                position: Vec2::new(info_x, position.y + 40.0),
                font_size: config.font_size * 0.8,
                color: config.error_color,
                max_width: Some(width - x_offset - 60.0),
            });
        }
        
        Ok(())
    }
    
    fn render_progress_bar(
        &self,
        commands: &mut Vec<RenderCommand>,
        state: &FileUploadState,
        config: &FileUploadConfig,
        layout: &LayoutResult,
    ) -> Result<()> {
        let progress_height = 6.0;
        let progress_y = layout.position.y + layout.size.y - progress_height - 8.0;
        
        // Progress background
        commands.push(RenderCommand::DrawRect {
            position: Vec2::new(layout.position.x + 8.0, progress_y),
            size: Vec2::new(layout.size.x - 16.0, progress_height),
            color: config.border_color,
            border_radius: 3.0,
            border_width: 0.0,
            border_color: Vec4::ZERO,
        });
        
        // Progress fill
        if state.upload_progress > 0.0 {
            commands.push(RenderCommand::DrawRect {
                position: Vec2::new(layout.position.x + 8.0, progress_y),
                size: Vec2::new((layout.size.x - 16.0) * state.upload_progress, progress_height),
                color: config.progress_color,
                border_radius: 3.0,
                border_width: 0.0,
                border_color: Vec4::ZERO,
            });
        }
        
        // Progress text
        commands.push(RenderCommand::DrawText {
            text: format!("Uploading... {}%", (state.upload_progress * 100.0) as u8),
            position: Vec2::new(layout.position.x + 8.0, progress_y - 18.0),
            font_size: config.font_size * 0.9,
            color: config.text_color,
            max_width: None,
        });
        
        Ok(())
    }
    
    fn render_error_message(
        &self,
        commands: &mut Vec<RenderCommand>,
        config: &FileUploadConfig,
        layout: &LayoutResult,
        error: &str,
    ) -> Result<()> {
        let error_y = layout.position.y + layout.size.y - 30.0;
        
        commands.push(RenderCommand::DrawText {
            text: format!("❌ {}", error),
            position: Vec2::new(layout.position.x + 8.0, error_y),
            font_size: config.font_size,
            color: config.error_color,
            max_width: Some(layout.size.x - 16.0),
        });
        
        Ok(())
    }
    
    fn process_files(
        &self,
        state: &mut FileUploadState,
        config: &FileUploadConfig,
        file_paths: Vec<String>,
    ) -> Result<()> {
        state.file_errors.clear();
        
        for path in file_paths {
            // Extract file info from path (simplified)
            let name = path.split('/').last().unwrap_or(&path).to_string();
            let extension = name.split('.').last().unwrap_or("").to_lowercase();
            
            // Simulate file size and MIME type detection
            let size = 1024 * 1024; // 1MB placeholder
            let mime_type = self.get_mime_type(&extension);
            
            let mut file = UploadFile::new(name.clone(), size, mime_type);
            
            // Validate file
            if let Some(error) = self.validate_file(&file, config) {
                file.is_valid = false;
                file.error = Some(error.clone());
                state.file_errors.insert(file.id.clone(), error);
            }
            
            // Check file limit
            if config.multiple {
                if state.files.len() >= state.max_files {
                    state.error_message = Some(format!("Maximum {} files allowed", state.max_files));
                    break;
                }
            } else {
                state.files.clear(); // Single file mode
            }
            
            state.files.push(file);
        }
        
        // Update total size
        state.total_size = state.files.iter().map(|f| f.size).sum();
        
        // Validate total size
        if let Some(max_total) = config.validation.max_total_size {
            if state.total_size > max_total {
                state.error_message = Some(format!("Total file size exceeds {} limit", 
                    self.format_bytes(max_total)));
            }
        }
        
        Ok(())
    }
    
    fn validate_file(&self, file: &UploadFile, config: &FileUploadConfig) -> Option<String> {
        // Check file size
        if let Some(max_size) = config.validation.max_size {
            if file.size > max_size {
                return Some(format!("File too large (max: {})", self.format_bytes(max_size)));
            }
        }
        
        if let Some(min_size) = config.validation.min_size {
            if file.size < min_size {
                return Some(format!("File too small (min: {})", self.format_bytes(min_size)));
            }
        }
        
        // Check extensions
        if !config.validation.allowed_extensions.is_empty() {
            if !config.validation.allowed_extensions.contains(&file.extension) {
                return Some(format!("File type '{}' not allowed", file.extension));
            }
        }
        
        // Check MIME types
        if !config.validation.allowed_mime_types.is_empty() {
            if !config.validation.allowed_mime_types.contains(&file.mime_type) {
                return Some(format!("MIME type '{}' not allowed", file.mime_type));
            }
        }
        
        None
    }
    
    fn get_file_icon(&self, file: &UploadFile) -> String {
        if file.is_image() {
            "🖼️"
        } else if file.is_video() {
            "🎥"
        } else if file.is_audio() {
            "🎵"
        } else {
            match file.extension.as_str() {
                "pdf" => "📄",
                "doc" | "docx" => "📝",
                "xls" | "xlsx" => "📊",
                "ppt" | "pptx" => "📈",
                "txt" => "📄",
                "zip" | "rar" | "7z" => "📦",
                _ => "📄",
            }
        }.to_string()
    }
    
    fn get_mime_type(&self, extension: &str) -> String {
        match extension {
            "jpg" | "jpeg" => "image/jpeg",
            "png" => "image/png",
            "gif" => "image/gif",
            "pdf" => "application/pdf",
            "txt" => "text/plain",
            "doc" => "application/msword",
            "docx" => "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
            "mp4" => "video/mp4",
            "mp3" => "audio/mpeg",
            _ => "application/octet-stream",
        }.to_string()
    }
    
    fn get_accept_string(&self, config: &FileUploadConfig) -> String {
        if config.validation.allowed_extensions.is_empty() && config.validation.allowed_mime_types.is_empty() {
            "*/*".to_string()
        } else {
            let mut accept_parts = Vec::new();
            
            // Add MIME types
            for mime_type in &config.validation.allowed_mime_types {
                accept_parts.push(mime_type.clone());
            }
            
            // Add extensions
            for ext in &config.validation.allowed_extensions {
                accept_parts.push(format!(".{}", ext));
            }
            
            accept_parts.join(",")
        }
    }
    
    fn format_bytes(&self, bytes: u64) -> String {
        let size = bytes as f64;
        if size < 1024.0 {
            format!("{} B", size)
        } else if size < 1024.0 * 1024.0 {
            format!("{:.1} KB", size / 1024.0)
        } else if size < 1024.0 * 1024.0 * 1024.0 {
            format!("{:.1} MB", size / (1024.0 * 1024.0))
        } else {
            format!("{:.1} GB", size / (1024.0 * 1024.0 * 1024.0))
        }
    }
    
    fn point_in_rect(&self, point: Vec2, rect_pos: Vec2, rect_size: Vec2) -> bool {
        point.x >= rect_pos.x &&
        point.x <= rect_pos.x + rect_size.x &&
        point.y >= rect_pos.y &&
        point.y <= rect_pos.y + rect_size.y
    }
}

// Custom InputEvent extensions for drag-and-drop (would need to be added to types.rs)
// These are placeholders for the actual implementation

// Implement the factory trait
crate::impl_widget_factory!(FileUploadWidget);

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_file_upload_creation() {
        let widget = FileUploadWidget::new();
        let state = widget.create_state();
        
        assert_eq!(state.files.len(), 0);
        assert!(!state.is_dragging);
        assert_eq!(state.upload_status, UploadStatus::Idle);
    }
    
    #[test]
    fn test_upload_file_creation() {
        let file = UploadFile::new(
            "test.jpg".to_string(),
            1024 * 1024,
            "image/jpeg".to_string()
        );
        
        assert_eq!(file.name, "test.jpg");
        assert_eq!(file.size, 1024 * 1024);
        assert_eq!(file.extension, "jpg");
        assert!(file.is_image());
        assert!(!file.is_video());
    }
    
    #[test]
    fn test_file_size_formatting() {
        let file = UploadFile::new("test.txt".to_string(), 1536, "text/plain".to_string());
        assert_eq!(file.format_size(), "1.5 KB");
        
        let large_file = UploadFile::new("big.zip".to_string(), 5 * 1024 * 1024, "application/zip".to_string());
        assert_eq!(large_file.format_size(), "5.0 MB");
    }
    
    #[test]
    fn test_file_validation() {
        let widget = FileUploadWidget::new();
        let mut config = FileUploadConfig::default();
        config.validation.max_size = Some(1024); // 1KB limit
        config.validation.allowed_extensions = vec!["txt".to_string(), "jpg".to_string()];
        
        // Valid file
        let valid_file = UploadFile::new("test.txt".to_string(), 512, "text/plain".to_string());
        assert!(widget.validate_file(&valid_file, &config).is_none());
        
        // Too large
        let large_file = UploadFile::new("big.txt".to_string(), 2048, "text/plain".to_string());
        assert!(widget.validate_file(&large_file, &config).is_some());
        
        // Wrong extension
        let wrong_ext = UploadFile::new("test.pdf".to_string(), 512, "application/pdf".to_string());
        assert!(widget.validate_file(&wrong_ext, &config).is_some());
    }
}