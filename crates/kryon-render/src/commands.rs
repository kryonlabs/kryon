use glam::{Vec2, Vec3, Vec4, Mat4};
use std::collections::HashMap;
use kryon_core::{ElementId, PropertyValue, TextAlignment, TransformData};

use crate::{CameraData, DirectionalLight, PointLight, ScrollbarOrientation};

/// Layout style information for containers (flexbox, grid, etc.)
#[derive(Debug, Clone)]
pub struct LayoutStyle {
    /// Display type (flex, block, grid, etc.)
    pub display: Option<String>,
    /// Flex direction (row, column, row-reverse, column-reverse)
    pub flex_direction: Option<String>,
    /// Justify content (flex-start, center, flex-end, space-between, space-around, space-evenly)
    pub justify_content: Option<String>,
    /// Align items (flex-start, center, flex-end, stretch, baseline)
    pub align_items: Option<String>,
    /// Align content (for multi-line flex containers)
    pub align_content: Option<String>,
    /// Flex wrap (wrap, nowrap, wrap-reverse)
    pub flex_wrap: Option<String>,
    /// Gap between flex items
    pub gap: Option<f32>,
    /// Row gap
    pub row_gap: Option<f32>,
    /// Column gap
    pub column_gap: Option<f32>,
}

impl Default for LayoutStyle {
    fn default() -> Self {
        Self {
            display: None,
            flex_direction: None,
            justify_content: None,
            align_items: None,
            align_content: None,
            flex_wrap: None,
            gap: None,
            row_gap: None,
            column_gap: None,
        }
    }
}

impl LayoutStyle {
    /// Create a new LayoutStyle from element's custom properties
    pub fn from_element_properties(custom_properties: &HashMap<String, PropertyValue>) -> Self {
        let mut style = Self::default();
        
        // Debug all custom properties
        eprintln!("🔍 [LAYOUT_STYLE_DEBUG] Custom properties for element:");
        for (key, value) in custom_properties {
            eprintln!("🔍 [LAYOUT_STYLE_DEBUG]   {} = {:?}", key, value);
        }
        
        if let Some(PropertyValue::String(display)) = custom_properties.get("display") {
            eprintln!("✅ [LAYOUT_STYLE_DEBUG] Found display property: '{}'", display);
            style.display = Some(display.clone());
        } else {
            eprintln!("❌ [LAYOUT_STYLE_DEBUG] No display property found or not a string");
        }
        
        if let Some(PropertyValue::String(flex_direction)) = custom_properties.get("flex_direction") {
            style.flex_direction = Some(flex_direction.clone());
        }
        
        if let Some(PropertyValue::String(justify_content)) = custom_properties.get("justify_content") {
            eprintln!("✅ [LAYOUT_STYLE_DEBUG] Found justify_content property: '{}'", justify_content);
            style.justify_content = Some(justify_content.clone());
        } else {
            eprintln!("❌ [LAYOUT_STYLE_DEBUG] No justify_content property found or not a string");
        }
        
        if let Some(PropertyValue::String(align_items)) = custom_properties.get("align_items") {
            eprintln!("✅ [LAYOUT_STYLE_DEBUG] Found align_items property: '{}'", align_items);
            style.align_items = Some(align_items.clone());
        } else {
            eprintln!("❌ [LAYOUT_STYLE_DEBUG] No align_items property found or not a string");
        }
        
        if let Some(PropertyValue::String(align_content)) = custom_properties.get("align_content") {
            style.align_content = Some(align_content.clone());
        }
        
        if let Some(PropertyValue::String(flex_wrap)) = custom_properties.get("flex_wrap") {
            style.flex_wrap = Some(flex_wrap.clone());
        }
        
        if let Some(PropertyValue::Float(gap)) = custom_properties.get("gap") {
            style.gap = Some(*gap);
        }
        
        if let Some(PropertyValue::Float(row_gap)) = custom_properties.get("row_gap") {
            style.row_gap = Some(*row_gap);
        }
        
        if let Some(PropertyValue::Float(column_gap)) = custom_properties.get("column_gap") {
            style.column_gap = Some(*column_gap);
        }
        
        style
    }
}

/// All rendering commands that can be issued to a backend
#[derive(Debug, Clone)]
pub enum RenderCommand {
    // Basic drawing commands
    DrawRect {
        position: Vec2,
        size: Vec2,
        color: Vec4,
        border_radius: f32,
        border_width: f32,
        border_color: Vec4,
        transform: Option<TransformData>,
        shadow: Option<String>,
        layout_style: Option<LayoutStyle>,
        z_index: i32,
    },
    
