use glam::{Vec2, Vec3, Vec4, Mat4};
use std::collections::HashMap;
// use tracing::info; // No longer needed

use kryon_core::{Element, ElementId, ElementType, PropertyValue, StyleComputer, TextAlignment, TransformData};
use kryon_layout::LayoutResult;

// Canvas-specific data structures
#[derive(Debug, Clone)]
pub struct CameraData {
    pub position: Vec3,
    pub target: Vec3,
    pub up: Vec3,
    pub fov: f32,
    pub near_plane: f32,
    pub far_plane: f32,
}

#[derive(Debug, Clone)]
pub struct Shadow {
    pub blur: f32,
    pub color: Vec4,
    pub offset_x: f32,
    pub offset_y: f32,
}

#[derive(Debug, Clone)]
pub struct ImageData {
    pub width: u32,
    pub height: u32,
    pub data: Vec<u8>,
}

#[derive(Debug, Clone)]
pub struct DirectionalLight {
    pub direction: Vec3,
    pub color: Vec4,
    pub intensity: f32,
}

#[derive(Debug, Clone)]
pub struct PointLight {
    pub position: Vec3,
    pub color: Vec4,
    pub intensity: f32,
    pub range: f32,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ScrollbarOrientation {
    Vertical,
    Horizontal,
}

pub mod events;
pub use events::*;

pub mod text_manager;
pub use text_manager::*;

pub mod embed_view;
pub use embed_view::*;

#[cfg(feature = "wasm")]
pub mod wasm;
#[cfg(feature = "wasm")]
pub use wasm::*;

#[derive(Debug, thiserror::Error)]
pub enum RenderError {
    #[error("Renderer initialization failed: {0}")]
    InitializationFailed(String),
    #[error("Render operation failed: {0}")]
    RenderFailed(String),
    #[error("Resource not found: {0}")]
    ResourceNotFound(String),
    #[error("Unsupported operation: {0}")]
    UnsupportedOperation(String),
}

pub type RenderResult<T> = std::result::Result<T, RenderError>;

/// Core rendering trait that all backends must implement.
pub trait Renderer {
    type Surface;
    type Context;

