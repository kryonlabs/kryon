//! PDF ViewProvider Implementation
//! 
//! Provides PDF document viewing capabilities with navigation and zoom controls.

use super::super::*;
use crate::RenderCommand;
use kryon_core::TextAlignment;
use glam::{Vec2, Vec4};
use std::collections::HashMap;
use kryon_core::PropertyValue;

/// PDF viewer provider for document viewing
pub struct PdfViewProvider;

impl PdfViewProvider {
    pub fn new() -> Self {
        Self
    }
}

impl ViewProvider for PdfViewProvider {
    fn type_id(&self) -> &'static str {
        "pdf"
    }
    
    fn create_view(&self, source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Box<dyn ViewInstance> {
        Box::new(PdfViewInstance::new(source, config))
    }
    
    fn can_handle(&self, source: Option<&str>) -> bool {
        // PDF provider handles .pdf files
        if let Some(src) = source {
            src.ends_with(".pdf") || src.contains("application/pdf")
        } else {
            false
        }
    }
}

/// PDF viewer instance
pub struct PdfViewInstance {
    file_path: Option<String>,
    bounds: Rect,
    config: HashMap<String, PropertyValue>,
    viewer_state: ViewerState,
    current_page: usize,
    total_pages: usize,
    zoom_level: f32,
    #[allow(dead_code)]
    rotation: f32, // degrees
    fit_mode: FitMode,
    // Mock PDF document state
    document_title: String,
    page_content: Vec<String>, // Text content per page
    page_images: Vec<bool>, // Whether page has images
    scroll_offset: Vec2,
}

#[derive(Debug, Clone)]
enum ViewerState {
    Loading,
    Ready,
    Error(String),
}

#[derive(Debug, Clone, Copy)]
enum FitMode {
    ActualSize,
    FitWidth,
    FitPage,
    FitVisible,
}

impl PdfViewInstance {
    pub fn new(source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Self {
        let file_path = source.map(|s| s.to_string());
        
        let zoom_level = config.get("zoom_level")
            .and_then(|v| v.as_float())
            .unwrap_or(1.0) as f32;
            
        let fit_mode = config.get("fit_mode")
            .and_then(|v| v.as_string())
            .map(|mode| match mode {
                "actual" => FitMode::ActualSize,
                "width" => FitMode::FitWidth,
                "page" => FitMode::FitPage,
                "visible" => FitMode::FitVisible,
                _ => FitMode::FitWidth,
            })
            .unwrap_or(FitMode::FitWidth);
        
        let (viewer_state, document_title, page_content, total_pages) = if let Some(ref path) = file_path {
            let (title, content, pages) = generate_mock_pdf_content(path);
            (ViewerState::Loading, title, content, pages)
        } else {
            (ViewerState::Error("No PDF file specified".to_string()), "Error".to_string(), vec![], 0)
        };
        
        Self {
            file_path,
            bounds: Rect::new(0.0, 0.0, 800.0, 600.0),
            config: config.clone(),
            viewer_state,
            current_page: 0,
            total_pages,
            zoom_level,
            rotation: 0.0,
            fit_mode,
            document_title,
            page_content,
            page_images: vec![true; total_pages], // Assume all pages have images for demo
            scroll_offset: Vec2::ZERO,
        }
    }
    
    fn simulate_pdf_load(&mut self) {
        match &self.viewer_state {
            ViewerState::Loading => {
                // Simulate loading completion
                self.viewer_state = ViewerState::Ready;
                if let Some(ref path) = self.file_path {
                    eprintln!("[PDF] Document loaded: {}", path);
                    eprintln!("[PDF] {} pages, current zoom: {}%", self.total_pages, (self.zoom_level * 100.0) as i32);
                }
            }
            _ => {}
        }
    }
    
    fn render_pdf_page(&self, frame: &mut dyn Frame, page_x: f32, page_y: f32, page_width: f32, page_height: f32) {
        // Draw page background (white paper)
        frame.execute_command(RenderCommand::DrawRect {
            position: Vec2::new(page_x, page_y),
            size: Vec2::new(page_width, page_height),
            color: Vec4::new(1.0, 1.0, 1.0, 1.0), // White page
            border_radius: 2.0,
            border_width: 1.0,
            border_color: Vec4::new(0.8, 0.8, 0.8, 1.0),
            transform: None,
            shadow: None,
            z_index: 1,
        });
        
        // Draw page content
        if self.current_page < self.page_content.len() {
            let content = &self.page_content[self.current_page];
            let lines: Vec<&str> = content.lines().collect();
            let line_height = 18.0 * self.zoom_level;
            let margin = 20.0 * self.zoom_level;
            
            for (i, line) in lines.iter().enumerate() {
                let text_y = page_y + margin + (i as f32 * line_height) - self.scroll_offset.y;
                
                // Skip lines that are off-screen
                if text_y < page_y - line_height || text_y > page_y + page_height + line_height {
                    continue;
                }
                
                let (font_size, color, _weight) = if line.trim().is_empty() {
                    continue;
                } else if line.starts_with("# ") {
                    // Heading
                    (16.0 * self.zoom_level, Vec4::new(0.1, 0.1, 0.1, 1.0), "bold")
                } else if line.starts_with("## ") {
                    // Subheading
                    (14.0 * self.zoom_level, Vec4::new(0.2, 0.2, 0.2, 1.0), "bold")
                } else {
                    // Body text
                    (12.0 * self.zoom_level, Vec4::new(0.3, 0.3, 0.3, 1.0), "normal")
                };
                
                let clean_text = line.trim_start_matches("# ").trim_start_matches("## ");
                
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(page_x + margin, text_y),
                    text: clean_text.to_string(),
                    font_size,
                    color,
                    alignment: TextAlignment::Start,
                    max_width: Some(page_width - margin * 2.0),
                    max_height: Some(line_height),
                    transform: None,
                    font_family: Some("serif".to_string()),
                    z_index: 2,
                });
            }
        }
        
