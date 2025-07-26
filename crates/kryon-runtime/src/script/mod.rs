//! Modular script system for Kryon Runtime
//!
//! This module provides a comprehensive script execution system with support for:
//! - Multiple scripting languages (Lua, JavaScript, Python, Wren)
//! - Bytecode compilation and execution
//! - DOM API bridge for UI manipulation
//! - Reactive template variables
//! - Professional error handling
//!
//! # Architecture
//!
//! ```text
//! ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
//! │   ScriptSystem  │───▶│  ScriptEngine   │───▶│  Language VM    │
//! │   (Public API)  │    │     (Trait)     │    │   (Lua, JS...)  │
//! └─────────────────┘    └─────────────────┘    └─────────────────┘
//!           │                       │                       │
//!           ▼                       ▼                       ▼
//! ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
//! │   Registry      │    │   Bridge API    │    │  Reactive Vars  │
//! │  (Management)   │    │  (DOM, Events)  │    │  (Templates)    │
//! └─────────────────┘    └─────────────────┘    └─────────────────┘
//! ```

use std::collections::HashMap;
use anyhow::Result;
use kryon_core::{ScriptEntry, Element, ElementId, PropertyValue, KRBFile, TextAlignment};
use glam::{Vec2, Vec3, Vec4, Mat4};

pub mod engine_trait;
pub mod error;
pub mod registry;
pub mod lua;

use engine_trait::{ScriptValue, BridgeData, ChangeSet};
use error::ScriptError;
use registry::ScriptRegistry;
use serde_json;

/// Main script system coordinator
/// 
/// This is the primary interface for script execution in Kryon Runtime.
/// It manages multiple script engines, handles bytecode execution, and
/// coordinates changes between scripts and the UI system.
pub struct ScriptSystem {
    /// Registry of available script engines
    registry: ScriptRegistry,
    /// Template variables for reactive updates
    template_variables: HashMap<String, String>,
    /// Element data for DOM API
    elements_data: HashMap<ElementId, Element>,
    /// Style mappings from KRB file for bridge data creation
    style_mappings: HashMap<u8, kryon_core::Style>,
    /// Bridge data for setting up new engines
    bridge_data: Option<BridgeData>,
}

impl ScriptSystem {
    /// Create a new script system
    pub fn new() -> Result<Self> {
        let registry = ScriptRegistry::new()?;
        
        Ok(Self {
            registry,
            template_variables: HashMap::new(),
            elements_data: HashMap::new(),
            style_mappings: HashMap::new(),
            bridge_data: None,
        })
    }
    
    /// Initialize the script system with KRB file data
    pub fn initialize(&mut self, krb_file: &KRBFile, elements: &HashMap<ElementId, Element>) -> Result<()> {
        // Store style mappings from KRB file (only what we need for bridge data)
        self.style_mappings = krb_file.styles.clone();
        self.elements_data = elements.clone();
        
        // Initialize bridge data for all engines
        let bridge_data = self.create_bridge_data(krb_file, elements)?;
        self.registry.setup_bridge_for_all_engines(&bridge_data)?;
        
        // Store bridge data for future engines
        self.bridge_data = Some(bridge_data);
        
        Ok(())
    }
    
    /// Load compiled scripts from KRB file
    pub fn load_compiled_scripts(&mut self, scripts: &[ScriptEntry]) -> Result<()> {
        for script in scripts {
            self.load_compiled_script(script)?;
        }
        Ok(())
    }
    
    /// Load a single compiled script
    pub fn load_compiled_script(&mut self, script: &ScriptEntry) -> Result<()> {
        // Convert ScriptLanguage to string for engine lookup
        let language = &script.language;
        
        // Check if engine exists before creating it
        let engine_exists = self.registry.get_engine(language).is_some();
        
        // Get or create engine for this language
        let engine = self.registry.get_or_create_engine(language)?;
        
        // If engine was just created, set up the bridge
        if !engine_exists {
            if let Some(bridge_data) = &self.bridge_data {
                engine.setup_bridge(bridge_data)?;
            }
        }
        
        // Load bytecode into the engine
        engine.load_bytecode(&script.name, &script.bytecode)?;
        
        Ok(())
    }
    
