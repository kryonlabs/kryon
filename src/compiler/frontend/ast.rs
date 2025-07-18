//! Abstract Syntax Tree types for the Kryon compiler

use crate::core::*;
use crate::core::types::*;
use std::collections::HashMap;

/// AST node types
#[derive(Debug, Clone)]
pub enum AstNode {
    /// Root file node
    File {
        directives: Vec<AstNode>,
        styles: Vec<AstNode>,
        fonts: Vec<AstNode>,
        components: Vec<AstNode>,
        scripts: Vec<AstNode>,
        app: Option<Box<AstNode>>,
    },
    
    /// @include directive
    Include {
        path: String,
    },
    
    /// @variables block
    Variables {
        variables: HashMap<String, String>,
    },
    
    /// @script directive
    Script {
        language: String,
        name: Option<String>,
        source: ScriptSource,
        mode: Option<String>,
    },
    
    /// style definition
    Style {
        name: String,
        extends: Vec<String>,
        properties: Vec<AstProperty>,
        pseudo_selectors: Vec<PseudoSelector>,
    },
    
    /// font declaration
    Font {
        name: String,
        path: String,
    },
    
    /// Define component
    Component {
        name: String,
        properties: Vec<ComponentProperty>,
        template: Box<AstNode>,
        functions: Vec<AstNode>, // Function templates within component
    },
    
    /// Properties block (in component definition)
    Properties {
        properties: Vec<ComponentProperty>,
    },
    
    /// Element (App, Container, Text, etc.)
    Element {
        element_type: String,
        properties: Vec<AstProperty>,
        pseudo_selectors: Vec<PseudoSelector>,
        children: Vec<AstNode>,
    },
    
    /// Template control flow structures
    /// @for loop: @for item in collection or @for index, item in collection
    For {
        index_variable: Option<String>, // Optional index variable (e.g., "i" in "@for i, item in collection")
        variable: String,               // Item variable (e.g., "item" in "@for item in collection")
        collection: String,             // Can be a property name or comma-separated list
        body: Vec<AstNode>,
    },
    
    /// @if conditional: @if condition
    If {
        condition: String,
        then_body: Vec<AstNode>,
        elif_branches: Vec<(String, Vec<AstNode>)>, // (condition, body) pairs
        else_body: Option<Vec<AstNode>>,
    },
}

/// Property in AST
#[derive(Debug, Clone)]
pub struct AstProperty {
    pub key: String,
    pub value: PropertyValue,
    pub line: usize,
    pub template_variables: Vec<String>, // Variables used in {{}} syntax
    pub has_templates: bool, // Quick check if this property has template variables
}

/// Property value types
#[derive(Debug, Clone)]
pub enum PropertyValue {
    String(String),
    Number(f64),
    Integer(i64),
    Boolean(bool),
    Color(String),
    Pixels(f64),
    Em(f64),
    Rem(f64),
    ViewportWidth(f64),
    ViewportHeight(f64),
    Percentage(f64),
    Degrees(f64),
    Radians(f64),
    Turns(f64),
    Object(HashMap<String, PropertyValue>),
    Array(Vec<PropertyValue>),
    Variable(String), // Template variable reference
    Expression(Box<Expression>),
    FunctionCall {
        name: String,
        args: Vec<PropertyValue>,
    },
}

/// Expression AST for complex value expressions
#[derive(Debug, Clone)]
pub enum Expression {
    /// Literal values
    String(String),
    Number(f64),
    Integer(i64),
    Boolean(bool),
    Variable(String),
    
    /// Binary comparison operators
    NotEquals(Box<Expression>, Box<Expression>),
    EqualEquals(Box<Expression>, Box<Expression>),
    LessThan(Box<Expression>, Box<Expression>),
    LessThanOrEqual(Box<Expression>, Box<Expression>),
    GreaterThan(Box<Expression>, Box<Expression>),
    GreaterThanOrEqual(Box<Expression>, Box<Expression>),
    
    /// Ternary conditional operator: condition ? true_value : false_value
    Ternary {
        condition: Box<Expression>,
        true_value: Box<Expression>,
        false_value: Box<Expression>,
    },
}

