use glam::{Vec2, Vec4};
use kryon_render::{Renderer, CommandRenderer, RenderCommand, RenderResult, RenderError};
use kryon_core::{Element, ElementId};
// use kryon_layout::LayoutResult;
use std::collections::HashMap;
use uuid::Uuid;

pub mod html_generator;
pub mod css_generator;
pub mod canvas_handler;
pub mod server;

pub use html_generator::*;
pub use css_generator::*;
pub use canvas_handler::*;

#[cfg(feature = "server")]
pub use server::*;

/// HTML/CSS renderer that converts Kryon render commands to HTML and CSS
pub struct HtmlRenderer {
    /// Generated HTML content
    html_content: String,
    /// Generated CSS content
    css_content: String,
    /// Canvas elements that need WebGPU/Canvas API
    canvas_elements: Vec<CanvasElement>,
    /// JavaScript content for interactivity
    js_content: String,
    /// Unique ID for this renderer instance
    instance_id: String,
    /// Viewport size
    viewport_size: Vec2,
    /// Command buffer for the current frame
    command_buffer: Vec<RenderCommand>,
    /// Element ID counter for unique HTML IDs
    element_counter: u32,
}

#[derive(Debug, Clone)]
pub struct CanvasElement {
    pub id: String,
    pub element_type: CanvasType,
    pub position: Vec2,
    pub size: Vec2,
    pub commands: Vec<RenderCommand>,
}

#[derive(Debug, Clone)]
pub enum CanvasType {
    Canvas2D,
    Canvas3D,
    WebGPU,
}

/// Configuration for HTML generation
#[derive(Debug, Clone)]
pub struct HtmlConfig {
    /// Whether to inline CSS
    pub inline_css: bool,
    /// Whether to inline JavaScript
    pub inline_js: bool,
    /// Whether to minify output
    pub minify: bool,
    /// Additional CSS to include
    pub additional_css: String,
    /// Additional JavaScript to include
    pub additional_js: String,
    /// Title for the HTML document
    pub title: String,
    /// Meta tags to include
    pub meta_tags: Vec<String>,
}

impl Default for HtmlConfig {
    fn default() -> Self {
        Self {
            inline_css: true,
            inline_js: true,
            minify: false,
            additional_css: String::new(),
            additional_js: String::new(),
            title: "Kryon Application".to_string(),
            meta_tags: vec![
                r#"<meta charset="UTF-8">"#.to_string(),
                r#"<meta name="viewport" content="width=device-width, initial-scale=1.0">"#.to_string(),
            ],
        }
    }
}

impl HtmlRenderer {
    pub fn new(viewport_size: Vec2) -> Self {
        Self {
            html_content: String::new(),
            css_content: String::new(),
            canvas_elements: Vec::new(),
            js_content: String::new(),
            instance_id: Uuid::new_v4().to_string(),
            viewport_size,
            command_buffer: Vec::new(),
            element_counter: 0,
        }
    }

    pub fn with_config(viewport_size: Vec2, _config: HtmlConfig) -> Self {
        // TODO: Use config to customize renderer
        Self::new(viewport_size)
    }

    /// Generate complete HTML document
    pub fn generate_html(&self, config: &HtmlConfig) -> String {
        let mut html = String::new();
        
        html.push_str("<!DOCTYPE html>\n");
        html.push_str("<html lang=\"en\">\n");
        html.push_str("<head>\n");
        
        // Add meta tags
        for meta in &config.meta_tags {
            html.push_str(&format!("    {}\n", meta));
        }
        
        html.push_str(&format!("    <title>{}</title>\n", config.title));
        
        // Add CSS
        if config.inline_css {
            html.push_str("    <style>\n");
            html.push_str(&self.css_content);
            if !config.additional_css.is_empty() {
                html.push_str(&config.additional_css);
            }
            html.push_str("    </style>\n");
        } else {
            html.push_str(&format!("    <link rel=\"stylesheet\" href=\"kryon-{}.css\">\n", self.instance_id));
        }
        
        html.push_str("</head>\n");
        html.push_str("<body>\n");
        
        // Add HTML content
        html.push_str(&self.html_content);
        
        // Add JavaScript
        if config.inline_js {
            html.push_str("    <script>\n");
            html.push_str(&self.js_content);
            if !config.additional_js.is_empty() {
                html.push_str(&config.additional_js);
            }
            html.push_str("    </script>\n");
        } else {
            html.push_str(&format!("    <script src=\"kryon-{}.js\"></script>\n", self.instance_id));
        }
        
        html.push_str("</body>\n");
        html.push_str("</html>\n");
        
        if config.minify {
            self.minify_html(html)
        } else {
            html
        }
    }

