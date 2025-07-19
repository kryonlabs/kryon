// FILE: src/core/state.rs

use crate::compiler::frontend::ast::AstNode;
use crate::compiler::frontend::ast::PropertyValue;
use crate::compiler::middle_end::variable_context::VariableContext;
use crate::core::constants::*;
use crate::core::types::*;
use crate::error::CompilerError;
use std::collections::{HashMap, HashSet};

// Transform data structures
#[derive(Debug, Clone)]
pub struct TransformData {
    pub transform_type: TransformType,
    pub properties: Vec<TransformProperty>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum TransformType {
    Transform2D = 0x01,
    Transform3D = 0x02,
    Matrix2D = 0x03,
    Matrix3D = 0x04,
}

#[derive(Debug, Clone)]
pub struct TransformProperty {
    pub property_type: TransformPropertyType,
    pub value_type: ValueType,
    pub value: Vec<u8>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum TransformPropertyType {
    Scale = 0x01,
    ScaleX = 0x02,
    ScaleY = 0x03,
    TranslateX = 0x04,
    TranslateY = 0x05,
    Rotate = 0x06,
    SkewX = 0x07,
    SkewY = 0x08,
    ScaleZ = 0x09,
    TranslateZ = 0x0A,
    RotateX = 0x0B,
    RotateY = 0x0C,
    RotateZ = 0x0D,
    Perspective = 0x0E,
    Matrix = 0x0F,
}

impl TransformPropertyType {
    pub fn from_name(name: &str) -> Option<Self> {
        match name {
            "scale" => Some(Self::Scale),
            "scale_x" => Some(Self::ScaleX),
            "scale_y" => Some(Self::ScaleY),
            "translate_x" => Some(Self::TranslateX),
            "translate_y" => Some(Self::TranslateY),
            "rotate" => Some(Self::Rotate),
            "skew_x" => Some(Self::SkewX),
            "skew_y" => Some(Self::SkewY),
            "scale_z" => Some(Self::ScaleZ),
            "translate_z" => Some(Self::TranslateZ),
            "rotate_x" => Some(Self::RotateX),
            "rotate_y" => Some(Self::RotateY),
            "rotate_z" => Some(Self::RotateZ),
            "perspective" => Some(Self::Perspective),
            "matrix" => Some(Self::Matrix),
            _ => None,
        }
    }
}

#[derive(Debug, Clone)]
pub struct CSSUnitValue {
    pub value: f64,
    pub unit: CSSUnit,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum CSSUnit {
    Pixels = 0x01,
    Em = 0x02,
    Rem = 0x03,
    ViewportWidth = 0x04,
    ViewportHeight = 0x05,
    Percentage = 0x06,
    Degrees = 0x07,
    Radians = 0x08,
    Turns = 0x09,
    Number = 0x0A,
}

impl CSSUnit {
    pub fn from_property_value(value: &PropertyValue) -> Option<CSSUnitValue> {
        match value {
            PropertyValue::Pixels(v) => {
                Some(CSSUnitValue { value: *v, unit: CSSUnit::Pixels })
            }
            PropertyValue::Em(v) => Some(CSSUnitValue { value: *v, unit: CSSUnit::Em }),
            PropertyValue::Rem(v) => {
                Some(CSSUnitValue { value: *v, unit: CSSUnit::Rem })
            }
            PropertyValue::ViewportWidth(v) => {
                Some(CSSUnitValue { value: *v, unit: CSSUnit::ViewportWidth })
            }
            PropertyValue::ViewportHeight(v) => {
                Some(CSSUnitValue { value: *v, unit: CSSUnit::ViewportHeight })
            }
            PropertyValue::Percentage(v) => {
                Some(CSSUnitValue { value: *v, unit: CSSUnit::Percentage })
            }
            PropertyValue::Degrees(v) => {
                Some(CSSUnitValue { value: *v, unit: CSSUnit::Degrees })
            }
            PropertyValue::Radians(v) => {
                Some(CSSUnitValue { value: *v, unit: CSSUnit::Radians })
            }
            PropertyValue::Turns(v) => {
                Some(CSSUnitValue { value: *v, unit: CSSUnit::Turns })
            }
            PropertyValue::Number(v) => {
                Some(CSSUnitValue { value: *v, unit: CSSUnit::Number })
            }
            PropertyValue::Integer(v) => {
                Some(CSSUnitValue { value: *v as f64, unit: CSSUnit::Number })
            }
            _ => None,
        }
    }
}

// Core data structures
#[derive(Debug, Clone)]
pub struct KrbProperty {
    pub property_id: u8,
    pub value_type: ValueType,
    pub size: u8,
    pub value: Vec<u8>,
}

#[derive(Debug, Clone)]
pub struct KrbCustomProperty {
    pub key_index: u8,
    pub value_type: ValueType,
    pub size: u8,
    pub value: Vec<u8>,
}

#[derive(Debug, Clone)]
pub struct StatePropertySet {
    pub state_flags: u8,
    pub property_count: u8,
    pub properties: Vec<KrbProperty>,
}

#[derive(Debug, Clone)]
pub struct SourceProperty {
    pub key: String,
    pub value: String,
    pub line_num: usize,
    pub template_variables: Vec<String>, // Variables used in this property
}

#[derive(Debug, Clone)]
pub struct KrbEvent {
    pub event_type: u8,
    pub callback_id: u8,
}

#[derive(Debug, Clone)]
pub struct StringEntry {
    pub text: String,
    pub length: usize,
    pub index: u8,
}

#[derive(Debug, Clone)]
pub struct ResourceEntry {
    pub resource_type: ResourceType,
    pub name_index: u8,
    pub format: ResourceFormat,
    pub data_string_index: u8,
    pub index: u8,
    pub calculated_size: u32,
}

#[derive(Debug, Clone)]
pub struct ComponentPropertyDef {
    pub name: String,
    pub value_type_hint: ValueType,
    pub default_value: String,
}

#[derive(Debug, Clone)]
pub struct ComponentDefinition {
    pub name: String,
    pub properties: Vec<ComponentPropertyDef>,
    pub definition_start_line: usize,
    pub definition_root_element_index: Option<usize>,
    pub calculated_size: u32,
    pub internal_template_element_offsets: HashMap<usize, u32>,
}

#[derive(Debug, Clone)]
pub struct TemplateVariable {
    pub name: String,
    pub name_index: u8,
    pub value_type: ValueType,
    pub default_value: String,
    pub default_value_index: u8,
}

#[derive(Debug, Clone)]
pub struct TemplateBinding {
    pub element_index: u16,
    pub property_id: u8,
    pub template_expression: String,
    pub template_expression_index: u8,
    pub variable_count: u8,
    pub variable_indices: Vec<u8>,
}

#[derive(Debug, Clone)]
pub struct ScriptFunction {
    pub function_name: String,
    pub function_name_index: u8,
}

#[derive(Debug, Clone)]
pub struct ScriptEntry {
    pub language_id: ScriptLanguage,
    pub name: String,
    pub name_index: u8,
    pub storage_format: u8,
    pub entry_point_count: u8,
    pub data_size: u16,
    pub entry_points: Vec<ScriptFunction>,
    pub code_data: Vec<u8>,
    pub resource_index: Option<u8>,
    pub calculated_size: u32,
    pub source_line_num: usize,
}

#[derive(Debug, Clone)]
pub struct StyleEntry {
    pub id: u8,
    pub source_name: String,
    pub name_index: u8,
    pub extends_style_names: Vec<String>,
    pub properties: Vec<KrbProperty>,
    pub source_properties: Vec<SourceProperty>,
    pub calculated_size: u32,
    pub is_resolved: bool,
    pub is_resolving: bool,
}

#[derive(Debug, Clone)]
pub struct FontEntry {
    pub name: String,
    pub path: String,
    pub name_index: u8,
    pub path_index: u8,
}

#[derive(Debug, Clone)]
pub struct Element {
    pub element_type: ElementType,
    pub id_string_index: u8,
    pub pos_x: u16,
    pub pos_y: u16,
    pub width: u16,
    pub height: u16,
    pub layout: u8,
    pub style_id: u8,
    pub checked: bool,
    pub property_count: u8,
    pub child_count: u8,
    pub event_count: u8,
    pub animation_count: u8,
    pub custom_prop_count: u8,
    pub state_prop_count: u8,
    pub krb_properties: Vec<KrbProperty>,
    pub krb_custom_properties: Vec<KrbCustomProperty>,
    pub krb_events: Vec<KrbEvent>,
    pub state_property_sets: Vec<StatePropertySet>,
    pub children: Vec<usize>,
    pub parent_index: Option<usize>,
    pub self_index: usize,
    pub is_component_instance: bool,
    pub component_def: Option<usize>,
    pub is_definition_root: bool,
    pub source_element_name: String,
    pub source_id_name: String,
    pub source_properties: Vec<SourceProperty>,
    pub source_children_indices: Vec<usize>,
    pub source_line_num: usize,
    pub layout_flags_source: u8,
    pub position_hint: String,
    pub orientation_hint: String,
    pub calculated_size: u32,
    pub absolute_offset: u32,
    pub processed_in_pass: bool,
}

#[derive(Debug, Clone)]
pub struct VariableDef {
    pub value: String,
    pub raw_value: String,
    pub def_line: usize,
    pub is_resolving: bool,
    pub is_resolved: bool,
}

#[derive(Debug, Clone)]
pub struct FunctionTemplate {
    pub id: usize,
    pub name_pattern: String,
    pub body: String,
    pub parameters: Vec<String>,
    pub language: String,
    pub scope: FunctionScope,
    pub required_vars: HashSet<String>,
    pub source_location: SourceLocation,
}

#[derive(Debug, Clone, PartialEq)]
pub enum FunctionScope {
    Global,
    Component(String),
}

#[derive(Debug, Clone)]
pub struct SourceLocation {
    pub file: String,
    pub line: usize,
    pub column: usize,
}

#[derive(Debug, Clone)]
pub struct ResolvedFunction {
    pub name: String,
    pub code: String,
    pub template_id: usize,
    pub instance_context: Option<String>,
    pub language: String,
    pub parameters: Vec<String>,
}

#[derive(Debug)]
pub struct CompilerState {
    pub elements: Vec<Element>,
    pub strings: Vec<StringEntry>,
    pub styles: Vec<StyleEntry>,
    pub fonts: Vec<FontEntry>,
    pub scripts: Vec<ScriptEntry>,
    pub resources: Vec<ResourceEntry>,
    pub component_defs: Vec<ComponentDefinition>,
    pub component_ast_templates: HashMap<String, AstNode>,
    pub variables: HashMap<String, VariableDef>,
    pub variable_context: VariableContext,
    pub has_app: bool,
    pub header_flags: u16,
    pub current_line_num: usize,
    pub current_file_path: String,
    pub element_offset: u32,
    pub style_offset: u32,
    pub component_def_offset: u32,
    pub anim_offset: u32,
    pub script_offset: u32,
    pub string_offset: u32,
    pub resource_offset: u32,
    pub total_size: u32,
    pub total_element_data_size: u32,
    pub total_style_data_size: u32,
    pub total_component_def_data_size: u32,
    pub total_script_data_size: u32,
    pub total_string_data_size: u32,
    pub total_resource_table_size: u32,
    pub template_variables: Vec<TemplateVariable>,
    pub template_bindings: Vec<TemplateBinding>,
    pub template_variable_offset: u32,
    pub template_binding_offset: u32,
    pub total_template_variable_size: u32,
    pub total_template_binding_size: u32,
    pub transforms: Vec<TransformData>,
    pub transform_offset: u32,
    pub total_transform_size: u32,
    pub function_templates: Vec<FunctionTemplate>,
    pub resolved_functions: HashMap<String, ResolvedFunction>,
    pub component_functions: HashMap<String, Vec<String>>,
    pub next_template_id: usize,
    pub component_scripts: HashMap<String, Vec<AstNode>>,
}

impl CompilerState {
    pub fn new() -> Self {
        Self {
            elements: Vec::new(),
            strings: Vec::new(),
            styles: Vec::new(),
            fonts: Vec::new(),
            scripts: Vec::new(),
            resources: Vec::new(),
            component_defs: Vec::new(),
            component_ast_templates: HashMap::new(),
            variables: HashMap::new(),
            variable_context: VariableContext::new(),
            has_app: false,
            header_flags: 0,
            current_line_num: 0,
            current_file_path: String::new(),
            element_offset: 0,
            style_offset: 0,
            component_def_offset: 0,
            anim_offset: 0,
            script_offset: 0,
            string_offset: 0,
            resource_offset: 0,
            total_size: 0,
            total_element_data_size: 0,
            total_style_data_size: 0,
            total_component_def_data_size: 0,
            total_script_data_size: 0,
            total_string_data_size: 0,
            total_resource_table_size: 0,
            template_variables: Vec::new(),
            template_bindings: Vec::new(),
            template_variable_offset: 0,
            template_binding_offset: 0,
            total_template_variable_size: 0,
            total_template_binding_size: 0,
            transforms: Vec::new(),
            transform_offset: 0,
            total_transform_size: 0,
            function_templates: Vec::new(),
            resolved_functions: HashMap::new(),
            component_functions: HashMap::new(),
            next_template_id: 0,
            component_scripts: HashMap::new(),
        }
    }

    pub fn add_string<S: AsRef<str>>(&mut self, text: S) -> Result<u8, CompilerError> {
        let text_str = text.as_ref();

        if text_str.is_empty() {
            if self.strings.is_empty() {
                self.strings.push(StringEntry {
                    text: String::new(),
                    length: 0,
                    index: 0,
                });
            }
            return Ok(0);
        }

        for (index, existing) in self.strings.iter().enumerate() {
            if existing.text == text_str {
                return Ok(index as u8);
            }
        }

        if self.strings.len() >= MAX_STRINGS {
            return Err(CompilerError::LimitExceeded {
                limit_type: "strings".to_string(),
                limit: MAX_STRINGS,
            });
        }

        let index = self.strings.len() as u8;
        self.strings.push(StringEntry {
            text: text_str.to_string(),
            length: text_str.len(),
            index,
        });
        Ok(index)
    }
}