        // Draw page number
        frame.execute_command(RenderCommand::DrawText {
            position: Vec2::new(page_x + page_width / 2.0, page_y + page_height - 15.0 * self.zoom_level),
            text: format!("{}", self.current_page + 1),
            font_size: 10.0 * self.zoom_level,
            color: Vec4::new(0.5, 0.5, 0.5, 1.0),
            alignment: TextAlignment::Center,
            max_width: Some(50.0),
            max_height: Some(15.0),
            transform: None,
            font_family: Some("sans-serif".to_string()),
            z_index: 3,
        });
        
        // Draw mock image placeholder if page has images
        if self.current_page < self.page_images.len() && self.page_images[self.current_page] {
            let img_width = page_width * 0.4;
            let img_height = img_width * 0.6;
            let img_x = page_x + page_width - img_width - 20.0 * self.zoom_level;
            let img_y = page_y + 60.0 * self.zoom_level;
            
            frame.execute_command(RenderCommand::DrawRect {
                position: Vec2::new(img_x, img_y),
                size: Vec2::new(img_width, img_height),
                color: Vec4::new(0.9, 0.95, 1.0, 1.0), // Light blue placeholder
                border_radius: 4.0,
                border_width: 1.0,
                border_color: Vec4::new(0.7, 0.7, 0.7, 1.0),
                transform: None,
                shadow: None,
                z_index: 2,
            });
            
            frame.execute_command(RenderCommand::DrawText {
                position: Vec2::new(img_x + img_width / 2.0, img_y + img_height / 2.0),
                text: "📷 Image".to_string(),
                font_size: 12.0 * self.zoom_level,
                color: Vec4::new(0.5, 0.5, 0.5, 1.0),
                alignment: TextAlignment::Center,
                max_width: Some(img_width),
                max_height: Some(img_height),
                transform: None,
                font_family: Some("sans-serif".to_string()),
                z_index: 3,
            });
        }
    }
}