    /// Generate standalone CSS file
    pub fn generate_css(&self, config: &HtmlConfig) -> String {
        let mut css = self.css_content.clone();
        
        if !config.additional_css.is_empty() {
            css.push_str(&config.additional_css);
        }
        
        if config.minify {
            self.minify_css(css)
        } else {
            css
        }
    }

    /// Generate standalone JavaScript file
    pub fn generate_js(&self, config: &HtmlConfig) -> String {
        let mut js = self.js_content.clone();
        
        if !config.additional_js.is_empty() {
            js.push_str(&config.additional_js);
        }
        
        if config.minify {
            self.minify_js(js)
        } else {
            js
        }
    }

    /// Get the unique instance ID
    pub fn instance_id(&self) -> &str {
        &self.instance_id
    }

    /// Get canvas elements that need special handling
    pub fn canvas_elements(&self) -> &[CanvasElement] {
        &self.canvas_elements
    }

    fn minify_html(&self, html: String) -> String {
        #[cfg(feature = "minify")]
        {
            match minify_html::minify(html.as_bytes(), &minify_html::Cfg::spec_compliant()) {
                Ok(minified) => String::from_utf8_lossy(&minified).to_string(),
                Err(_) => html, // Fallback to original if minification fails
            }
        }
        #[cfg(not(feature = "minify"))]
        html
    }

    fn minify_css(&self, css: String) -> String {
        #[cfg(feature = "minify")]
        {
            match lightningcss::stylesheet::StyleSheet::parse(&css, lightningcss::stylesheet::ParserOptions::default()) {
                Ok(mut stylesheet) => {
                    let result = stylesheet.to_css(lightningcss::stylesheet::PrinterOptions {
                        minify: true,
                        ..Default::default()
                    });
                    match result {
                        Ok(minified) => minified.code,
                        Err(_) => css, // Fallback
                    }
                }
                Err(_) => css, // Fallback
            }
        }
        #[cfg(not(feature = "minify"))]
        css
    }

    fn minify_js(&self, js: String) -> String {
        // TODO: Add JS minification if needed
        js
    }

    fn next_element_id(&mut self) -> String {
        self.element_counter += 1;
        format!("kryon-element-{}", self.element_counter)
    }

}

impl Renderer for HtmlRenderer {
    type Surface = Vec2; // Viewport size
    type Context = (); // No context needed for HTML generation

    fn initialize(surface: Self::Surface) -> RenderResult<Self> {
        Ok(HtmlRenderer::new(surface))
    }

    fn begin_frame(&mut self, clear_color: Vec4) -> RenderResult<Self::Context> {
        // Clear previous frame content
        self.html_content.clear();
        self.css_content.clear();
        self.js_content.clear();
        self.canvas_elements.clear();
        self.command_buffer.clear();
        self.element_counter = 0;

        // Set up base styles
        self.css_content.push_str("/* Kryon HTML Renderer */\n");
        self.css_content.push_str("* { box-sizing: border-box; }\n");
        self.css_content.push_str(&format!(
            "body {{ margin: 0; padding: 0; background-color: rgba({}, {}, {}, {}); font-family: Arial, sans-serif; }}\n",
            (clear_color.x * 255.0) as u8,
            (clear_color.y * 255.0) as u8,
            (clear_color.z * 255.0) as u8,
            clear_color.w
        ));

        Ok(())
    }

    fn end_frame(&mut self, _context: Self::Context) -> RenderResult<()> {
        // Process all commands and generate HTML/CSS
        self.process_commands()?;
        Ok(())
    }

    fn render_element(
        &mut self,
        _context: &mut Self::Context,
        _element: &Element,
        _layout: &kryon_layout::LayoutResult,
        _element_id: ElementId,
    ) -> RenderResult<()> {
        // Not used in command-based rendering
        Ok(())
    }