    fn initialize(surface: Self::Surface) -> RenderResult<Self>
    where
        Self: Sized;
    fn begin_frame(&mut self, clear_color: Vec4) -> RenderResult<Self::Context>;
    fn end_frame(&mut self, context: Self::Context) -> RenderResult<()>;
    fn render_element(
        &mut self,
        context: &mut Self::Context,
        element: &Element,
        layout: &LayoutResult,
        element_id: ElementId,
    ) -> RenderResult<()>;
    fn resize(&mut self, new_size: Vec2) -> RenderResult<()>;
    fn viewport_size(&self) -> Vec2;
}

/// High-level rendering commands for backends that use them.
#[derive(Debug, Clone)]
pub enum RenderCommand {
    DrawRect {
        position: Vec2,
        size: Vec2,
        color: Vec4,
        border_radius: f32,
        border_width: f32,
        border_color: Vec4,
        transform: Option<TransformData>,
        shadow: Option<String>,
        z_index: i32,
    },
    DrawText {
        position: Vec2,
        text: String,
        font_size: f32,
        color: Vec4,
        alignment: TextAlignment,
        max_width: Option<f32>,
        max_height: Option<f32>,
        transform: Option<TransformData>,
        font_family: Option<String>,
        z_index: i32,
    },
    DrawRichText {
        position: Vec2,
        rich_text: kryon_core::RichText,
        max_width: Option<f32>,
        max_height: Option<f32>,
        default_color: Vec4,
        alignment: Option<TextAlignment>,
        transform: Option<TransformData>,
        z_index: i32,
    },
    DrawImage {
        position: Vec2,
        size: Vec2,
        source: String,
        opacity: f32,
        transform: Option<TransformData>,
    },
    SetClip {
        position: Vec2,
        size: Vec2,
    },
    ClearClip,
    /// Informs the renderer of the application's intended canvas size.
    SetCanvasSize(Vec2),
    /// Unified embed view command for external content rendering
    EmbedView {
        view_id: String,
        view_type: String,        // "webview", "wasm", "native_renderer", "iframe", etc.
        source: Option<String>,   // URI, file path, or function name
        position: Vec2,
        size: Vec2,
        config: HashMap<String, PropertyValue>,
        z_index: i32,
    },
    /// Legacy native renderer view command (deprecated)
    NativeRendererView {
        position: Vec2,
        size: Vec2,
        backend: String,
        script_name: String,
        element_id: ElementId,
        config: HashMap<String, PropertyValue>,
        z_index: i32,
    },
    /// Input-specific rendering commands
    DrawTextInput {
        position: Vec2,
        size: Vec2,
        text: String,
        placeholder: String,
        font_size: f32,
        text_color: Vec4,
        background_color: Vec4,
        border_color: Vec4,
        border_width: f32,
        border_radius: f32,
        is_focused: bool,
        is_readonly: bool,
        transform: Option<TransformData>,
    },
    DrawCheckbox {
        position: Vec2,
        size: Vec2,
        is_checked: bool,
        text: String,
        font_size: f32,
        text_color: Vec4,
        background_color: Vec4,
        border_color: Vec4,
        border_width: f32,
        check_color: Vec4,
        transform: Option<TransformData>,
    },
    DrawSlider {
        position: Vec2,
        size: Vec2,
        value: f32,
        min_value: f32,
        max_value: f32,
        track_color: Vec4,
        thumb_color: Vec4,
        border_color: Vec4,
        border_width: f32,
        transform: Option<TransformData>,
    },
    DrawScrollbar {
        position: Vec2,
        size: Vec2,
        orientation: ScrollbarOrientation,
        scroll_position: f32,
        content_size: f32,
        viewport_size: f32,
        track_color: Vec4,
        thumb_color: Vec4,
        border_color: Vec4,
        border_width: f32,
        z_index: i32,
    },
    /// Basic 2D drawing commands (standalone, not canvas-specific)
    DrawLine {
        start: Vec2,
        end: Vec2,
        color: Vec4,
        width: f32,
        transform: Option<TransformData>,
        z_index: i32,
    },
    DrawCircle {
        center: Vec2,
        radius: f32,
        fill_color: Option<Vec4>,
        stroke_color: Option<Vec4>,
        stroke_width: f32,
        transform: Option<TransformData>,
        z_index: i32,
    },
    DrawEllipse {
        center: Vec2,
        rx: f32,
        ry: f32,
        fill_color: Option<Vec4>,
        stroke_color: Option<Vec4>,
        stroke_width: f32,
        transform: Option<TransformData>,
        z_index: i32,
    },
    DrawPolygon {
        points: Vec<Vec2>,
        fill_color: Option<Vec4>,
        stroke_color: Option<Vec4>,
        stroke_width: f32,
        transform: Option<TransformData>,
        z_index: i32,
    },
    DrawPath {
        path_data: String,
        fill_color: Option<Vec4>,
        stroke_color: Option<Vec4>,
        stroke_width: f32,
        transform: Option<TransformData>,
        z_index: i32,
    },
    /// Canvas-specific rendering commands
    BeginCanvas {
        canvas_id: String,
        position: Vec2,
        size: Vec2,
    },
    EndCanvas,
    /// Basic 2D drawing commands for Canvas
    DrawCanvasLine {
        start: Vec2,
        end: Vec2,
        color: Vec4,
        width: f32,
    },
    DrawCanvasRect {
        position: Vec2,
        size: Vec2,
        fill_color: Option<Vec4>,
        stroke_color: Option<Vec4>,
        stroke_width: f32,
    },
    DrawCanvasCircle {
        center: Vec2,
        radius: f32,
        fill_color: Option<Vec4>,
        stroke_color: Option<Vec4>,
        stroke_width: f32,
    },
    DrawCanvasText {
        position: Vec2,
        text: String,
        font_size: f32,
        color: Vec4,
        font_family: Option<String>,
        alignment: TextAlignment,
    },
    /// Draw an ellipse on canvas
    DrawCanvasEllipse {
        center: Vec2,
        rx: f32,
        ry: f32,
        fill_color: Option<Vec4>,
        stroke_color: Option<Vec4>,
        stroke_width: f32,
    },
    /// Draw a polygon from a list of vertices
    DrawCanvasPolygon {
        points: Vec<Vec2>,
        fill_color: Option<Vec4>,
        stroke_color: Option<Vec4>,
        stroke_width: f32,
    },
    /// Draw a complex shape using SVG-like path data
    DrawCanvasPath {
        path_data: String,
        fill_color: Option<Vec4>,
        stroke_color: Option<Vec4>,
        stroke_width: f32,
    },
    /// Draw an image on canvas
    DrawCanvasImage {
        source: String,
        position: Vec2,
        size: Vec2,
        opacity: f32,
    },
    /// 3D Canvas rendering commands
    BeginCanvas3D {
        canvas_id: String,
        position: Vec2,
        size: Vec2,
        camera: CameraData,
    },
    EndCanvas3D,
    /// Draw a 3D cube
    DrawCanvas3DCube {
        position: Vec3,
        size: Vec3,
        color: Vec4,
        wireframe: bool,
    },
    /// Draw a 3D sphere
    DrawCanvas3DSphere {
        center: Vec3,
        radius: f32,
        color: Vec4,
        wireframe: bool,
    },
    /// Draw a 3D plane
    DrawCanvas3DPlane {
        center: Vec3,
        size: Vec2,
        normal: Vec3,
        color: Vec4,
    },
    /// Draw a 3D mesh from vertices
    DrawCanvas3DMesh {
        vertices: Vec<Vec3>,
        indices: Vec<u32>,
        normals: Vec<Vec3>,
        uvs: Vec<Vec2>,
        color: Vec4,
        texture: Option<String>,
    },
    /// Set 3D lighting
    SetCanvas3DLighting {
        ambient_color: Vec4,
        directional_light: Option<DirectionalLight>,
        point_lights: Vec<PointLight>,
    },
    /// Apply 3D transformation matrix
    ApplyCanvas3DTransform {
        transform: Mat4,
    },
    /// WASM View rendering commands
    BeginWasmView {
        wasm_id: String,
        position: Vec2,
        size: Vec2,
    },
    EndWasmView,
    /// Execute a WASM function with given parameters
    ExecuteWasmFunction {
        function_name: String,
        params: Vec<f64>, // Simple numeric parameters for now
    },
}

/// Trait for backends that use command-based rendering.
pub trait CommandRenderer: Renderer {
    fn execute_commands(
        &mut self,
        context: &mut Self::Context,
        commands: &[RenderCommand],
    ) -> RenderResult<()>;
    