    /// Execute a function with arguments
    pub fn call_function(&mut self, function_name: &str, args: Vec<PropertyValue>) -> Result<ScriptValue> {
        // Convert PropertyValue to ScriptValue
        let script_args: Vec<ScriptValue> = args.into_iter()
            .map(|pv| self.property_value_to_script_value(pv))
            .collect();
        
        // Try to find the function in any of the active engines
        let mut result = None;
        for engine in self.registry.get_all_engines_mut() {
            if engine.has_function(function_name) {
                result = Some(engine.call_function(function_name, script_args.clone())?);
                break;
            }
        }
        
        result.ok_or_else(|| ScriptError::FunctionNotFound {
            function: function_name.to_string(),
            available: self.get_all_function_names().join(", "),
        }.into())
    }
    
    /// Initialize template variables for reactive updates
    pub fn initialize_template_variables(&mut self, variables: &HashMap<String, String>) -> Result<()> {
        self.template_variables = variables.clone();
        
        // Initialize reactive variables in all engines
        for engine in self.registry.get_all_engines_mut() {
            engine.setup_reactive_variables(variables)?;
        }
        
        Ok(())
    }
    
    /// Execute initialization functions
    pub fn execute_init_functions(&mut self) -> Result<()> {
        // Execute onReady callbacks first
        for engine in self.registry.get_all_engines_mut() {
            engine.execute_on_ready_callbacks()?;
        }
        
        // Execute initialization functions (ending with "_init" or named "init")
        let function_names = self.get_all_function_names();
        for function_name in function_names {
            if function_name.ends_with("_init") || function_name == "init" {
                if let Err(e) = self.call_function(&function_name, vec![]) {
                    tracing::warn!("Failed to execute init function '{}': {}", function_name, e);
                }
            }
        }
        
        Ok(())
    }
    
    /// Get all pending changes from scripts
    pub fn get_pending_changes(&mut self) -> Result<HashMap<String, ChangeSet>> {
        let mut all_changes = HashMap::new();
        
        for engine in self.registry.get_all_engines_mut() {
            let engine_changes = engine.get_pending_changes()?;
            for (change_type, changes) in engine_changes {
                all_changes.insert(change_type, changes);
            }
        }
        
        Ok(all_changes)
    }
    
    /// Clear all pending changes from scripts
    pub fn clear_pending_changes(&mut self) -> Result<()> {
        for engine in self.registry.get_all_engines_mut() {
            engine.clear_pending_changes()?;
        }
        Ok(())
    }
    
    /// Apply pending changes to elements
    pub fn apply_pending_changes(&mut self, elements: &mut HashMap<ElementId, Element>) -> Result<bool> {
        let changes = self.get_pending_changes()?;
        self.apply_pending_dom_changes(elements, &changes)
    }
    
