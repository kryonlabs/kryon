use glam::{Vec2, Vec3, Vec4, Mat4};
use std::collections::HashMap;
use kryon_core::{ElementId, PropertyValue, TextAlignment, TransformData};

use crate::{CameraData, DirectionalLight, PointLight, ScrollbarOrientation};

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
    
    // Basic 2D shapes (standalone, not canvas-specific)
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