impl PropertyValue {
    /// Convert PropertyValue to string representation
    pub fn to_string(&self) -> String {
        match self {
            PropertyValue::String(s) => s.clone(),
            PropertyValue::Number(n) => n.to_string(),
            PropertyValue::Integer(i) => i.to_string(),
            PropertyValue::Boolean(b) => b.to_string(),
            PropertyValue::Color(c) => c.clone(),
            PropertyValue::Pixels(p) => format!("{}px", p),
            PropertyValue::Em(e) => format!("{}em", e),
            PropertyValue::Rem(r) => format!("{}rem", r),
            PropertyValue::ViewportWidth(vw) => format!("{}vw", vw),
            PropertyValue::ViewportHeight(vh) => format!("{}vh", vh),
            PropertyValue::Percentage(p) => format!("{}%", p),
            PropertyValue::Degrees(d) => format!("{}deg", d),
            PropertyValue::Radians(r) => format!("{}rad", r),
            PropertyValue::Turns(t) => format!("{}turn", t),
            PropertyValue::Object(_) => "[Object]".to_string(),
            PropertyValue::Array(arr) => {
                let items: Vec<String> = arr.iter().map(|v| v.to_string()).collect();
                format!("[{}]", items.join(", "))
            },
            PropertyValue::Variable(v) => format!("${}", v),
            PropertyValue::Expression(expr) => expr.to_string(),
            PropertyValue::FunctionCall { name, args } => {
                let arg_strs: Vec<String> = args.iter().map(|a| a.to_string()).collect();
                format!("{}({})", name, arg_strs.join(", "))
            }
        }
    }
    
    /// Check if this value contains template variables
    pub fn has_variables(&self) -> bool {
        match self {
            PropertyValue::Variable(_) => true,
            PropertyValue::String(s) => s.contains('$'),
            PropertyValue::Object(obj) => obj.values().any(|v| v.has_variables()),
            PropertyValue::Array(arr) => arr.iter().any(|v| v.has_variables()),
            PropertyValue::Expression(expr) => expr.has_variables(),
            _ => false,
        }
    }
    
    /// Extract template variables from this value
    pub fn extract_variables(&self) -> Vec<String> {
        let mut variables = Vec::new();
        match self {
            PropertyValue::Variable(v) => variables.push(v.clone()),
            PropertyValue::String(s) => {
                // Extract $variable patterns from string
                // This is simplified - would need proper regex parsing
                if s.contains('$') {
                    // TODO: Implement proper variable extraction
                }
            },
            PropertyValue::Object(obj) => {
                for value in obj.values() {
                    variables.extend(value.extract_variables());
                }
            },
            PropertyValue::Expression(expr) => {
                variables.extend(expr.extract_variables());
            },
            PropertyValue::Array(arr) => {
                for value in arr {
                    variables.extend(value.extract_variables());
                }
            },
            _ => {}
        }
        variables
    }
}

impl Expression {
    /// Convert Expression to string representation
    pub fn to_string(&self) -> String {
        match self {
            Expression::String(s) => format!("\"{}\"", s),
            Expression::Number(n) => n.to_string(),
            Expression::Integer(i) => i.to_string(),
            Expression::Boolean(b) => b.to_string(),
            Expression::Variable(v) => format!("${}", v),
            Expression::NotEquals(left, right) => format!("{} != {}", left.to_string(), right.to_string()),
            Expression::EqualEquals(left, right) => format!("{} == {}", left.to_string(), right.to_string()),
            Expression::LessThan(left, right) => format!("{} < {}", left.to_string(), right.to_string()),
            Expression::LessThanOrEqual(left, right) => format!("{} <= {}", left.to_string(), right.to_string()),
            Expression::GreaterThan(left, right) => format!("{} > {}", left.to_string(), right.to_string()),
            Expression::GreaterThanOrEqual(left, right) => format!("{} >= {}", left.to_string(), right.to_string()),
            Expression::Ternary { condition, true_value, false_value } => {
                format!("{} ? {} : {}", condition.to_string(), true_value.to_string(), false_value.to_string())
            }
        }
    }
    
    /// Check if this expression contains template variables
    pub fn has_variables(&self) -> bool {
        match self {
            Expression::Variable(_) => true,
            Expression::String(s) => s.contains('$'),
            Expression::NotEquals(left, right) |
            Expression::EqualEquals(left, right) |
            Expression::LessThan(left, right) |
            Expression::LessThanOrEqual(left, right) |
            Expression::GreaterThan(left, right) |
            Expression::GreaterThanOrEqual(left, right) => {
                left.has_variables() || right.has_variables()
            }
            Expression::Ternary { condition, true_value, false_value } => {
                condition.has_variables() || true_value.has_variables() || false_value.has_variables()
            }
            _ => false,
        }
    }
    