    /// Apply pending DOM changes from a given change set
    pub fn apply_pending_dom_changes(&mut self, elements: &mut HashMap<ElementId, Element>, changes: &HashMap<String, ChangeSet>) -> Result<bool> {
        let mut any_changes = false;
        
        // Apply style changes
        if let Some(style_changes) = changes.get("style_changes") {
            for (element_id_str, style_id_str) in &style_changes.data {
                if let (Ok(element_id), Ok(style_id)) = (element_id_str.parse::<ElementId>(), style_id_str.parse::<u8>()) {
                    if let Some(element) = elements.get_mut(&element_id) {
                        element.style_id = style_id;
                        any_changes = true;
                    }
                }
            }
        }
        
        // Apply text changes
        if let Some(text_changes) = changes.get("text_changes") {
            for (element_id_str, new_text) in &text_changes.data {
                if let Ok(element_id) = element_id_str.parse::<ElementId>() {
                    if let Some(element) = elements.get_mut(&element_id) {
                        element.text = new_text.clone();
                        any_changes = true;
                    }
                }
            }
        }
        
        // Apply visibility changes
        if let Some(visibility_changes) = changes.get("visibility_changes") {
            for (element_id_str, visible_str) in &visibility_changes.data {
                if let (Ok(element_id), Ok(visible)) = (element_id_str.parse::<ElementId>(), visible_str.parse::<bool>()) {
                    if let Some(element) = elements.get_mut(&element_id) {
                        element.visible = visible;
                        any_changes = true;
                    }
                }
            }
        }
        
        // Apply state changes (checked, etc.)
        if let Some(state_changes) = changes.get("state_changes") {
            for (element_id_str, checked_str) in &state_changes.data {
                if let (Ok(element_id), Ok(checked)) = (element_id_str.parse::<ElementId>(), checked_str.parse::<bool>()) {
                    if let Some(element) = elements.get_mut(&element_id) {
                        use kryon_core::InteractionState;
                        element.current_state = if checked {
                            InteractionState::Checked
                        } else {
                            InteractionState::Normal
                        };
                        any_changes = true;
                    }
                }
            }
        }
        
        // Refresh elements data in engines if changes were made
        if any_changes {
            self.elements_data = elements.clone();
            let bridge_data = self.create_bridge_data_from_stored(elements)?;
            self.registry.setup_bridge_for_all_engines(&bridge_data)?;
        }
        
        Ok(any_changes)
    }
    
    /// Get all function names from all engines
    fn get_all_function_names(&self) -> Vec<String> {
        let mut all_functions = Vec::new();
        for engine in self.registry.get_all_engines() {
            all_functions.extend(engine.get_function_names());
        }
        all_functions
    }
    
    /// Create bridge data for DOM API
    fn create_bridge_data(&self, krb_file: &KRBFile, elements: &HashMap<ElementId, Element>) -> Result<BridgeData> {
        // Create element ID mappings
        let mut element_ids = HashMap::new();
        for (element_id, element) in elements {
            if !element.id.is_empty() {
                element_ids.insert(element.id.clone(), *element_id);
            }
        }
        
        // Create style ID mappings
        let mut style_ids = HashMap::new();
        for (style_id, style) in &krb_file.styles {
            style_ids.insert(style.name.clone(), *style_id);
        }
        
        // Create component properties
        let mut component_properties = HashMap::new();
        for (element_id, element) in elements {
            if !element.custom_properties.is_empty() {
                let mut props = HashMap::new();
                for (prop_name, prop_value) in &element.custom_properties {
                    props.insert(prop_name.clone(), self.property_value_to_script_value(prop_value.clone()));
                }
                component_properties.insert(element_id.to_string(), props.clone());
                
                // Also store by string ID if available
                if !element.id.is_empty() {
                    component_properties.insert(element.id.clone(), props.clone());
                }
            }
        }
        
        Ok(BridgeData {
            element_ids,
            style_ids,
            component_properties,
            elements_data: elements.clone(),
            template_variables: self.template_variables.clone(),
        })
    }
    
    /// Create bridge data using stored style mappings
    fn create_bridge_data_from_stored(&self, elements: &HashMap<ElementId, Element>) -> Result<BridgeData> {
        // Create element ID mappings
        let mut element_ids = HashMap::new();
        for (element_id, element) in elements {
            if !element.id.is_empty() {
                element_ids.insert(element.id.clone(), *element_id);
            }
        }
        
        // Create style ID mappings using stored style mappings
        let mut style_ids = HashMap::new();
        for (style_id, style) in &self.style_mappings {
            style_ids.insert(style.name.clone(), *style_id);
        }
        
        // Create component properties
        let mut component_properties = HashMap::new();
        for (element_id, element) in elements {
            if !element.custom_properties.is_empty() {
                let mut props = HashMap::new();
                for (prop_name, prop_value) in &element.custom_properties {
                    props.insert(prop_name.clone(), self.property_value_to_script_value(prop_value.clone()));
                }
                component_properties.insert(element_id.to_string(), props.clone());
                
                // Also store by string ID if available
                if !element.id.is_empty() {
                    component_properties.insert(element.id.clone(), props.clone());
                }
            }
        }
        
        Ok(BridgeData {
            element_ids,
            style_ids,
            component_properties,
            elements_data: elements.clone(),
            template_variables: self.template_variables.clone(),
        })
    }
    
