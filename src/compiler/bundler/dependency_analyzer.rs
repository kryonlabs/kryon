//! Dependency analysis for smart bundling
//! 
//! This module analyzes the compiled app to determine:
//! - Which EmbedView types are actually used
//! - Which script languages are used
//! - Which resources (fonts, images) are referenced
//! - Which styles are applied

use std::collections::{HashSet, HashMap};
use anyhow::Result;

use crate::compiler::CompilerState;
use crate::core::types::ElementType;
use crate::core::state::Element;
use crate::compiler::frontend::ast::PropertyValue;
use crate::error::CompilerError;

/// Collected dependency information
#[derive(Debug, Clone)]
pub struct AppDependencies {
    /// EmbedView types used (e.g., "webview", "wasm", "native_renderer")
    pub embed_view_types: HashSet<String>,
    /// Script languages used (e.g., "lua", "javascript", "python")
    pub script_languages: HashSet<String>,
    /// Resources referenced (fonts, images, etc.)
    pub referenced_resources: HashMap<String, ResourceReference>,
    /// Styles actually applied to elements
    pub used_styles: HashSet<String>,
    /// Element types used in the app
    pub used_element_types: HashSet<ElementType>,
}

#[derive(Debug, Clone)]
pub struct ResourceReference {
    pub resource_type: ResourceType,
    pub path: String,
    pub used_by: Vec<String>, // Element IDs that use this resource
}

#[derive(Debug, Clone, PartialEq)]
pub enum ResourceType {
    Font,
    Image,
    Video,
    Audio,
    WasmModule,
    Script,
    Stylesheet,
    Other(String),
}

pub struct DependencyAnalyzer;

impl DependencyAnalyzer {
    pub fn new() -> Self {
        Self
    }
    
    /// Analyze the compiled app to determine dependencies
    pub fn analyze(&self, state: &CompilerState) -> Result<AppDependencies> {
        let mut dependencies = AppDependencies {
            embed_view_types: HashSet::new(),
            script_languages: HashSet::new(),
            referenced_resources: HashMap::new(),
            used_styles: HashSet::new(),
            used_element_types: HashSet::new(),
        };
        
        // Analyze elements
        self.analyze_elements(state, &mut dependencies)?;
        
        // Analyze scripts
        self.analyze_scripts(state, &mut dependencies)?;
        
        // Analyze styles
        self.analyze_styles(state, &mut dependencies)?;
        
        Ok(dependencies)
    }
    
    /// Analyze elements to find EmbedView types and resource references
    fn analyze_elements(&self, state: &CompilerState, deps: &mut AppDependencies) -> Result<()> {
        for element in &state.elements {
            // Track element types used
            deps.used_element_types.insert(element.element_type);
            
            // Analyze EmbedView elements
            if element.element_type == ElementType::EmbedView {
                self.analyze_embed_view_element(element, deps)?;
            }
            
            // Analyze Canvas elements
            if element.element_type == ElementType::Canvas {
                self.analyze_canvas_element(element, deps)?;
            }
            
            // Analyze Image elements
            if element.element_type == ElementType::Image {
                self.analyze_image_element(element, deps)?;
            }
            
            // Analyze Video elements
            if element.element_type == ElementType::Video {
                self.analyze_video_element(element, deps)?;
            }
            
            // Track font usage - simplified for now since font_family is not in Element struct
            // This would need to be extracted from the element's properties
            // self.add_font_reference(&element.font_family, &element.source_id_name, deps);
        }
        
        Ok(())
    }
    
    /// Analyze EmbedView element to determine ViewProvider type
    fn analyze_embed_view_element(&self, element: &Element, deps: &mut AppDependencies) -> Result<()> {
        // Check for explicit type property in custom properties
        for custom_prop in &element.krb_custom_properties {
            // We need to look up the property name from the string table
            // For now, let's use a simplified approach based on common patterns
            if let Ok(prop_value) = self.decode_custom_property_value(custom_prop) {
                if let PropertyValue::String(view_type) = prop_value {
                    // Common EmbedView type names
                    if ["webview", "wasm", "native_renderer", "gba", "dos", "uxn", "code_editor", "terminal", "model_viewer"].contains(&view_type.as_str()) {
                        deps.embed_view_types.insert(view_type);
                    }
                }
            }
        }
        
        // Check source properties for additional dependencies
        for custom_prop in &element.krb_custom_properties {
            if let Ok(prop_value) = self.decode_custom_property_value(custom_prop) {
                if let PropertyValue::String(source) = prop_value {
                    // Look for sources that indicate specific ViewProvider types
                    if source.ends_with(".wasm") {
                        deps.embed_view_types.insert("wasm".to_string());
                    } else if source.starts_with("http://") || source.starts_with("https://") {
                        deps.embed_view_types.insert("webview".to_string());
                    } else if source.ends_with(".gba") || source.ends_with(".gb") {
                        deps.embed_view_types.insert("gba".to_string());
                    } else if source.ends_with(".exe") || source.ends_with(".com") {
                        deps.embed_view_types.insert("dos".to_string());
                    } else if source.ends_with(".rom") {
                        deps.embed_view_types.insert("uxn".to_string());
                    }
                    
                    self.analyze_embed_view_source(&source, &element.source_id_name, deps)?;
                }
            }
        }
        
        Ok(())
    }
    