    fn resize(&mut self, new_size: Vec2) -> RenderResult<()> {
        self.viewport_size = new_size;
        Ok(())
    }

    fn viewport_size(&self) -> Vec2 {
        self.viewport_size
    }
}

impl CommandRenderer for HtmlRenderer {
    fn execute_commands(
        &mut self,
        _context: &mut Self::Context,
        commands: &[RenderCommand],
    ) -> RenderResult<()> {
        // Store commands for processing
        self.command_buffer.extend_from_slice(commands);
        Ok(())
    }
}

impl HtmlRenderer {
    fn process_commands(&mut self) -> RenderResult<()> {
        // Create a root container
        self.html_content.push_str(&format!(
            "<div id=\"kryon-root-{}\" class=\"kryon-root\">\n",
            self.instance_id
        ));

        // Add root styles
        self.css_content.push_str(&format!(
            ".kryon-root {{ width: {}px; height: {}px; position: relative; overflow: hidden; }}\n",
            self.viewport_size.x, self.viewport_size.y
        ));

        // Process each command
        let commands = self.command_buffer.clone();
        for command in commands {
            self.process_command(&command)?;
        }

        self.html_content.push_str("</div>\n");
        Ok(())
    }

    fn process_command(&mut self, command: &RenderCommand) -> RenderResult<()> {
        match command {
            RenderCommand::DrawRect { position, size, color, border_radius, border_width, border_color, z_index, .. } => {
                self.generate_rect_element(*position, *size, *color, *border_radius, *border_width, *border_color, *z_index)?;
            }
            RenderCommand::DrawText { position, text, font_size, color, alignment, max_width, max_height, font_family, z_index, .. } => {
                self.generate_text_element(*position, text, *font_size, *color, *alignment, *max_width, *max_height, font_family, *z_index)?;
            }
            RenderCommand::DrawTextInput { position, size, text, placeholder, font_size, text_color, background_color, border_color, border_width, border_radius, is_focused, is_readonly, .. } => {
                self.generate_input_element(*position, *size, text, placeholder, *font_size, *text_color, *background_color, *border_color, *border_width, *border_radius, *is_focused, *is_readonly)?;
            }
            RenderCommand::DrawCheckbox { position, size, is_checked, text, font_size, text_color, background_color, border_color, border_width, check_color, .. } => {
                self.generate_checkbox_element(*position, *size, *is_checked, text, *font_size, *text_color, *background_color, *border_color, *border_width, *check_color)?;
            }
            RenderCommand::DrawImage { position, size, source, opacity, .. } => {
                self.generate_image_element(*position, *size, source, *opacity)?;
            }
            RenderCommand::BeginCanvas { canvas_id, position, size } => {
                self.begin_canvas_element(canvas_id, *position, *size, CanvasType::Canvas2D)?;
            }
            RenderCommand::BeginCanvas3D { canvas_id, position, size, .. } => {
                self.begin_canvas_element(canvas_id, *position, *size, CanvasType::Canvas3D)?;
            }
            RenderCommand::EndCanvas | RenderCommand::EndCanvas3D => {
                self.end_canvas_element()?;
            }
            RenderCommand::SetCanvasSize(_) => {
                // Canvas size is handled in BeginCanvas
            }
            _ => {
                // Handle other commands or skip unsupported ones
                eprintln!("Warning: HTML renderer does not support command: {:?}", command);
            }
        }
        Ok(())
    }
}

/// Result type for HTML rendering operations
pub type HtmlResult<T> = Result<T, HtmlError>;

/// Error types for HTML rendering
#[derive(Debug, thiserror::Error)]
pub enum HtmlError {
    #[error("HTML generation failed: {0}")]
    GenerationFailed(String),
    #[error("Invalid render command: {0}")]
    InvalidCommand(String),
    #[error("Canvas operation failed: {0}")]
    CanvasFailed(String),
    #[error("Server error: {0}")]
    ServerError(String),
}

impl From<HtmlError> for RenderError {
    fn from(err: HtmlError) -> Self {
        RenderError::RenderFailed(err.to_string())
    }
}