    // Container hierarchy commands for HTML rendering
    BeginContainer {
        element_id: String,
        position: Vec2,
        size: Vec2,
        color: Vec4,
        border_radius: f32,
        border_width: f32,
        border_color: Vec4,
        layout_style: Option<LayoutStyle>,
        z_index: i32,
    },
    EndContainer,
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
    DrawLine {
        start: Vec2,
        end: Vec2,
        color: Vec4,
        width: f32,
    },
    DrawTriangle {
        points: Vec<Vec2>,
        color: Vec4,
    },
    DrawSvg {
        position: Vec2,
        size: Vec2,
        source: String, // File path or inline SVG content
        opacity: f32,
        transform: Option<TransformData>,
        background_color: Option<Vec4>, // Optional background color
        z_index: i32,
    },
    
    // Clipping
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
    
    // Input controls
    DrawTextInput {
        position: Vec2,
        size: Vec2,
        text: String,
        placeholder: String,
        font_size: f32,
        text_color: Vec4,
        placeholder_color: Vec4,
        background_color: Vec4,
        border_color: Vec4,
        focus_border_color: Vec4,
        border_width: f32,
        border_radius: f32,
        is_focused: bool,
        is_editing: bool,
        is_readonly: bool,
        cursor_position: usize,
        selection_start: Option<usize>,
        selection_end: Option<usize>,
        text_scroll_offset: f32,
        input_type: u8, // Maps to InputType enum
        transform: Option<TransformData>,
        z_index: i32,
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
    DrawSelectButton {
        position: Vec2,
        size: Vec2,
        text: String,
        is_open: bool,
        font_size: f32,
        text_color: Vec4,
        background_color: Vec4,
        border_color: Vec4,
        border_width: f32,
        transform: Option<TransformData>,
    },
    DrawSelectMenu {
        position: Vec2,
        size: Vec2,
        options: Vec<kryon_core::SelectOption>,
        highlighted_index: Option<usize>,
        font_size: f32,
        text_color: Vec4,
        background_color: Vec4,
        border_color: Vec4,
        border_width: f32,
        highlight_color: Vec4,
        selected_color: Vec4,
        transform: Option<TransformData>,
    },
    
    // Basic 2D shapes (standalone, not canvas-specific)
    DrawLineWithTransform {
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
    
    // Canvas-specific 2D commands
    BeginCanvas {
        canvas_id: String,
        position: Vec2,
        size: Vec2,
    },
    EndCanvas,
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
    DrawCanvasEllipse {
        center: Vec2,
        rx: f32,
        ry: f32,
        fill_color: Option<Vec4>,
        stroke_color: Option<Vec4>,
        stroke_width: f32,
    },
    DrawCanvasPolygon {
        points: Vec<Vec2>,
        fill_color: Option<Vec4>,
        stroke_color: Option<Vec4>,
        stroke_width: f32,
    },
    DrawCanvasPath {
        path_data: String,
        fill_color: Option<Vec4>,
        stroke_color: Option<Vec4>,
        stroke_width: f32,
    },
    DrawCanvasImage {
        source: String,
        position: Vec2,
        size: Vec2,
        opacity: f32,
    },
    
    // 3D Canvas commands
    BeginCanvas3D {
        canvas_id: String,
        position: Vec2,
        size: Vec2,
        camera: CameraData,
    },
    EndCanvas3D,
    DrawCanvas3DCube {
        position: Vec3,
        size: Vec3,
        color: Vec4,
        wireframe: bool,
    },
    DrawCanvas3DSphere {
        center: Vec3,
        radius: f32,
        color: Vec4,
        wireframe: bool,
    },
    DrawCanvas3DPlane {
        center: Vec3,
        size: Vec2,
        normal: Vec3,
        color: Vec4,
    },
    DrawCanvas3DMesh {
        vertices: Vec<Vec3>,
        indices: Vec<u32>,
        normals: Vec<Vec3>,
        uvs: Vec<Vec2>,
        color: Vec4,
        texture: Option<String>,
    },
    SetCanvas3DLighting {
        ambient_color: Vec4,
        directional_light: Option<DirectionalLight>,
        point_lights: Vec<PointLight>,
    },
    ApplyCanvas3DTransform {
        transform: Mat4,
    },
    
    // WASM View commands
    BeginWasmView {
        wasm_id: String,
        position: Vec2,
        size: Vec2,
    },
    EndWasmView,
    ExecuteWasmFunction {
        function_name: String,
        params: Vec<f64>, // Simple numeric parameters for now
    },
}