    /// Extract template variables from this expression
    pub fn extract_variables(&self) -> Vec<String> {
        let mut variables = Vec::new();
        match self {
            Expression::Variable(v) => variables.push(v.clone()),
            Expression::String(s) => {
                // Extract $variable patterns from string
                if s.contains('$') {
                    // TODO: Implement proper variable extraction
                }
            },
            Expression::NotEquals(left, right) |
            Expression::EqualEquals(left, right) |
            Expression::LessThan(left, right) |
            Expression::LessThanOrEqual(left, right) |
            Expression::GreaterThan(left, right) |
            Expression::GreaterThanOrEqual(left, right) => {
                variables.extend(left.extract_variables());
                variables.extend(right.extract_variables());
            }
            Expression::Ternary { condition, true_value, false_value } => {
                variables.extend(condition.extract_variables());
                variables.extend(true_value.extract_variables());
                variables.extend(false_value.extract_variables());
            }
            _ => {}
        }
        variables
    }
}

/// Component property definition
#[derive(Debug, Clone)]
pub struct ComponentProperty {
    pub name: String,
    pub property_type: Option<String>,
    pub default_value: Option<String>,
    pub line: usize,
}

/// Pseudo-selector (e.g., &:hover)
#[derive(Debug, Clone)]
pub struct PseudoSelector {
    pub state: String,
    pub properties: Vec<AstProperty>,
    pub line: usize,
}

/// Script source type
#[derive(Debug, Clone)]
pub enum ScriptSource {
    Inline(String),
    External(String),
}

impl AstProperty {
    pub fn new(key: String, value: PropertyValue, line: usize) -> Self {
        Self { 
            key, 
            value, 
            line,
            template_variables: Vec::new(),
            has_templates: false,
        }
    }
    
    pub fn new_with_templates(key: String, value: PropertyValue, line: usize, template_variables: Vec<String>) -> Self {
        let has_templates = !template_variables.is_empty();
        Self { 
            key, 
            value, 
            line,
            template_variables,
            has_templates,
        }
    }
    
    /// Get the cleaned value (without quotes if it was quoted)
    pub fn cleaned_value(&self) -> String {
        match &self.value {
            PropertyValue::String(s) => clean_and_quote_value(s).0,
            _ => self.value.to_string(),
        }
    }
    
    /// Check if this property value was quoted
    pub fn was_quoted(&self) -> bool {
        match &self.value {
            PropertyValue::String(s) => clean_and_quote_value(s).1,
            _ => false,
        }
    }
    
    /// Get raw string value (for compatibility)
    pub fn value_string(&self) -> String {
        self.value.to_string()
    }
}

impl ComponentProperty {
    pub fn new(name: String, property_type: Option<String>, default_value: Option<String>, line: usize) -> Self {
        Self {
            name,
            property_type,
            default_value,
            line,
        }
    }
    
    /// Get the value type hint
    pub fn value_type_hint(&self) -> ValueType {
        match self.property_type.as_deref() {
            Some("String") => ValueType::String,
            Some("Int") => ValueType::Int,
            Some("Float") => ValueType::Float,
            Some("Bool") => ValueType::Bool,
            Some("Color") => ValueType::Color,
            Some("StyleID") => ValueType::StyleId,
            Some("Resource") => ValueType::Resource,
            Some(t) if t.starts_with("Enum(") => ValueType::Enum,
            Some(_) => ValueType::Custom,
            None => ValueType::String, // Default for inferred types
        }
    }
}

impl PseudoSelector {
    pub fn new(state: String, properties: Vec<AstProperty>, line: usize) -> Self {
        Self { state, properties, line }
    }
    
    /// Get the state flag for this pseudo-selector
    pub fn state_flag(&self) -> Option<u8> {
        match self.state.as_str() {
            "hover" => Some(STATE_HOVER),
            "active" => Some(STATE_ACTIVE),
            "focus" => Some(STATE_FOCUS),
            "disabled" => Some(STATE_DISABLED),
            "checked" => Some(STATE_CHECKED),
            _ => None,
        }
    }
}