impl ViewInstance for PdfViewInstance {
    fn draw(&mut self, frame: &mut dyn Frame, bounds: Rect) {
        self.bounds = bounds;
        
        // Simulate PDF loading
        self.simulate_pdf_load();
        
        // Draw viewer background
        frame.execute_command(RenderCommand::DrawRect {
            position: Vec2::new(bounds.x, bounds.y),
            size: Vec2::new(bounds.width, bounds.height),
            color: Vec4::new(0.95, 0.95, 0.95, 1.0), // Light gray background
            border_radius: 0.0,
            border_width: 0.0,
            border_color: Vec4::ZERO,
            transform: None,
            shadow: None,
            z_index: 0,
        });
        
        // Draw toolbar
        let toolbar_height = 40.0;
        frame.execute_command(RenderCommand::DrawRect {
            position: Vec2::new(bounds.x, bounds.y),
            size: Vec2::new(bounds.width, toolbar_height),
            color: Vec4::new(0.9, 0.9, 0.9, 1.0),
            border_radius: 0.0,
            border_width: 1.0,
            border_color: Vec4::new(0.8, 0.8, 0.8, 1.0),
            transform: None,
            shadow: None,
            z_index: 1,
        });
        
        // Draw document title in toolbar
        frame.execute_command(RenderCommand::DrawText {
            position: Vec2::new(bounds.x + 10.0, bounds.y + 8.0),
            text: format!("📄 {}", self.document_title),
            font_size: 14.0,
            color: Vec4::new(0.2, 0.2, 0.2, 1.0),
            alignment: TextAlignment::Start,
            max_width: Some(bounds.width * 0.6),
            max_height: Some(toolbar_height - 10.0),
            transform: None,
            font_family: Some("sans-serif".to_string()),
            z_index: 2,
        });
        
        // Draw page navigation in toolbar
        frame.execute_command(RenderCommand::DrawText {
            position: Vec2::new(bounds.x + bounds.width - 150.0, bounds.y + 8.0),
            text: format!("Page {} of {} | {}%", 
                self.current_page + 1, 
                self.total_pages, 
                (self.zoom_level * 100.0) as i32
            ),
            font_size: 12.0,
            color: Vec4::new(0.4, 0.4, 0.4, 1.0),
            alignment: TextAlignment::End,
            max_width: Some(140.0),
            max_height: Some(toolbar_height - 10.0),
            transform: None,
            font_family: Some("monospace".to_string()),
            z_index: 2,
        });
        
        // Calculate page area
        let page_area = Rect::new(
            bounds.x + 10.0,
            bounds.y + toolbar_height + 10.0,
            bounds.width - 20.0,
            bounds.height - toolbar_height - 20.0,
        );
        
        match &self.viewer_state {
            ViewerState::Loading => {
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(page_area.x + page_area.width / 2.0 - 60.0, page_area.y + page_area.height / 2.0),
                    text: "📄 Loading PDF...".to_string(),
                    font_size: 16.0,
                    color: Vec4::new(0.5, 0.5, 0.5, 1.0),
                    alignment: TextAlignment::Center,
                    max_width: Some(page_area.width),
                    max_height: Some(page_area.height),
                    transform: None,
                    font_family: Some("sans-serif".to_string()),
                    z_index: 1,
                });
            }
            ViewerState::Ready => {
                // Calculate page dimensions based on fit mode
                let base_width = 595.0; // A4 width in points
                let base_height = 842.0; // A4 height in points
                let aspect_ratio = base_width / base_height;
                
                let (page_width, page_height) = match self.fit_mode {
                    FitMode::FitWidth => {
                        let w = page_area.width * 0.9;
                        (w, w / aspect_ratio)
                    }
                    FitMode::FitPage => {
                        let w = page_area.width * 0.9;
                        let h = page_area.height * 0.9;
                        if w / aspect_ratio <= h {
                            (w, w / aspect_ratio)
                        } else {
                            (h * aspect_ratio, h)
                        }
                    }
                    FitMode::ActualSize => {
                        (base_width * self.zoom_level, base_height * self.zoom_level)
                    }
                    FitMode::FitVisible => {
                        let w = page_area.width * 0.95;
                        let h = page_area.height * 0.95;
                        if w / aspect_ratio <= h {
                            (w, w / aspect_ratio)
                        } else {
                            (h * aspect_ratio, h)
                        }
                    }
                };
                
                let page_x = page_area.x + (page_area.width - page_width) / 2.0;
                let page_y = page_area.y + (page_area.height - page_height) / 2.0;
                
                self.render_pdf_page(frame, page_x, page_y, page_width, page_height);
            }
            ViewerState::Error(msg) => {
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(page_area.x + 20.0, page_area.y + 20.0),
                    text: format!("❌ Error loading PDF: {}", msg),
                    font_size: 14.0,
                    color: Vec4::new(0.8, 0.2, 0.2, 1.0),
                    alignment: TextAlignment::Start,
                    max_width: Some(page_area.width - 40.0),
                    max_height: Some(page_area.height - 40.0),
                    transform: None,
                    font_family: Some("sans-serif".to_string()),
                    z_index: 1,
                });
            }
        }
        
        // Draw status bar
        let status_bar_height = 20.0;
        let status_y = bounds.y + bounds.height - status_bar_height;
        
        frame.execute_command(RenderCommand::DrawRect {
            position: Vec2::new(bounds.x, status_y),
            size: Vec2::new(bounds.width, status_bar_height),
            color: Vec4::new(0.85, 0.85, 0.85, 1.0),
            border_radius: 0.0,
            border_width: 1.0,
            border_color: Vec4::new(0.7, 0.7, 0.7, 1.0),
            transform: None,
            shadow: None,
            z_index: 4,
        });
        
        // Fit mode indicator
        let fit_text = match self.fit_mode {
            FitMode::ActualSize => "Actual Size",
            FitMode::FitWidth => "Fit Width",
            FitMode::FitPage => "Fit Page",
            FitMode::FitVisible => "Fit Visible",
        };
        
        frame.execute_command(RenderCommand::DrawText {
            position: Vec2::new(bounds.x + 5.0, status_y + 2.0),
            text: fit_text.to_string(),
            font_size: 10.0,
            color: Vec4::new(0.4, 0.4, 0.4, 1.0),
            alignment: TextAlignment::Start,
            max_width: Some(100.0),
            max_height: Some(16.0),
            transform: None,
            font_family: Some("sans-serif".to_string()),
            z_index: 5,
        });
    }
    
    fn handle_event(&mut self, event: &InputEvent) -> bool {
        match event {
            InputEvent::MouseDown { position: _, button: _ } => {
                // Simple interaction for demo
                eprintln!("[PDF] Mouse click detected");
                true
            }
            InputEvent::KeyDown { key } => {
                match key.as_str() {
                    "Right" | "PageDown" | "Space" => {
                        if self.current_page + 1 < self.total_pages {
                            self.current_page += 1;
                            self.scroll_offset = Vec2::ZERO;
                            eprintln!("[PDF] Next page: {}/{}", self.current_page + 1, self.total_pages);
                        }
                        true
                    }
                    "Left" | "PageUp" => {
                        if self.current_page > 0 {
                            self.current_page -= 1;
                            self.scroll_offset = Vec2::ZERO;
                            eprintln!("[PDF] Previous page: {}/{}", self.current_page + 1, self.total_pages);
                        }
                        true
                    }
                    "Home" => {
                        self.current_page = 0;
                        self.scroll_offset = Vec2::ZERO;
                        eprintln!("[PDF] First page");
                        true
                    }
                    "End" => {
                        self.current_page = self.total_pages.saturating_sub(1);
                        self.scroll_offset = Vec2::ZERO;
                        eprintln!("[PDF] Last page");
                        true
                    }
                    "Equal" | "Plus" => {
                        self.zoom_level = (self.zoom_level * 1.2).min(5.0);
                        eprintln!("[PDF] Zoom in: {}%", (self.zoom_level * 100.0) as i32);
                        true
                    }
                    "Minus" => {
                        self.zoom_level = (self.zoom_level * 0.8).max(0.25);
                        eprintln!("[PDF] Zoom out: {}%", (self.zoom_level * 100.0) as i32);
                        true
                    }
                    "1" => {
                        self.fit_mode = FitMode::ActualSize;
                        eprintln!("[PDF] Fit mode: Actual Size");
                        true
                    }
                    "2" => {
                        self.fit_mode = FitMode::FitWidth;
                        eprintln!("[PDF] Fit mode: Fit Width");
                        true
                    }
                    "3" => {
                        self.fit_mode = FitMode::FitPage;
                        eprintln!("[PDF] Fit mode: Fit Page");
                        true
                    }
                    "4" => {
                        self.fit_mode = FitMode::FitVisible;
                        eprintln!("[PDF] Fit mode: Fit Visible");
                        true
                    }
                    "Up" => {
                        self.scroll_offset.y = (self.scroll_offset.y - 50.0).max(0.0);
                        true
                    }
                    "Down" => {
                        self.scroll_offset.y += 50.0;
                        true
                    }
                    _ => false,
                }
            }
            _ => false,
        }
    }
    
    fn update_config(&mut self, config: &HashMap<String, PropertyValue>) {
        if let Some(zoom) = config.get("zoom_level").and_then(|v| v.as_float()) {
            self.zoom_level = zoom as f32;
        }
        
        if let Some(fit_mode) = config.get("fit_mode").and_then(|v| v.as_string()) {
            self.fit_mode = match fit_mode {
                "actual" => FitMode::ActualSize,
                "width" => FitMode::FitWidth,
                "page" => FitMode::FitPage,
                "visible" => FitMode::FitVisible,
                _ => self.fit_mode,
            };
        }
        
        self.config = config.clone();
        eprintln!("[PDF] Config updated - Zoom: {}%, Fit: {:?}", 
                 (self.zoom_level * 100.0) as i32, self.fit_mode);
    }
    
    fn update_source(&mut self, new_source: Option<&str>) {
        self.file_path = new_source.map(|s| s.to_string());
        
        if let Some(ref path) = self.file_path {
            let (title, content, pages) = generate_mock_pdf_content(path);
            self.document_title = title;
            self.page_content = content;
            self.total_pages = pages;
            self.current_page = 0;
            self.scroll_offset = Vec2::ZERO;
            self.viewer_state = ViewerState::Loading;
            self.page_images = vec![true; pages];
            
            eprintln!("[PDF] Loading document: {}", path);
        } else {
            self.viewer_state = ViewerState::Error("No PDF file specified".to_string());
        }
    }
    
    fn resize(&mut self, new_bounds: Rect) {
        self.bounds = new_bounds;
        eprintln!("[PDF] Viewer resized to {}×{}", new_bounds.width as i32, new_bounds.height as i32);
    }
    
    fn destroy(&mut self) {
        eprintln!("[PDF] PDF viewer destroyed");
        // In a real implementation: cleanup PDF renderer, close document, etc.
    }
}

