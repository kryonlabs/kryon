//! Advanced style inheritance and resolution system

use crate::error::{CompilerError, Result};
use crate::core::*;
use crate::core::types::*;
use std::collections::{HashMap, VecDeque};

pub struct StyleResolver {
    resolution_cache: HashMap<String, Vec<KrbProperty>>,
}

impl StyleResolver {
    pub fn new() -> Self {
        Self {
            resolution_cache: HashMap::new(),
        }
    }
    
    pub fn resolve_all_styles(&mut self, state: &mut CompilerState) -> Result<()> {
        // Clear any existing cache
        self.resolution_cache.clear();
        
        // Build dependency graph
        let dependency_graph = self.build_dependency_graph(state)?;
        
        // Topologically sort styles to resolve dependencies in correct order
        let resolution_order = self.topological_sort(&dependency_graph)?;
        
        // Resolve styles in dependency order
        for style_name in resolution_order {
            if let Some(style_index) = state.styles.iter().position(|s| s.source_name == style_name) {
                self.resolve_style_by_index(style_index, state)?;
            }
        }
        
        // Validate all styles are resolved
        self.validate_resolution_completeness(state)?;
        
        Ok(())
    }
    
    fn build_dependency_graph(&self, state: &CompilerState) -> Result<HashMap<String, Vec<String>>> {
        let mut graph = HashMap::new();
        
        for style in &state.styles {
            let mut dependencies = Vec::new();
            
            for extends_name in &style.extends_style_names {
                // Check if the extended style exists
                if !state.styles.iter().any(|s| s.source_name == *extends_name) {
                    return Err(CompilerError::semantic_legacy(
                        0,
                        format!("Style '{}' extends undefined style '{}'", style.source_name, extends_name)
                    ));
                }
                
                dependencies.push(extends_name.clone());
            }
            
            graph.insert(style.source_name.clone(), dependencies);
        }
        
        Ok(graph)
    }
    
    fn topological_sort(&self, graph: &HashMap<String, Vec<String>>) -> Result<Vec<String>> {
        let mut in_degree = HashMap::new();
        let mut adj_list = HashMap::new();
        
        // Initialize in-degree and adjacency list
        for (node, dependencies) in graph {
            in_degree.entry(node.clone()).or_insert(0);
            adj_list.entry(node.clone()).or_insert(Vec::new());
            
            for dep in dependencies {
                *in_degree.entry(node.clone()).or_insert(0) += 1;
                adj_list.entry(dep.clone()).or_insert(Vec::new()).push(node.clone());
            }
        }
        
        // Start with nodes that have no dependencies
        let mut queue = VecDeque::new();
        for (node, &degree) in &in_degree {
            if degree == 0 {
                queue.push_back(node.clone());
            }
        }
        
        let mut result = Vec::new();
        
        // Process nodes in topological order
        while let Some(current) = queue.pop_front() {
            result.push(current.clone());
            
            if let Some(neighbors) = adj_list.get(&current) {
                for neighbor in neighbors {
                    if let Some(degree) = in_degree.get_mut(neighbor) {
                        *degree -= 1;
                        if *degree == 0 {
                            queue.push_back(neighbor.clone());
                        }
                    }
                }
            }
        }
        
        // Check for cycles
        if result.len() != graph.len() {
            // Find the cycle for error reporting
            let unresolved: Vec<_> = graph.keys()
                .filter(|k| !result.contains(k))
                .cloned()
                .collect();
            
            return Err(CompilerError::semantic_legacy(
                0,
                format!("Circular dependency detected in style inheritance: {:?}", unresolved)
            ));
        }
        
        Ok(result)
    }
    