    /// Set the mouse cursor type (optional - some backends may not support this)
    fn set_cursor(&mut self, _cursor_type: kryon_core::CursorType) {
        // Default implementation does nothing
    }
}

/// The bridge between the scene graph and the rendering backend.
/// It translates elements and layout into a stream of `RenderCommand`s.
pub struct ElementRenderer<R: CommandRenderer> {
    backend: R,
    style_computer: StyleComputer,
    viewport_size: Vec2,
}

impl<R: CommandRenderer> ElementRenderer<R> {
    pub fn new(backend: R, style_computer: StyleComputer) -> Self {
        let viewport_size = backend.viewport_size();
        Self {
            backend,
            style_computer,
            viewport_size,
        }
    }

    /// Renders a complete frame by generating and executing a single batch of commands.
    pub fn render_frame(
        &mut self,
        elements: &HashMap<ElementId, Element>,
        layout: &LayoutResult,
        root_id: ElementId,
        clear_color: Vec4,
    ) -> RenderResult<()> {
        let mut context = self.backend.begin_frame(clear_color)?;

        if let Some(root_element) = elements.get(&root_id) {
            let mut all_commands = Vec::new();

            // Use the root element's size as defined in the KRB file for the canvas.
            let canvas_size = root_element.size;
            if canvas_size.x > 0.0 && canvas_size.y > 0.0 {
                all_commands.push(RenderCommand::SetCanvasSize(canvas_size));
            }

            // Recursively fill the command list from the element tree.
            self.collect_render_commands(&mut all_commands, elements, layout, root_id, root_element)?;

            // Sort all commands by z_index to ensure proper layering
            all_commands.sort_by_key(|cmd| {
                match cmd {
                    RenderCommand::DrawRect { z_index, .. } => *z_index,
                    RenderCommand::DrawText { z_index, .. } => *z_index,
                    RenderCommand::DrawRichText { z_index, .. } => *z_index,
                    RenderCommand::DrawScrollbar { z_index, .. } => *z_index,
                    RenderCommand::DrawLine { z_index, .. } => *z_index,
                    RenderCommand::DrawCircle { z_index, .. } => *z_index,
                    RenderCommand::DrawEllipse { z_index, .. } => *z_index,
                    RenderCommand::DrawPolygon { z_index, .. } => *z_index,
                    RenderCommand::DrawPath { z_index, .. } => *z_index,
                    RenderCommand::DrawImage { .. } => 0,
                    RenderCommand::DrawTextInput { .. } => 1,
                    RenderCommand::DrawCheckbox { .. } => 1,
                    RenderCommand::DrawSlider { .. } => 1,
                    _ => 0,
                }
            });

            self.backend.execute_commands(&mut context, &all_commands)?;
        }

        self.backend.end_frame(context)?;
        Ok(())
    }