impl Default for PdfViewProvider {
    fn default() -> Self {
        Self::new()
    }
}

// Helper function for generating mock PDF content
fn generate_mock_pdf_content(file_path: &str) -> (String, Vec<String>, usize) {
    let filename = file_path.split('/').last().unwrap_or("document.pdf");
    let title = filename.trim_end_matches(".pdf").to_string();
    
    let pages = vec![
        format!(r#"# {}

This is a mock PDF document being rendered by the Kryon PDF provider.

## Features

• Multi-page navigation with keyboard shortcuts
• Zoom controls (+ and - keys)
• Multiple fit modes (1-4 keys)
• Scroll support within pages
• Document metadata display

## Navigation Controls

Right/PageDown/Space: Next page
Left/PageUp: Previous page
Home: First page
End: Last page

## Zoom Controls

+/=: Zoom in
-: Zoom out

## Fit Modes

1: Actual Size
2: Fit Width
3: Fit Page  
4: Fit Visible

This document demonstrates the capabilities of embedded PDF viewing within Kryon applications."#, title),

        r#"# Page 2: Technical Details

## PDF Rendering Engine

The PDF provider uses advanced rendering techniques to display documents:

• Vector graphics support
• Font rendering with proper metrics
• Image embedding and scaling
• Interactive elements (future)

## Implementation Notes

This is a mock implementation that demonstrates the structure and capabilities of a full PDF renderer. In a production system, this would integrate with libraries like:

• PDF.js for web-based rendering
• Poppler for native desktop rendering
• MuPDF for lightweight applications

## Performance Optimization

• Lazy loading of pages
• Cached rendering results  
• Incremental updates
• Memory efficient text extraction"#.to_string(),

        r#"# Page 3: Integration with Kryon

## EmbedView Integration

The PDF provider seamlessly integrates with Kryon's EmbedView system:

```kry
EmbedView {
    provider: "pdf"
    source: "document.pdf"
    zoom_level: 1.2
    fit_mode: "width"
}
```

## Configuration Options

• zoom_level: Default zoom percentage
• fit_mode: How to fit pages in the viewport
• page_cache_size: Number of pages to cache
• rendering_quality: DPI for text rendering

## Event Handling

The PDF provider responds to:
• Mouse wheel scrolling
• Keyboard navigation
• Touch gestures (on mobile)
• Custom toolbar interactions"#.to_string(),
    ];
    
    let page_count = pages.len();
    (title, pages, page_count)
}