    fn resolve_style_by_index(&mut self, style_index: usize, state: &mut CompilerState) -> Result<()> {
        // Check if already resolved first
        if state.styles[style_index].is_resolved {
            return Ok(());
        }
        
        let style_name = state.styles[style_index].source_name.clone();
        let extends_style_names = state.styles[style_index].extends_style_names.clone();
        let source_properties = state.styles[style_index].source_properties.clone();
        
        // Check cache first
        if let Some(cached_properties) = self.resolution_cache.get(&style_name) {
            state.styles[style_index].properties = cached_properties.clone();
            state.styles[style_index].is_resolved = true;
            return Ok(());
        }
        
        // Collect inherited properties
        let mut inherited_properties = HashMap::new();
        
        // Process inheritance chain
        for base_style_name in &extends_style_names {
            let base_properties = self.get_base_style_properties(base_style_name, state)?;
            
            // Merge base properties (later bases override earlier ones)
            for prop in base_properties {
                inherited_properties.insert(prop.property_id, prop);
            }
        }
        
        // Convert own source properties to KRB properties and add/override
        let own_properties = self.convert_source_properties_to_krb(&source_properties, state)?;
        
        for prop in own_properties {
            inherited_properties.insert(prop.property_id, prop);
        }
        
        // Convert to final property list
        let final_properties: Vec<KrbProperty> = inherited_properties.into_values().collect();
        
        // Cache the result
        self.resolution_cache.insert(style_name, final_properties.clone());
        
        // Update the style
        state.styles[style_index].properties = final_properties;
        state.styles[style_index].is_resolved = true;
        
        Ok(())
    }
    
    fn get_base_style_properties(&mut self, base_style_name: &str, state: &mut CompilerState) -> Result<Vec<KrbProperty>> {
        // Find the base style
        if let Some(base_index) = state.styles.iter().position(|s| s.source_name == *base_style_name) {
            // Ensure the base style is resolved first
            if !state.styles[base_index].is_resolved {
                self.resolve_style_by_index(base_index, state)?;
            }
            
            Ok(state.styles[base_index].properties.clone())
        } else {
            Err(CompilerError::semantic_legacy(
                0,
                format!("Base style '{}' not found", base_style_name)
            ))
        }
    }
    
    fn convert_source_properties_to_krb(&self, source_properties: &[SourceProperty], state: &mut CompilerState) -> Result<Vec<KrbProperty>> {
        let mut krb_properties = Vec::new();
        
        for source_prop in source_properties {
            if let Some(krb_prop) = self.convert_single_property(source_prop, state)? {
                krb_properties.push(krb_prop);
            }
        }
        
        Ok(krb_properties)
    }
    