    /// Analyze EmbedView source to detect resource dependencies
    fn analyze_embed_view_source(&self, source: &str, element_id: &str, deps: &mut AppDependencies) -> Result<()> {
        let resource_type = self.detect_resource_type(source);
        
        if matches!(resource_type, ResourceType::WasmModule) {
            deps.embed_view_types.insert("wasm".to_string());
        }
        
        deps.referenced_resources.insert(source.to_string(), ResourceReference {
            resource_type,
            path: source.to_string(),
            used_by: vec![element_id.to_string()],
        });
        
        Ok(())
    }
    
    /// Analyze Canvas element for script dependencies
    fn analyze_canvas_element(&self, element: &Element, deps: &mut AppDependencies) -> Result<()> {
        // Check for draw_script property in custom properties
        for custom_prop in &element.krb_custom_properties {
            if let Ok(prop_value) = self.decode_custom_property_value(custom_prop) {
                if let PropertyValue::String(script_name) = prop_value {
                    // Canvas scripts are typically Lua for now
                    deps.script_languages.insert("lua".to_string());
                }
            }
        }
        
        // Check for lifecycle hooks - Canvas elements often have Lua scripts
        if !element.krb_custom_properties.is_empty() {
            deps.script_languages.insert("lua".to_string());
        }
        
        Ok(())
    }
    
    /// Analyze Image element for image dependencies
    fn analyze_image_element(&self, element: &Element, deps: &mut AppDependencies) -> Result<()> {
        // Check for src property in custom properties
        for custom_prop in &element.krb_custom_properties {
            if let Ok(prop_value) = self.decode_custom_property_value(custom_prop) {
                if let PropertyValue::String(src) = prop_value {
                    deps.referenced_resources.insert(src.clone(), ResourceReference {
                        resource_type: ResourceType::Image,
                        path: src.clone(),
                        used_by: vec![element.source_id_name.clone()],
                    });
                }
            }
        }
        
        Ok(())
    }
    
    /// Analyze Video element for video dependencies
    fn analyze_video_element(&self, element: &Element, deps: &mut AppDependencies) -> Result<()> {
        // Check for src property in custom properties
        for custom_prop in &element.krb_custom_properties {
            if let Ok(prop_value) = self.decode_custom_property_value(custom_prop) {
                if let PropertyValue::String(src) = prop_value {
                    deps.referenced_resources.insert(src.clone(), ResourceReference {
                        resource_type: ResourceType::Video,
                        path: src.clone(),
                        used_by: vec![element.source_id_name.clone()],
                    });
                }
            }
        }
        
        Ok(())
    }
    
    /// Analyze scripts to determine required languages
    fn analyze_scripts(&self, state: &CompilerState, deps: &mut AppDependencies) -> Result<()> {
        for script in &state.scripts {
            let language = self.detect_script_language_from_id(script.language_id)?;
            deps.script_languages.insert(language);
            
            // Analyze script content for additional dependencies
            // Convert code_data back to string for analysis (simplified)
            let script_content = String::from_utf8_lossy(&script.code_data);
            self.analyze_script_content(&script_content, deps)?;
        }
        
        Ok(())
    }
    