    /// Recursively traverses the element tree and appends drawing commands to a list.
    fn collect_render_commands(
        &self,
        all_commands: &mut Vec<RenderCommand>,
        elements: &HashMap<ElementId, Element>,
        layout: &LayoutResult,
        element_id: ElementId,
        element: &Element,
    ) -> RenderResult<()> {
        // Check if element or any parent is invisible
        if !self.is_element_visible(elements, element_id) {
            eprintln!("🚫 [RENDER_SKIP] Element {} ('{}') skipped - not visible", element_id, element.id);
            return Ok(());
        }
        
        eprintln!("✅ [RENDER_ELEMENT] Rendering element {} ('{}') - visible", element_id, element.id);

        // Check if this element needs clipping for overflow
        let needs_clip = element.overflow_x != kryon_core::OverflowType::Visible || 
                        element.overflow_y != kryon_core::OverflowType::Visible;
        
        // Get element position and size for clipping
        let position = layout.computed_positions.get(&element_id).copied();
        let size = layout.computed_sizes.get(&element_id).copied();
        
        // Apply clipping if needed
        if needs_clip && position.is_some() && size.is_some() {
            all_commands.push(RenderCommand::SetClip {
                position: position.unwrap(),
                size: size.unwrap(),
            });
        }
        
        // Generate commands for the current element and append them.
        let mut element_commands = self.element_to_commands(element, layout, element_id)?;
        all_commands.append(&mut element_commands);

        // Check if we need to add scrollbar for overflow
        if (element.overflow_x == kryon_core::OverflowType::Scroll || 
            element.overflow_y == kryon_core::OverflowType::Scroll) && 
            position.is_some() && size.is_some() {
            
            let pos = position.unwrap();
            let sz = size.unwrap();
            
            // Get z-index for scrollbar (should be above content)
            let z_index = element.z_index + 1000; // Scrollbar should be above content
            
            // Add vertical scrollbar if needed
            if element.overflow_y == kryon_core::OverflowType::Scroll {
                // Calculate content height (sum of children heights)
                let mut content_height: f32 = 0.0;
                for &child_id in &element.children {
                    if let Some(child_size) = layout.computed_sizes.get(&child_id) {
                        if let Some(child_pos) = layout.computed_positions.get(&child_id) {
                            let child_bottom = child_pos.y + child_size.y - pos.y;
                            content_height = content_height.max(child_bottom);
                        }
                    }
                }
                
                // Only show scrollbar if content exceeds container
                if content_height > sz.y {
                    all_commands.push(RenderCommand::DrawScrollbar {
                        position: Vec2::new(pos.x + sz.x - 15.0, pos.y), // Right side
                        size: Vec2::new(15.0, sz.y), // Standard scrollbar width
                        orientation: ScrollbarOrientation::Vertical,
                        scroll_position: 0.0, // TODO: Track actual scroll position
                        content_size: content_height,
                        viewport_size: sz.y,
                        track_color: Vec4::new(0.9, 0.9, 0.9, 1.0),
                        thumb_color: Vec4::new(0.6, 0.6, 0.6, 1.0),
                        border_color: Vec4::new(0.8, 0.8, 0.8, 1.0),
                        border_width: 1.0,
                        z_index,
                    });
                }
            }
        }

        // Recurse for children.
        for &child_id in &element.children {
            if let Some(child_element) = elements.get(&child_id) {
                self.collect_render_commands(all_commands, elements, layout, child_id, child_element)?;
            }
        }
        
        // Clear clipping after rendering children
        if needs_clip {
            all_commands.push(RenderCommand::ClearClip);
        }
        
        Ok(())
    }