    /// Execute Canvas script and get drawing commands
    pub fn execute_canvas_script(&mut self, script_name: &str) -> Result<Vec<kryon_render::RenderCommand>> {
        let mut json_commands = Vec::new();
        
        // Find the Lua engine (Canvas scripts are primarily Lua-based)
        for engine in self.registry.get_all_engines_mut() {
            if let Some(lua_engine) = engine.as_any_mut().downcast_mut::<crate::script::lua::LuaEngine>() {
                // Execute the Canvas script
                lua_engine.execute_canvas_script(script_name)?;
                
                // Get the drawing commands from the Lua bridge
                json_commands = lua_engine.get_canvas_commands()?;
                
                // Clear the commands after retrieving them
                lua_engine.clear_canvas_commands()?;
                break;
            }
        }
        
        // Convert JSON commands to RenderCommand
        let mut commands = Vec::new();
        for json_cmd in json_commands {
            if let Some(render_cmd) = self.json_to_render_command(&json_cmd) {
                commands.push(render_cmd);
            }
        }
        
        Ok(commands)
    }
    
    /// Execute Canvas lifecycle hook (onLoad, onUpdate)
    pub fn execute_canvas_lifecycle_hook(&mut self, hook_name: &str, delta_time: Option<f64>) -> Result<()> {
        // Find the Lua engine (Canvas scripts are primarily Lua-based)
        for engine in self.registry.get_all_engines_mut() {
            if let Some(lua_engine) = engine.as_any_mut().downcast_mut::<crate::script::lua::LuaEngine>() {
                // Execute the lifecycle hook with optional delta time
                lua_engine.execute_canvas_lifecycle_hook(hook_name, delta_time)?;
                break;
            }
        }
        
        Ok(())
    }
    