    /// Analyze script content for resource references
    fn analyze_script_content(&self, content: &str, deps: &mut AppDependencies) -> Result<()> {
        // Look for resource references in script code
        // This is a simplified analysis - in practice, you'd want more sophisticated parsing
        
        // Look for common resource patterns
        let patterns = [
            (r#"loadImage\("([^"]+)"\)"#, ResourceType::Image),
            (r#"loadSound\("([^"]+)"\)"#, ResourceType::Audio),
            (r#"loadFont\("([^"]+)"\)"#, ResourceType::Font),
            (r#"loadWasm\("([^"]+)"\)"#, ResourceType::WasmModule),
        ];
        
        for (pattern, resource_type) in patterns {
            if let Ok(regex) = regex::Regex::new(pattern) {
                for capture in regex.captures_iter(content) {
                    if let Some(resource_path) = capture.get(1) {
                        deps.referenced_resources.insert(resource_path.as_str().to_string(), ResourceReference {
                            resource_type: resource_type.clone(),
                            path: resource_path.as_str().to_string(),
                            used_by: vec!["script".to_string()],
                        });
                    }
                }
            }
        }
        
        Ok(())
    }
    
    /// Analyze styles to determine which are actually used
    fn analyze_styles(&self, state: &CompilerState, deps: &mut AppDependencies) -> Result<()> {
        // Collect all style names referenced by elements
        for element in &state.elements {
            if element.style_id > 0 {
                if let Some(style) = state.styles.iter()
                    .find(|style| style.id == element.style_id) {
                    deps.used_styles.insert(style.source_name.clone());
                }
            }
        }
        
        Ok(())
    }
    
    /// Detect resource type from file extension or content
    fn detect_resource_type(&self, path: &str) -> ResourceType {
        let path_lower = path.to_lowercase();
        
        if path_lower.ends_with(".wasm") {
            ResourceType::WasmModule
        } else if path_lower.ends_with(".png") || path_lower.ends_with(".jpg") || path_lower.ends_with(".jpeg") || path_lower.ends_with(".gif") || path_lower.ends_with(".svg") {
            ResourceType::Image
        } else if path_lower.ends_with(".mp4") || path_lower.ends_with(".webm") || path_lower.ends_with(".ogg") {
            ResourceType::Video
        } else if path_lower.ends_with(".mp3") || path_lower.ends_with(".wav") || path_lower.ends_with(".flac") {
            ResourceType::Audio
        } else if path_lower.ends_with(".ttf") || path_lower.ends_with(".otf") || path_lower.ends_with(".woff") || path_lower.ends_with(".woff2") {
            ResourceType::Font
        } else if path_lower.ends_with(".js") || path_lower.ends_with(".lua") || path_lower.ends_with(".py") || path_lower.ends_with(".wren") {
            ResourceType::Script
        } else if path_lower.ends_with(".css") {
            ResourceType::Stylesheet
        } else {
            ResourceType::Other(path.to_string())
        }
    }
    
    /// Detect script language from file extension or explicit language
    fn detect_script_language(&self, language: &str) -> Result<String> {
        match language.to_lowercase().as_str() {
            "lua" => Ok("lua".to_string()),
            "javascript" | "js" => Ok("javascript".to_string()),
            "python" | "py" => Ok("python".to_string()),
            "wren" => Ok("wren".to_string()),
            _ => Ok("lua".to_string()), // Default to Lua
        }
    }
    
    /// Detect script language from ScriptLanguage enum
    fn detect_script_language_from_id(&self, language_id: crate::core::types::ScriptLanguage) -> Result<String> {
        match language_id {
            crate::core::types::ScriptLanguage::Lua => Ok("lua".to_string()),
            crate::core::types::ScriptLanguage::JavaScript => Ok("javascript".to_string()),
            crate::core::types::ScriptLanguage::Python => Ok("python".to_string()),
            crate::core::types::ScriptLanguage::Wren => Ok("wren".to_string()),
        }
    }
    
    /// Add a font reference to dependencies
    fn add_font_reference(&self, font_family: &str, element_id: &str, deps: &mut AppDependencies) {
        // Try to find the font file
        let font_path = format!("{}.ttf", font_family); // Simplified
        
        deps.referenced_resources.entry(font_path.clone()).or_insert_with(|| ResourceReference {
            resource_type: ResourceType::Font,
            path: font_path,
            used_by: vec![],
        }).used_by.push(element_id.to_string());
    }
    
    /// Decode a custom property value from its KRB format
    /// This is a simplified implementation - in a full implementation,
    /// we'd need to decode the binary format properly
    fn decode_custom_property_value(&self, custom_prop: &crate::core::state::KrbCustomProperty) -> Result<PropertyValue> {
        // For now, we'll make a best-effort attempt to decode string values
        // This is simplified - a full implementation would need proper decoding
        match custom_prop.value_type {
            crate::core::types::ValueType::String => {
                // Try to decode as UTF-8 string
                let string_value = String::from_utf8(custom_prop.value.clone())
                    .unwrap_or_else(|_| "".to_string());
                Ok(PropertyValue::String(string_value))
            }
            _ => {
                // For other types, we can't easily decode without more context
                // Return a placeholder that won't match our patterns
                Ok(PropertyValue::String("".to_string()))
            }
        }
    }
}

impl Default for DependencyAnalyzer {
    fn default() -> Self {
        Self::new()
    }
}