    /// Helper function to check visibility including parent chain
    fn is_element_visible(
        &self,
        elements: &HashMap<ElementId, Element>,
        element_id: ElementId,
    ) -> bool {
        if let Some(element) = elements.get(&element_id) {
            if !element.visible {
                return false;
            }
            
            // Check parent visibility recursively
            if let Some(parent_id) = element.parent {
                return self.is_element_visible(elements, parent_id);
            }
        }
        
        true // Root element or element not found - consider visible
    }

    /// Translates a single element into one or more `RenderCommand`s.
    /// This function is the heart of the renderer logic.
    fn element_to_commands(
        &self,
        element: &Element,
        layout: &LayoutResult,
        element_id: ElementId,
    ) -> RenderResult<Vec<RenderCommand>> {
        let mut commands = Vec::new();

        // Get the final computed style for the element using its current interaction state.
        let style = self.style_computer.compute_with_state(element_id, element.current_state);

        // Get the position and size FROM THE LAYOUT ENGINE. This is the single source of truth.
        let Some(position) = layout.computed_positions.get(&element_id).copied() else {
            eprintln!("🚫 [RENDER_CMD] Element {} ('{}') has no position in layout", element_id, element.id);
            return Ok(commands); // Element not positioned by layout, so it can't be drawn.
        };
        let Some(size) = layout.computed_sizes.get(&element_id).copied() else {
            eprintln!("🚫 [RENDER_CMD] Element {} ('{}') has no size in layout", element_id, element.id);
            return Ok(commands); // Element has no size, so it can't be drawn.
        };
        
        // Debug output for dropdown-related elements
        if element.id == "menu" || element.id == "button" || element.id == "container" {
            eprintln!("🔍 [RENDER_DEBUG] Element {} ('{}') - pos=({}, {}), size=({}, {}), visible={}", 
                element_id, element.id, position.x, position.y, size.x, size.y, element.visible);
        }
        
        // Debug output to track layout vs rendering discrepancy
        if size.x == 800.0 && size.y > 70.0 && size.y < 80.0 {
            eprintln!("[RENDER_CMD] Element {} (800px wide): pos=({}, {}), size=({}, {})", 
                element_id, position.x, position.y, size.x, size.y);
        }
        
        // Draw the background/border rectangle.
        let mut bg_color = style.background_color;
        bg_color.w *= element.opacity;

        let border_width = style.border_width;
        let mut border_color = style.border_color;
        border_color.w *= element.opacity;

        // Check if element has transform data
        let transform = element.custom_properties.get("transform_index")
            .and_then(|v| v.as_int())
            .and_then(|_index| {
                // TODO: Get transform data from KRB file transforms array
                // For now, return None until we have access to the transforms
                None
            });
        
        if bg_color.w > 0.0 || border_width > 0.0 {
            // Extract shadow information from element properties
            let shadow = element.custom_properties.get("shadow")
                .and_then(|v| v.as_string())
                .map(|s| s.to_string());
                
            // Extract z_index from element properties
            let z_index = element.z_index;
            
            commands.push(RenderCommand::DrawRect {
                position,
                size,
                color: bg_color,
                border_radius: style.border_radius,
                border_width,
                border_color,
                transform: transform.clone(),
                shadow,
                z_index,
            });
        }

        // Check for rich text spans first
        if let Some(spans_property) = element.custom_properties.get("spans") {
            if let PropertyValue::RichText(rich_text) = spans_property {
                let mut text_color = style.text_color;
                text_color.w *= element.opacity;

                if text_color.w > 0.0 {
                    let text_z_index = element.z_index;
                    
                    commands.push(RenderCommand::DrawRichText {
                        position,
                        rich_text: rich_text.clone(),
                        max_width: Some(size.x),
                        max_height: Some(size.y),
                        default_color: text_color,
                        alignment: Some(element.text_alignment),
                        transform: transform.clone(),
                        z_index: text_z_index,
                    });
                }
            }
        }
        // Draw the text, if any (fallback for simple text).
        else if !element.text.is_empty() {
            let mut text_color = style.text_color;
            text_color.w *= element.opacity;

            if text_color.w > 0.0 {
                // Extract z_index from element properties
                let text_z_index = element.z_index;
                
                // The position for the text block is the same as the element's bounding box.
                // The renderer backend (e.g., Ratatui) will handle alignment within that box.
                eprintln!("[RENDER_TEXT] Element {}: text='{}', alignment={:?}, size={:?}", 
                    element.id, element.text, element.text_alignment, size);
                commands.push(RenderCommand::DrawText {
                    position, // Use the element's top-left corner.
                    text: element.text.clone(),
                    font_size: element.font_size,
                    color: text_color,
                    alignment: element.text_alignment,
                    max_width: Some(size.x), // The max width is the element's full width.
                    max_height: Some(size.y), // The max height is the element's full height.
                    transform: transform.clone(),
                    font_family: if element.font_family.is_empty() || element.font_family == "default" {
                        None
                    } else {
                        Some(element.font_family.clone())
                    },
                    z_index: text_z_index,
                });
            }
        }
        
        // Draw images for Image elements
        if element.element_type == ElementType::Image {
            if let Some(src_property) = element.custom_properties.get("src") {
                if let PropertyValue::String(image_source) = src_property {
                    commands.push(RenderCommand::DrawImage {
                        position,
                        size,
                        source: image_source.clone(),
                        opacity: element.opacity,
                        transform: transform.clone(),
                    });
                }
            }
        }
        
        // Handle Link elements - render similar to Text but with link styling
        if element.element_type == ElementType::Link {
            // Draw the background/border if specified (already done above)
            
            // Draw the link text with special styling
            if !element.text.is_empty() {
                let mut link_color = element.custom_properties.get("color")
                    .and_then(|v| v.as_color())
                    .unwrap_or(Vec4::new(0.0, 0.0, 1.0, 1.0)); // Default blue
                
                // Apply interaction state colors
                match element.current_state {
                    kryon_core::InteractionState::Hover => {
                        // Slightly lighter blue on hover
                        link_color = Vec4::new(0.2, 0.2, 1.0, 1.0);
                    }
                    kryon_core::InteractionState::Active => {
                        // Darker blue when active/pressed
                        link_color = Vec4::new(0.0, 0.0, 0.8, 1.0);
                    }
                    _ => {
                        // Use default or custom color
                    }
                }
                
                link_color.w *= element.opacity;
                
                if link_color.w > 0.0 {
                    // Extract z_index from element properties
                    let link_z_index = element.z_index;
                    
                    commands.push(RenderCommand::DrawText {
                        position,
                        text: element.text.clone(),
                        font_size: element.font_size,
                        color: link_color,
                        alignment: element.text_alignment,
                        max_width: Some(size.x),
                        max_height: Some(size.y),
                        transform: transform.clone(),
                        font_family: if element.font_family.is_empty() || element.font_family == "default" {
                            None
                        } else {
                            Some(element.font_family.clone())
                        },
                        z_index: link_z_index,
                    });
                }
            }
            
            // Skip the regular text rendering for Link elements
            return Ok(commands);
        }
        
        // Handle Input elements with different types
        if element.element_type == ElementType::Input {
            let input_type = element.custom_properties.get("input_type")
                .and_then(|v| if let PropertyValue::String(s) = v { Some(s.as_str()) } else { None })
                .unwrap_or("text"); // Default to text if no type specified
            
            match input_type {
                "text" | "password" | "email" | "number" | "tel" | "url" | "search" => {
                    // Get input-specific properties
                    let placeholder = element.custom_properties.get("placeholder")
                        .and_then(|v| if let PropertyValue::String(s) = v { Some(s.clone()) } else { None })
                        .unwrap_or_default();
                    
                    let is_readonly = element.custom_properties.get("readonly")
                        .and_then(|v| if let PropertyValue::Bool(b) = v { Some(*b) } else { None })
                        .unwrap_or(false);
                    
                    // Use text content as the input value
                    let input_text = element.text.clone();
                    
                    commands.push(RenderCommand::DrawTextInput {
                        position,
                        size,
                        text: input_text,
                        placeholder,
                        font_size: element.font_size,
                        text_color: style.text_color,
                        background_color: bg_color,
                        border_color,
                        border_width,
                        border_radius: style.border_radius,
                        is_focused: element.current_state == kryon_core::InteractionState::Focus,
                        is_readonly,
                        transform: transform.clone(),
                    });
                }
                "checkbox" | "radio" => {
                    let check_text = element.custom_properties.get("text")
                        .and_then(|v| if let PropertyValue::String(s) = v { Some(s.clone()) } else { None })
                        .unwrap_or_default();
                    
                    commands.push(RenderCommand::DrawCheckbox {
                        position,
                        size,
                        is_checked: element.current_state == kryon_core::InteractionState::Checked,
                        text: check_text,
                        font_size: element.font_size,
                        text_color: style.text_color,
                        background_color: bg_color,
                        border_color,
                        border_width,
                        check_color: style.text_color, // Use text color for checkmark
                        transform: transform.clone(),
                    });
                }
                "range" => {
                    let min_value = element.custom_properties.get("min")
                        .and_then(|v| v.as_float())
                        .unwrap_or(0.0);
                    
                    let max_value = element.custom_properties.get("max")
                        .and_then(|v| v.as_float())
                        .unwrap_or(100.0);
                    
                    let current_value = element.custom_properties.get("value")
                        .and_then(|v| v.as_float())
                        .unwrap_or(min_value);
                    
                    commands.push(RenderCommand::DrawSlider {
                        position,
                        size,
                        value: current_value,
                        min_value,
                        max_value,
                        track_color: bg_color,
                        thumb_color: style.text_color,
                        border_color,
                        border_width,
                        transform: transform.clone(),
                    });
                }
                _ => {
                    // For unsupported input types, render as text input
                    eprintln!("[RENDER] Warning: Unsupported input type '{}', rendering as text input", input_type);
                    commands.push(RenderCommand::DrawTextInput {
                        position,
                        size,
                        text: element.text.clone(),
                        placeholder: String::new(),
                        font_size: element.font_size,
                        text_color: style.text_color,
                        background_color: bg_color,
                        border_color,
                        border_width,
                        border_radius: style.border_radius,
                        is_focused: false,
                        is_readonly: false,
                        transform: transform.clone(),
                    });
                }
            }
            
            // Skip drawing the default background rect and text for Input elements
            // since the input-specific commands handle their own rendering
            return Ok(commands);
        }
        
        // Handle Canvas elements (native 2D/3D drawing)
        if element.element_type == ElementType::Canvas {
            let canvas_script = element.custom_properties.get("draw_script")
                .and_then(|v| if let PropertyValue::String(s) = v { Some(s.clone()) } else { None });
            
            commands.push(RenderCommand::EmbedView {
                view_id: element.id.clone(),
                view_type: "canvas".to_string(),
                source: canvas_script,
                position,
                size,
                config: element.custom_properties.clone(),
                z_index: element.z_index,
            });
            
            // Don't render anything else for Canvas
            return Ok(commands);
        }
        
        // Handle Video elements (native video playback)
        if element.element_type == ElementType::Video {
            let video_source = element.custom_properties.get("src")
                .and_then(|v| if let PropertyValue::String(s) = v { Some(s.clone()) } else { None });
            
            let autoplay = element.custom_properties.get("autoplay")
                .and_then(|v| if let PropertyValue::Bool(b) = v { Some(*b) } else { None })
                .unwrap_or(false);
            
            let muted = element.custom_properties.get("muted")
                .and_then(|v| if let PropertyValue::Bool(b) = v { Some(*b) } else { None })
                .unwrap_or(false);
            
            let loop_video = element.custom_properties.get("loop")
                .and_then(|v| if let PropertyValue::Bool(b) = v { Some(*b) } else { None })
                .unwrap_or(false);
            
            // TODO: Add native video render command
            // For now, render as a placeholder
            commands.push(RenderCommand::DrawRect {
                position,
                size,
                color: Vec4::new(0.1, 0.1, 0.1, 1.0), // Dark background
                border_radius: 4.0,
                border_width: 2.0,
                border_color: Vec4::new(0.3, 0.3, 0.3, 1.0),
                transform: None,
                shadow: None,
                z_index: element.z_index,
            });
            
            if let Some(src) = video_source {
                commands.push(RenderCommand::DrawText {
                    position: Vec2::new(position.x + size.x / 2.0 - 40.0, position.y + size.y / 2.0),
                    text: format!("🎬 Video: {}", src),
                    font_size: 14.0,
                    color: Vec4::new(0.8, 0.8, 0.8, 1.0),
                    alignment: TextAlignment::Center,
                    max_width: Some(size.x),
                    max_height: Some(size.y),
                    transform: None,
                    font_family: None,
                    z_index: element.z_index + 1,
                });
            }
            
            // Don't render anything else for Video
            return Ok(commands);
        }
        
        // Handle EmbedView elements (webview, wasm, native renderer emulation)
        if element.element_type == ElementType::EmbedView {
            let view_type = element.embed_view_type.as_ref()
                .or_else(|| element.custom_properties.get("type")
                    .and_then(|v| if let PropertyValue::String(s) = v { Some(s) } else { None }))
                .cloned()
                .unwrap_or_else(|| "webview".to_string()); // Default to webview
                
            let view_source = element.embed_view_source.as_ref()
                .or_else(|| element.custom_properties.get("source")
                    .and_then(|v| if let PropertyValue::String(s) = v { Some(s) } else { None }))
                .cloned();
            
            commands.push(RenderCommand::EmbedView {
                view_id: element.id.clone(),
                view_type,
                source: view_source,
                position,
                size,
                config: element.embed_view_config.clone(),
                z_index: element.z_index,
            });
            
            // Don't render anything else for EmbedView
            return Ok(commands);
        }

        Ok(commands)
    }

    pub fn resize(&mut self, new_size: Vec2) -> RenderResult<()> {
        self.viewport_size = new_size;
        self.backend.resize(new_size)
    }

    pub fn viewport_size(&self) -> Vec2 {
        self.viewport_size
    }

    pub fn backend(&self) -> &R {
        &self.backend
    }

    pub fn backend_mut(&mut self) -> &mut R {
        &mut self.backend
    }
}