    /// Convert JSON command to RenderCommand
    fn json_to_render_command(&self, json_cmd: &serde_json::Value) -> Option<kryon_render::RenderCommand> {
        use kryon_render::RenderCommand;
        
        let cmd_type = json_cmd.get("type")?.as_str()?;
        
        match cmd_type {
            "DrawCanvasLine" => {
                let start = self.json_to_vec2(json_cmd.get("start")?)?;
                let end = self.json_to_vec2(json_cmd.get("end")?)?;
                let color = self.json_to_vec4(json_cmd.get("color")?)?;
                let width = json_cmd.get("width")?.as_f64()? as f32;
                
                Some(RenderCommand::DrawCanvasLine { start, end, color, width })
            },
            "DrawCanvasRect" => {
                let position = self.json_to_vec2(json_cmd.get("position")?)?;
                let size = self.json_to_vec2(json_cmd.get("size")?)?;
                let fill_color = json_cmd.get("fill_color").and_then(|c| self.json_to_vec4(c));
                let stroke_color = json_cmd.get("stroke_color").and_then(|c| self.json_to_vec4(c));
                let stroke_width = json_cmd.get("stroke_width")?.as_f64()? as f32;
                
                Some(RenderCommand::DrawCanvasRect { position, size, fill_color, stroke_color, stroke_width })
            },
            "DrawCanvasCircle" => {
                let center = self.json_to_vec2(json_cmd.get("center")?)?;
                let radius = json_cmd.get("radius")?.as_f64()? as f32;
                let fill_color = json_cmd.get("fill_color").and_then(|c| self.json_to_vec4(c));
                let stroke_color = json_cmd.get("stroke_color").and_then(|c| self.json_to_vec4(c));
                let stroke_width = json_cmd.get("stroke_width")?.as_f64()? as f32;
                
                Some(RenderCommand::DrawCanvasCircle { center, radius, fill_color, stroke_color, stroke_width })
            },
            "DrawCanvasText" => {
                let text = json_cmd.get("text")?.as_str()?.to_string();
                let position = self.json_to_vec2(json_cmd.get("position")?)?;
                let font_size = json_cmd.get("font_size")?.as_f64()? as f32;
                let color = self.json_to_vec4(json_cmd.get("color")?)?;
                let font_family = json_cmd.get("font_family").and_then(|f| f.as_str().map(String::from));
                let alignment = json_cmd.get("alignment").and_then(|a| a.as_str()).unwrap_or("Left");
                
                let text_alignment = match alignment {
                    "Center" => TextAlignment::Center,
                    "Right" => TextAlignment::End,
                    "End" => TextAlignment::End,
                    _ => TextAlignment::Start,
                };
                
                Some(RenderCommand::DrawCanvasText { 
                    text, 
                    position, 
                    font_size, 
                    color, 
                    font_family, 
                    alignment: text_alignment 
                })
            },
            "DrawCanvasEllipse" => {
                let center = self.json_to_vec2(json_cmd.get("center")?)?;
                let rx = json_cmd.get("rx")?.as_f64()? as f32;
                let ry = json_cmd.get("ry")?.as_f64()? as f32;
                let fill_color = json_cmd.get("fill_color").and_then(|c| self.json_to_vec4(c));
                let stroke_color = json_cmd.get("stroke_color").and_then(|c| self.json_to_vec4(c));
                let stroke_width = json_cmd.get("stroke_width")?.as_f64()? as f32;
                
                Some(RenderCommand::DrawCanvasEllipse { center, rx, ry, fill_color, stroke_color, stroke_width })
            },
            "DrawCanvasPolygon" => {
                let points_json = json_cmd.get("points")?.as_array()?;
                let mut points = Vec::new();
                for point_json in points_json {
                    if let Some(point) = self.json_to_vec2(point_json) {
                        points.push(point);
                    }
                }
                let fill_color = json_cmd.get("fill_color").and_then(|c| self.json_to_vec4(c));
                let stroke_color = json_cmd.get("stroke_color").and_then(|c| self.json_to_vec4(c));
                let stroke_width = json_cmd.get("stroke_width")?.as_f64()? as f32;
                
                Some(RenderCommand::DrawCanvasPolygon { points, fill_color, stroke_color, stroke_width })
            },
            "DrawCanvasPath" => {
                let path_data = json_cmd.get("path_data")?.as_str()?.to_string();
                let fill_color = json_cmd.get("fill_color").and_then(|c| self.json_to_vec4(c));
                let stroke_color = json_cmd.get("stroke_color").and_then(|c| self.json_to_vec4(c));
                let stroke_width = json_cmd.get("stroke_width")?.as_f64()? as f32;
                
                Some(RenderCommand::DrawCanvasPath { path_data, fill_color, stroke_color, stroke_width })
            },
            "DrawCanvasImage" => {
                let source = json_cmd.get("source")?.as_str()?.to_string();
                let position = self.json_to_vec2(json_cmd.get("position")?)?;
                let size = self.json_to_vec2(json_cmd.get("size")?)?;
                let opacity = json_cmd.get("opacity")?.as_f64()? as f32;
                
                Some(RenderCommand::DrawCanvasImage { source, position, size, opacity })
            },
            // 3D Canvas commands
            "BeginCanvas3D" => {
                let camera = json_cmd.get("camera")?;
                let camera_data = kryon_render::CameraData {
                    position: self.json_to_vec3(camera.get("position")?)?,
                    target: self.json_to_vec3(camera.get("target")?)?,
                    up: self.json_to_vec3(camera.get("up")?)?,
                    fov: camera.get("fov")?.as_f64()? as f32,
                    near_plane: camera.get("near_plane")?.as_f64()? as f32,
                    far_plane: camera.get("far_plane")?.as_f64()? as f32,
                };
                
                Some(RenderCommand::BeginCanvas3D {
                    canvas_id: "default".to_string(),
                    position: Vec2::new(0.0, 0.0), // Will be set by Canvas element
                    size: Vec2::new(800.0, 600.0), // Will be set by Canvas element
                    camera: camera_data,
                })
            },
            "EndCanvas3D" => {
                Some(RenderCommand::EndCanvas3D)
            },
            "DrawCanvas3DCube" => {
                let position = self.json_to_vec3(json_cmd.get("position")?)?;
                let size = self.json_to_vec3(json_cmd.get("size")?)?;
                let color = self.json_to_vec4(json_cmd.get("color")?)?;
                let wireframe = json_cmd.get("wireframe")?.as_bool().unwrap_or(false);
                
                Some(RenderCommand::DrawCanvas3DCube { position, size, color, wireframe })
            },
            "DrawCanvas3DSphere" => {
                let center = self.json_to_vec3(json_cmd.get("center")?)?;
                let radius = json_cmd.get("radius")?.as_f64()? as f32;
                let color = self.json_to_vec4(json_cmd.get("color")?)?;
                let wireframe = json_cmd.get("wireframe")?.as_bool().unwrap_or(false);
                
                Some(RenderCommand::DrawCanvas3DSphere { center, radius, color, wireframe })
            },
            "DrawCanvas3DPlane" => {
                let center = self.json_to_vec3(json_cmd.get("center")?)?;
                let size = self.json_to_vec2(json_cmd.get("size")?)?;
                let normal = self.json_to_vec3(json_cmd.get("normal")?)?;
                let color = self.json_to_vec4(json_cmd.get("color")?)?;
                
                Some(RenderCommand::DrawCanvas3DPlane { center, size, normal, color })
            },
            "DrawCanvas3DMesh" => {
                let vertices_json = json_cmd.get("vertices")?.as_array()?;
                let mut vertices = Vec::new();
                for vertex_json in vertices_json {
                    if let Some(vertex) = self.json_to_vec3(vertex_json) {
                        vertices.push(vertex);
                    }
                }
                
                let indices_json = json_cmd.get("indices")?.as_array()?;
                let mut indices = Vec::new();
                for index_json in indices_json {
                    if let Some(index) = index_json.as_u64() {
                        indices.push(index as u32);
                    }
                }
                
                let normals_json = json_cmd.get("normals")?.as_array()?;
                let mut normals = Vec::new();
                for normal_json in normals_json {
                    if let Some(normal) = self.json_to_vec3(normal_json) {
                        normals.push(normal);
                    }
                }
                
                let uvs_json = json_cmd.get("uvs")?.as_array()?;
                let mut uvs = Vec::new();
                for uv_json in uvs_json {
                    if let Some(uv) = self.json_to_vec2(uv_json) {
                        uvs.push(uv);
                    }
                }
                
                let color = self.json_to_vec4(json_cmd.get("color")?)?;
                let texture = json_cmd.get("texture").and_then(|t| t.as_str().map(String::from));
                
                Some(RenderCommand::DrawCanvas3DMesh { vertices, indices, normals, uvs, color, texture })
            },
            "SetCanvas3DLighting" => {
                let ambient_color = self.json_to_vec4(json_cmd.get("ambient_color")?)?;
                
                let directional_light = json_cmd.get("directional_light").and_then(|dl| {
                    let direction = self.json_to_vec3(dl.get("direction")?);
                    let color = self.json_to_vec4(dl.get("color")?);
                    let intensity = dl.get("intensity")?.as_f64()? as f32;
                    
                    if let (Some(direction), Some(color)) = (direction, color) {
                        Some(kryon_render::DirectionalLight { direction, color, intensity })
                    } else {
                        None
                    }
                });
                
                let point_lights_json = json_cmd.get("point_lights")?.as_array()?;
                let mut point_lights = Vec::new();
                for pl_json in point_lights_json {
                    if let (Some(position), Some(color)) = (
                        self.json_to_vec3(pl_json.get("position")?),
                        self.json_to_vec4(pl_json.get("color")?)
                    ) {
                        let intensity = pl_json.get("intensity")?.as_f64()? as f32;
                        let range = pl_json.get("range")?.as_f64()? as f32;
                        
                        point_lights.push(kryon_render::PointLight {
                            position,
                            color,
                            intensity,
                            range,
                        });
                    }
                }
                
                Some(RenderCommand::SetCanvas3DLighting { ambient_color, directional_light, point_lights })
            },
            "ApplyCanvas3DTransform" => {
                let transform = json_cmd.get("transform")?;
                let translation = self.json_to_vec3(transform.get("translation")?)?;
                let rotation = self.json_to_vec3(transform.get("rotation")?)?;
                let scale = self.json_to_vec3(transform.get("scale")?)?;
                
                // Create transformation matrix from translation, rotation, and scale
                let transform_matrix = Mat4::from_scale_rotation_translation(
                    scale,
                    glam::Quat::from_euler(glam::EulerRot::XYZ, 
                        rotation.x.to_radians(), 
                        rotation.y.to_radians(), 
                        rotation.z.to_radians()),
                    translation,
                );
                
                Some(RenderCommand::ApplyCanvas3DTransform { transform: transform_matrix })
            },
            _ => None,
        }
    }
    
