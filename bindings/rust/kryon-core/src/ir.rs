use serde::{Deserialize, Serialize};
use std::collections::HashMap;

use crate::{Color, Dimension};

/// Top-level IR document structure (.kir file format v3.0)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IRDocument {
    /// Format version (currently "3.0")
    pub format_version: String,

    /// Component definitions (reusable templates)
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub component_definitions: Vec<ComponentDefinition>,

    /// Root component of the UI tree
    pub root: IRComponent,

    /// Logic section (functions, event bindings)
    #[serde(default)]
    pub logic: Logic,

    /// Reactive state manifest
    #[serde(default)]
    pub reactive_manifest: ReactiveManifest,
}

impl IRDocument {
    /// Create a new IR document with the given root component
    pub fn new(root: IRComponent) -> Self {
        IRDocument {
            format_version: "3.0".to_string(),
            component_definitions: Vec::new(),
            root,
            logic: Logic::default(),
            reactive_manifest: ReactiveManifest::default(),
        }
    }

    /// Serialize to JSON string
    pub fn to_json(&self) -> Result<String, serde_json::Error> {
        serde_json::to_string_pretty(self)
    }

    /// Deserialize from JSON string
    pub fn from_json(json: &str) -> Result<Self, serde_json::Error> {
        serde_json::from_str(json)
    }

    /// Write to .kir file
    pub fn write_to_file(&self, path: &str) -> Result<(), std::io::Error> {
        let json = self.to_json().map_err(|e| {
            std::io::Error::new(std::io::ErrorKind::InvalidData, e.to_string())
        })?;
        std::fs::write(path, json)
    }

    /// Read from .kir file
    pub fn read_from_file(path: &str) -> Result<Self, std::io::Error> {
        let json = std::fs::read_to_string(path)?;
        Self::from_json(&json).map_err(|e| {
            std::io::Error::new(std::io::ErrorKind::InvalidData, e.to_string())
        })
    }
}

/// Component definition (reusable template)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ComponentDefinition {
    pub name: String,
    pub template: IRComponent,
}

/// Logic section containing functions and event bindings
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct Logic {
    #[serde(default)]
    pub functions: HashMap<String, String>,

    #[serde(default)]
    pub event_bindings: Vec<EventBinding>,
}

/// Event binding
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EventBinding {
    pub component_id: u32,
    pub event_type: String,
    pub handler: String,
}

/// Reactive state manifest
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct ReactiveManifest {
    #[serde(default)]
    pub variables: Vec<ReactiveVariable>,
}

/// Reactive variable definition
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ReactiveVariable {
    pub id: String,
    pub var_type: String,
    pub initial_value: serde_json::Value,
}

/// IR Component - represents a single UI component
#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct IRComponent {
    /// Unique component ID
    pub id: u32,

    /// Component type (Container, Text, Button, etc.)
    #[serde(rename = "type")]
    pub component_type: ComponentType,

    // Layout properties
    #[serde(skip_serializing_if = "Option::is_none")]
    pub width: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub height: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub padding: Option<f32>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub margin: Option<f32>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub gap: Option<f32>,

    // Alignment properties
    #[serde(rename = "justifyContent", skip_serializing_if = "Option::is_none")]
    pub justify_content: Option<AlignmentValue>,

    #[serde(rename = "alignItems", skip_serializing_if = "Option::is_none")]
    pub align_items: Option<AlignmentValue>,

    // Flex properties
    #[serde(rename = "flexGrow", skip_serializing_if = "Option::is_none")]
    pub flex_grow: Option<u8>,

    #[serde(rename = "flexShrink", skip_serializing_if = "Option::is_none")]
    pub flex_shrink: Option<u8>,

    // Visual properties
    #[serde(skip_serializing_if = "Option::is_none")]
    pub background: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub color: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub opacity: Option<f32>,

    #[serde(rename = "fontSize", skip_serializing_if = "Option::is_none")]
    pub font_size: Option<f32>,

    // Content properties
    #[serde(skip_serializing_if = "Option::is_none")]
    pub content: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub text: Option<String>,

    #[serde(skip_serializing_if = "Option::is_none")]
    pub placeholder: Option<String>,

    // App-specific properties
    #[serde(rename = "windowTitle", skip_serializing_if = "Option::is_none")]
    pub window_title: Option<String>,

    // Children
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub children: Vec<IRComponent>,
}