    fn convert_single_property(&self, source_prop: &SourceProperty, state: &mut CompilerState) -> Result<Option<KrbProperty>> {
        let property_id = self.get_property_id(&source_prop.key);
        let cleaned_value = clean_and_quote_value(&source_prop.value).0;
        
        let krb_prop = match property_id {
            PropertyId::BackgroundColor | PropertyId::ForegroundColor | PropertyId::BorderColor => {
                if let Ok(color) = parse_color(&cleaned_value) {
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Color,
                        size: 4,
                        value: color.to_bytes().to_vec(),
                    })
                } else {
                    let length_info = if cleaned_value.starts_with('#') {
                        format!("length: {} chars after #", cleaned_value.len() - 1)
                    } else {
                        format!("length: {} chars total", cleaned_value.len())
                    };
                    return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid color value: '{}' ({})", cleaned_value, length_info)
                    ));
                }
            }
            PropertyId::BorderWidth | PropertyId::BorderRadius => {
                if let Ok(val) = cleaned_value.parse::<u8>() {
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Byte,
                        size: 1,
                        value: vec![val],
                    })
                } else {
                    return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid numeric value: {}", cleaned_value)
                    ));
                }
            }
            PropertyId::FontSize => {
                if let Ok(val) = cleaned_value.parse::<u16>() {
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Short,
                        size: 2,
                        value: val.to_le_bytes().to_vec(),
                    })
                } else if cleaned_value.starts_with('$') {
                    // This is a template variable - store it as a string for later resolution
                    let string_index = state.add_string(cleaned_value.clone())?;
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::TemplateVariable,
                        size: 1,
                        value: vec![string_index],
                    })
                } else {
                    return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid font size: {}", cleaned_value)
                    ));
                }
            }
            PropertyId::FontWeight => {
                if cleaned_value.starts_with('$') {
                    // This is a template variable - store it as a string for later resolution
                    let string_index = state.add_string(cleaned_value.clone())?;
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::TemplateVariable,
                        size: 1,
                        value: vec![string_index],
                    })
                } else {
                    let weight_value = match cleaned_value.as_str() {
                        "normal" => 0u8,
                        "bold" => 1u8,
                        "light" => 2u8,
                        "heavy" => 3u8,
                        weight_str => {
                            // Try parsing as numeric weight
                            match weight_str.parse::<u16>() {
                                Ok(weight) => match weight {
                                    100..=200 => 2u8, // light
                                    201..=500 => 0u8, // normal
                                    501..=700 => 1u8, // bold
                                    701..=900 => 3u8, // heavy
                                    _ => 0u8, // default to normal
                                },
                                Err(_) => return Err(CompilerError::semantic_legacy(
                                    source_prop.line_num,
                                    format!("Invalid font weight: {}", cleaned_value)
                                )),
                            }
                        }
                    };
                    
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Enum,
                        size: 1,
                        value: vec![weight_value],
                    })
                }
            }
            PropertyId::TextAlignment => {
                let alignment_value = match cleaned_value.as_str() {
                    "start" => 0u8,
                    "center" => 1u8,
                    "end" => 2u8,
                    "justify" => 3u8,
                    _ => return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid text alignment: {}", cleaned_value)
                    )),
                };
                
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Enum,
                    size: 1,
                    value: vec![alignment_value],
                })
            }
            PropertyId::ListStyleType => {
                let list_style_value = match cleaned_value.as_str() {
                    "bullet" => 1u8,
                    "number" => 2u8,
                    "none" => 0u8,
                    _ => return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid list style type: '{}'. Use 'bullet', 'number', or 'none'", cleaned_value)
                    )),
                };
                
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Enum,
                    size: 1,
                    value: vec![list_style_value],
                })
            }
            PropertyId::WhiteSpace => {
                let white_space_value = match cleaned_value.as_str() {
                    "normal" => 0u8,
                    "nowrap" => 1u8,
                    "pre" => 2u8,
                    _ => return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid white-space value: '{}'. Use 'normal', 'nowrap', or 'pre'", cleaned_value)
                    )),
                };
                
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Enum,
                    size: 1,
                    value: vec![white_space_value],
                })
            }
            PropertyId::Opacity => {
                if let Ok(opacity) = cleaned_value.parse::<f64>() {
                    if opacity >= 0.0 && opacity <= 1.0 {
                        let fixed_point = float_to_fixed_point(opacity);
                        Some(KrbProperty {
                            property_id: property_id as u8,
                            value_type: ValueType::Percentage,
                            size: 2,
                            value: fixed_point.to_le_bytes().to_vec(),
                        })
                    } else {
                        return Err(CompilerError::semantic_legacy(
                            source_prop.line_num,
                            format!("Opacity must be between 0.0 and 1.0: {}", opacity)
                        ));
                    }
                } else {
                    return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid opacity value: {}", cleaned_value)
                    ));
                }
            }
            PropertyId::Visibility => {
                // Handle visible/visibility property - convert boolean or string to boolean, or template variables
                if cleaned_value.starts_with('$') {
                    // This is a template variable - store it as a string for later resolution
                    let string_index = state.add_string(cleaned_value.clone())?;
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::TemplateVariable,
                        size: 1,
                        value: vec![string_index],
                    })
                } else {
                    let visible = match cleaned_value.to_lowercase().as_str() {
                        "true" | "visible" | "1" => true,
                        "false" | "hidden" | "0" => false,
                        _ => {
                            return Err(CompilerError::semantic_legacy(
                                source_prop.line_num,
                                format!("Invalid visibility value: '{}'. Use 'true', 'false', 'visible', 'hidden', or a template variable starting with '$'", cleaned_value)
                            ));
                        }
                    };
                    
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Byte,
                        size: 1,
                        value: vec![if visible { 1 } else { 0 }],
                    })
                }
            }
            PropertyId::Padding | PropertyId::Margin => {
                // Parse edge insets (can be 1, 2, or 4 values)
                let edge_insets = self.parse_edge_insets(&cleaned_value)?;
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::EdgeInsets,
                    size: 4,
                    value: edge_insets.to_vec(),
                })
            }
            PropertyId::Width | PropertyId::Height => {
                // Handle width/height properties as u16 values or percentages
                if let Ok(size) = cleaned_value.parse::<u16>() {
                    // Regular pixel value
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Short,
                        size: 2,
                        value: size.to_le_bytes().to_vec(),
                    })
                } else if cleaned_value.ends_with('%') {
                    // Percentage value - parse as float and store as percentage type
                    let percent_str = &cleaned_value[..cleaned_value.len() - 1]; // Remove '%'
                    if let Ok(percent) = percent_str.parse::<f32>() {
                        // Store percentage as 4-byte float
                        Some(KrbProperty {
                            property_id: property_id as u8,
                            value_type: ValueType::Percentage,
                            size: 4,
                            value: percent.to_le_bytes().to_vec(),
                        })
                    } else {
                        return Err(CompilerError::semantic_legacy(
                            source_prop.line_num,
                            format!("Invalid percentage value: {}", cleaned_value)
                        ));
                    }
                } else {
                    return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid size value: {} (must be a number or percentage)", cleaned_value)
                    ));
                }
            }
            PropertyId::ZIndex => {
                if let Ok(val) = cleaned_value.parse::<i16>() {
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Short,
                        size: 2,
                        value: val.to_le_bytes().to_vec(),
                    })
                } else {
                    return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid z-index value: {}", cleaned_value)
                    ));
                }
            }
            PropertyId::Position => {
                let position_value = match cleaned_value.as_str() {
                    "static" => 0u8,
                    "relative" => 1u8,
                    "absolute" => 2u8,
                    "fixed" => 3u8,
                    "sticky" => 4u8,
                    _ => return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid position value: '{}'. Use 'static', 'relative', 'absolute', 'fixed', or 'sticky'", cleaned_value)
                    )),
                };
                
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::Enum,
                    size: 1,
                    value: vec![position_value],
                })
            }
            PropertyId::Left | PropertyId::Top | PropertyId::Right | PropertyId::Bottom => {
                // Handle positioning offset properties as i16 values or auto
                if cleaned_value == "auto" {
                    // Use special value -32768 to represent auto
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Short,
                        size: 2,
                        value: (-32768i16).to_le_bytes().to_vec(),
                    })
                } else if let Ok(offset) = cleaned_value.parse::<i16>() {
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Short,
                        size: 2,
                        value: offset.to_le_bytes().to_vec(),
                    })
                } else {
                    return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid position offset value: {} (must be a number or 'auto')", cleaned_value)
                    ));
                }
            }
            PropertyId::Display => {
                let display_value = match cleaned_value.as_str() {
                    "none" => 0u8,
                    "block" => 1u8,
                    "inline" => 2u8,
                    "inline-block" => 3u8,
                    "flex" => 4u8,
                    "inline-flex" => 5u8,
                    "grid" => 6u8,
                    "inline-grid" => 7u8,
                    _ => return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid display value: '{}'. Use 'none', 'block', 'inline', 'inline-block', 'flex', 'inline-flex', 'grid', or 'inline-grid'", cleaned_value)
                    )),
                };
                
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::String,
                    size: 1,
                    value: {
                        let string_index = state.add_string(cleaned_value.clone())?;
                        vec![string_index]
                    },
                })
            }
            PropertyId::FlexDirection => {
                let flex_direction_value = match cleaned_value.as_str() {
                    "row" => 0u8,
                    "row-reverse" => 1u8,
                    "column" => 2u8,
                    "column-reverse" => 3u8,
                    _ => return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid flex-direction value: '{}'. Use 'row', 'row-reverse', 'column', or 'column-reverse'", cleaned_value)
                    )),
                };
                
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::String,
                    size: 1,
                    value: {
                        let string_index = state.add_string(cleaned_value.clone())?;
                        vec![string_index]
                    },
                })
            }
            PropertyId::AlignItems => {
                let align_items_value = match cleaned_value.as_str() {
                    "stretch" => 0u8,
                    "flex-start" | "start" => 1u8,
                    "flex-end" | "end" => 2u8,
                    "center" => 3u8,
                    "baseline" => 4u8,
                    _ => return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid align-items value: '{}'. Use 'stretch', 'start', 'end', 'center', or 'baseline'", cleaned_value)
                    )),
                };
                
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::String,
                    size: 1,
                    value: {
                        let string_index = state.add_string(cleaned_value.clone())?;
                        vec![string_index]
                    },
                })
            }
            PropertyId::JustifyContent => {
                let justify_content_value = match cleaned_value.as_str() {
                    "flex-start" | "start" => 0u8,
                    "flex-end" | "end" => 1u8,
                    "center" => 2u8,
                    "space-between" => 3u8,
                    "space-around" => 4u8,
                    "space-evenly" => 5u8,
                    _ => return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid justify-content value: '{}'. Use 'start', 'end', 'center', 'space-between', 'space-around', or 'space-evenly'", cleaned_value)
                    )),
                };
                
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::String,
                    size: 1,
                    value: {
                        let string_index = state.add_string(cleaned_value.clone())?;
                        vec![string_index]
                    },
                })
            }
            PropertyId::Gap => {
                if let Ok(val) = cleaned_value.parse::<u8>() {
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Byte,
                        size: 1,
                        value: vec![val],
                    })
                } else {
                    return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid gap value: {}", cleaned_value)
                    ));
                }
            }
            PropertyId::PaddingTop | PropertyId::PaddingRight | PropertyId::PaddingBottom | PropertyId::PaddingLeft => {
                if let Ok(val) = cleaned_value.parse::<u8>() {
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Byte,
                        size: 1,
                        value: vec![val],
                    })
                } else {
                    return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid padding value: {}", cleaned_value)
                    ));
                }
            }
            PropertyId::MarginTop | PropertyId::MarginRight | PropertyId::MarginBottom | PropertyId::MarginLeft => {
                if let Ok(val) = cleaned_value.parse::<u8>() {
                    Some(KrbProperty {
                        property_id: property_id as u8,
                        value_type: ValueType::Byte,
                        size: 1,
                        value: vec![val],
                    })
                } else {
                    return Err(CompilerError::semantic_legacy(
                        source_prop.line_num,
                        format!("Invalid margin value: {}", cleaned_value)
                    ));
                }
            }
            PropertyId::Invalid => None, // Skip invalid properties
            _ => {
                // For other properties, store as string for now
                Some(KrbProperty {
                    property_id: property_id as u8,
                    value_type: ValueType::String,
                    size: 1,
                    value: {
                        let string_index = state.add_string(cleaned_value.clone()).unwrap_or(0);
                        vec![string_index]
                    },
                })
            }
        };
        
        Ok(krb_prop)
    }
    
    fn parse_edge_insets(&self, value: &str) -> Result<[u8; 4]> {
        let parts: Vec<&str> = value.split_whitespace().collect();
        
        let insets = match parts.len() {
            1 => {
                // All sides the same
                let val = parts[0].parse::<u8>().map_err(|_| {
                    CompilerError::semantic_legacy(0, format!("Invalid edge inset value: {}", parts[0]))
                })?;
                [val, val, val, val]
            }
            2 => {
                // Vertical, horizontal
                let vertical = parts[0].parse::<u8>().map_err(|_| {
                    CompilerError::semantic_legacy(0, format!("Invalid edge inset value: {}", parts[0]))
                })?;
                let horizontal = parts[1].parse::<u8>().map_err(|_| {
                    CompilerError::semantic_legacy(0, format!("Invalid edge inset value: {}", parts[1]))
                })?;
                [vertical, horizontal, vertical, horizontal]
            }
            4 => {
                // Top, right, bottom, left
                let top = parts[0].parse::<u8>().map_err(|_| {
                    CompilerError::semantic_legacy(0, format!("Invalid edge inset value: {}", parts[0]))
                })?;
                let right = parts[1].parse::<u8>().map_err(|_| {
                    CompilerError::semantic_legacy(0, format!("Invalid edge inset value: {}", parts[1]))
                })?;
                let bottom = parts[2].parse::<u8>().map_err(|_| {
                    CompilerError::semantic_legacy(0, format!("Invalid edge inset value: {}", parts[2]))
                })?;
                let left = parts[3].parse::<u8>().map_err(|_| {
                    CompilerError::semantic_legacy(0, format!("Invalid edge inset value: {}", parts[3]))
                })?;
                [top, right, bottom, left]
            }
            _ => {
                return Err(CompilerError::semantic_legacy(
                    0,
                    format!("Invalid edge inset format: '{}' (expected 1, 2, or 4 values)", value)
                ));
            }
        };
        
        Ok(insets)
    }
    
    fn get_property_id(&self, key: &str) -> PropertyId {
        PropertyId::from_name(key)
    }
    
    fn validate_resolution_completeness(&self, state: &CompilerState) -> Result<()> {
        for style in &state.styles {
            if !style.is_resolved {
                return Err(CompilerError::semantic_legacy(
                    0,
                    format!("Style '{}' was not resolved", style.source_name)
                ));
            }
        }
        
        Ok(())
    }
    
    /// Get style resolution statistics
    pub fn get_resolution_stats(&self, state: &CompilerState) -> StyleResolutionStats {
        let mut stats = StyleResolutionStats {
            total_styles: state.styles.len(),
            resolved_styles: 0,
            inheritance_chains: HashMap::new(),
            property_overrides: HashMap::new(),
        };
        
        for style in &state.styles {
            if style.is_resolved {
                stats.resolved_styles += 1;
            }
            
            // Analyze inheritance chain
            if !style.extends_style_names.is_empty() {
                let chain_length = self.calculate_inheritance_chain_length(&style.source_name, state);
                stats.inheritance_chains.insert(style.source_name.clone(), chain_length);
            }
            
            // Count property overrides
            let override_count = self.count_property_overrides(style, state);
            if override_count > 0 {
                stats.property_overrides.insert(style.source_name.clone(), override_count);
            }
        }
        
        stats
    }
    
    fn calculate_inheritance_chain_length(&self, style_name: &str, state: &CompilerState) -> usize {
        if let Some(style) = state.styles.iter().find(|s| s.source_name == *style_name) {
            if style.extends_style_names.is_empty() {
                1
            } else {
                1 + style.extends_style_names.iter()
                    .map(|base| self.calculate_inheritance_chain_length(base, state))
                    .max()
                    .unwrap_or(0)
            }
        } else {
            0
        }
    }
    
    fn count_property_overrides(&self, style: &StyleEntry, state: &CompilerState) -> usize {
        let mut override_count = 0;
        
        // Count how many properties this style overrides from its base styles
        for source_prop in &style.source_properties {
            let property_id = self.get_property_id(&source_prop.key);
            
            // Check if any base style defines this property
            for base_style_name in &style.extends_style_names {
                if let Some(base_style) = state.styles.iter().find(|s| s.source_name == *base_style_name) {
                    if base_style.properties.iter().any(|p| p.property_id == property_id as u8) {
                        override_count += 1;
                        break;
                    }
                }
            }
        }
        
        override_count
    }
}