    /// Convert JSON object to Vec2
    fn json_to_vec2(&self, json: &serde_json::Value) -> Option<Vec2> {
        let x = json.get("x")?.as_f64()? as f32;
        let y = json.get("y")?.as_f64()? as f32;
        Some(Vec2::new(x, y))
    }
    
    /// Convert JSON object to Vec3
    fn json_to_vec3(&self, json: &serde_json::Value) -> Option<Vec3> {
        let x = json.get("x")?.as_f64()? as f32;
        let y = json.get("y")?.as_f64()? as f32;
        let z = json.get("z")?.as_f64()? as f32;
        Some(Vec3::new(x, y, z))
    }
    
    /// Convert JSON object to Vec4
    fn json_to_vec4(&self, json: &serde_json::Value) -> Option<Vec4> {
        let r = json.get("r")?.as_f64()? as f32;
        let g = json.get("g")?.as_f64()? as f32;
        let b = json.get("b")?.as_f64()? as f32;
        let a = json.get("a")?.as_f64()? as f32;
        Some(Vec4::new(r, g, b, a))
    }
    
    /// Convert PropertyValue to ScriptValue
    fn property_value_to_script_value(&self, value: PropertyValue) -> ScriptValue {
        match value {
            PropertyValue::String(s) => ScriptValue::String(s),
            PropertyValue::Int(i) => ScriptValue::Integer(i as i64),
            PropertyValue::Float(f) => ScriptValue::Number(f as f64),
            PropertyValue::Bool(b) => ScriptValue::Boolean(b),
            PropertyValue::Percentage(p) => ScriptValue::Number(p as f64),
            PropertyValue::Color(color) => {
                let hex = format!("#{:02X}{:02X}{:02X}{:02X}",
                    (color.x * 255.0) as u8,
                    (color.y * 255.0) as u8,
                    (color.z * 255.0) as u8,
                    (color.w * 255.0) as u8
                );
                ScriptValue::String(hex)
            },
            PropertyValue::Resource(res) => ScriptValue::String(res),
            PropertyValue::Transform(_) => ScriptValue::String(format!("{:?}", value)),
            PropertyValue::CSSUnit(css_unit) => ScriptValue::Number(css_unit.value as f64),
            PropertyValue::RichText(rich_text) => ScriptValue::String(rich_text.to_plain_text()),
        }
    }
}

impl Default for ScriptSystem {
    fn default() -> Self {
        Self::new().expect("Failed to create default ScriptSystem")
    }
}

// Extension trait to convert ScriptLanguage to string
/// Supported script languages
#[derive(Debug, Clone, PartialEq)]
pub enum ScriptLanguage {
    Lua,
    JavaScript,
    Python,
    Wren,
}

impl ScriptLanguage {
    #[allow(dead_code)]
    fn as_str(&self) -> &'static str {
        match self {
            ScriptLanguage::Lua => "lua",
            ScriptLanguage::JavaScript => "javascript",
            ScriptLanguage::Python => "python",
            ScriptLanguage::Wren => "wren",
        }
    }
}