/// Component type enumeration
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub enum ComponentType {
    #[default]
    // Layout containers
    Container,
    Row,
    Column,
    Center,

    // Content components
    Text,
    Button,
    Input,
    Checkbox,

    // Tab components
    TabGroup,
    TabBar,
    Tab,
    TabContent,
    TabPanel,

    // Advanced components
    Table,
    TableRow,
    TableCell,
    Dropdown,
    Scrollable,
}

/// Alignment value (can be a constant or variable reference)
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(untagged)]
pub enum AlignmentValue {
    /// Direct alignment value
    Constant(Alignment),
    /// Variable reference
    Variable { var: String },
}

/// Alignment enumeration
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Alignment {
    Start,
    Center,
    End,
    #[serde(rename = "space-between")]
    SpaceBetween,
    #[serde(rename = "space-around")]
    SpaceAround,
}

impl IRComponent {
    /// Create a new component with the given type and ID
    pub fn new(component_type: ComponentType, id: u32) -> Self {
        IRComponent {
            id,
            component_type,
            width: None,
            height: None,
            padding: None,
            margin: None,
            gap: None,
            justify_content: None,
            align_items: None,
            flex_grow: None,
            flex_shrink: None,
            background: None,
            color: None,
            opacity: None,
            font_size: None,
            content: None,
            text: None,
            placeholder: None,
            window_title: None,
            children: Vec::new(),
        }
    }

    /// Add a child component
    pub fn add_child(&mut self, child: IRComponent) {
        self.children.push(child);
    }

    /// Set width
    pub fn with_width(mut self, width: impl Into<String>) -> Self {
        self.width = Some(width.into());
        self
    }

    /// Set height
    pub fn with_height(mut self, height: impl Into<String>) -> Self {
        self.height = Some(height.into());
        self
    }

    /// Set background color
    pub fn with_background(mut self, color: impl Into<String>) -> Self {
        self.background = Some(color.into());
        self
    }

    /// Set text color
    pub fn with_color(mut self, color: impl Into<String>) -> Self {
        self.color = Some(color.into());
        self
    }

    /// Set padding
    pub fn with_padding(mut self, padding: f32) -> Self {
        self.padding = Some(padding);
        self
    }

    /// Set gap
    pub fn with_gap(mut self, gap: f32) -> Self {
        self.gap = Some(gap);
        self
    }

    /// Set content (for Text components)
    pub fn with_content(mut self, content: impl Into<String>) -> Self {
        self.content = Some(content.into());
        self
    }

    /// Set text (for Button components)
    pub fn with_text(mut self, text: impl Into<String>) -> Self {
        self.text = Some(text.into());
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_component_creation() {
        let component = IRComponent::new(ComponentType::Column, 1)
            .with_width("100%")
            .with_height("200px")
            .with_background("#1a1a2e");

        assert_eq!(component.id, 1);
        assert_eq!(component.component_type, ComponentType::Column);
        assert_eq!(component.width, Some("100%".to_string()));
        assert_eq!(component.height, Some("200px".to_string()));
        assert_eq!(component.background, Some("#1a1a2e".to_string()));
    }

    #[test]
    fn test_document_serialization() {
        let root = IRComponent::new(ComponentType::Container, 1)
            .with_width("800px")
            .with_height("600px");

        let doc = IRDocument::new(root);
        let json = doc.to_json().unwrap();

        assert!(json.contains("\"format_version\": \"3.0\""));
        assert!(json.contains("\"type\": \"Container\""));
    }

    #[test]
    fn test_document_roundtrip() {
        let root = IRComponent::new(ComponentType::Column, 1)
            .with_width("100%")
            .with_background("#000000");

        let doc = IRDocument::new(root);
        let json = doc.to_json().unwrap();
        let parsed = IRDocument::from_json(&json).unwrap();

        assert_eq!(parsed.format_version, "3.0");
        assert_eq!(parsed.root.component_type, ComponentType::Column);
        assert_eq!(parsed.root.width, Some("100%".to_string()));
    }
}