#[derive(Debug, Clone)]
pub struct StyleResolutionStats {
    pub total_styles: usize,
    pub resolved_styles: usize,
    pub inheritance_chains: HashMap<String, usize>,
    pub property_overrides: HashMap<String, usize>,
}

impl StyleResolutionStats {
    pub fn print_summary(&self) {
        println!("Style Resolution Statistics:");
        println!("  Total styles: {}", self.total_styles);
        println!("  Resolved styles: {}", self.resolved_styles);
        
        if !self.inheritance_chains.is_empty() {
            println!("  Inheritance chains:");
            for (style, length) in &self.inheritance_chains {
                println!("    {}: {} levels", style, length);
            }
        }
        
        if !self.property_overrides.is_empty() {
            println!("  Property overrides:");
            for (style, count) in &self.property_overrides {
                println!("    {}: {} overrides", style, count);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_dependency_graph_building() {
        let resolver = StyleResolver::new();
        let mut state = CompilerState::new();
        
        // Add test styles
        state.styles.push(StyleEntry {
            id: 1,
            source_name: "base".to_string(),
            name_index: 1,
            extends_style_names: vec![],
            properties: vec![],
            source_properties: vec![],
            calculated_size: 0,
            is_resolved: false,
            is_resolving: false,
        });
        
        state.styles.push(StyleEntry {
            id: 2,
            source_name: "derived".to_string(),
            name_index: 2,
            extends_style_names: vec!["base".to_string()],
            properties: vec![],
            source_properties: vec![],
            calculated_size: 0,
            is_resolved: false,
            is_resolving: false,
        });
        
        let graph = resolver.build_dependency_graph(&state).unwrap();
        
        assert_eq!(graph.get("base").unwrap().len(), 0);
        assert_eq!(graph.get("derived").unwrap(), &vec!["base".to_string()]);
    }
    
    #[test]
    fn test_edge_insets_parsing() {
        let resolver = StyleResolver::new();
        
        // Single value
        let insets = resolver.parse_edge_insets("10").unwrap();
        assert_eq!(insets, [10, 10, 10, 10]);
        
        // Two values
        let insets = resolver.parse_edge_insets("10 20").unwrap();
        assert_eq!(insets, [10, 20, 10, 20]);
        
        // Four values
        let insets = resolver.parse_edge_insets("10 20 30 40").unwrap();
        assert_eq!(insets, [10, 20, 30, 40]);
    }
    
    #[test]
    fn test_circular_dependency_detection() {
        let resolver = StyleResolver::new();
        let mut graph = HashMap::new();
        
        graph.insert("a".to_string(), vec!["b".to_string()]);
        graph.insert("b".to_string(), vec!["c".to_string()]);
        graph.insert("c".to_string(), vec!["a".to_string()]);
        
        let result = resolver.topological_sort(&graph);
        assert!(result.is_err());
    }
}

/// Convert a 64-bit float to 8.8 fixed point (16-bit)
fn float_to_fixed_point(value: f64) -> u16 {
    (value * 256.0).round() as u16
}

/// Convert 8.8 fixed point to 64-bit float
fn fixed_point_to_float(value: u16) -> f64 {
    value as f64 / 256.0
}



/// Apply style properties (width, height, layout) to element headers
/// This bridges the gap between style definitions and element size calculations
pub fn apply_style_properties_to_elements(state: &mut CompilerState) -> Result<()> {
    for element in &mut state.elements {
        // Element.style_id is u8, not Option<u8>
        if element.style_id > 0 {
            // Find the style by ID
            if let Some(style) = state.styles.iter().find(|s| s.id == element.style_id) {
                // Apply width, height, and layout properties if found in style
                for property in &style.properties {
                    // Convert property_id back to PropertyId using manual matching
                    match property.property_id {
                        0x19 => { // PropertyId::Width
                            if property.value_type == ValueType::Short && property.value.len() >= 2 {
                                let width = u16::from_le_bytes([property.value[0], property.value[1]]);
                                element.width = width;
                            }
                        },
                        0x1A => { // PropertyId::Height  
                            if property.value_type == ValueType::Short && property.value.len() >= 2 {
                                let height = u16::from_le_bytes([property.value[0], property.value[1]]);
                                element.height = height;
                            }
                        },
                        _ => {} // Ignore other properties for now
                    }
                }
            }
        }
    }
    Ok(())
}

