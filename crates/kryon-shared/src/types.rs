//! Unified Type System for Kryon
//! 
//! This is the single source of truth for all element types, property IDs, 
//! and other type definitions used throughout the Kryon ecosystem.
//! 
//! ALL OTHER CRATES MUST IMPORT FROM HERE - DO NOT DUPLICATE DEFINITIONS!

use std::collections::HashMap;

#[cfg(feature = "serde_support")]
use serde::{Deserialize, Serialize};

// =============================================================================
// ELEMENT TYPES
// =============================================================================

/// All element types supported by Kryon with their binary format hex values
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "serde_support", derive(Serialize, Deserialize))]
#[repr(u8)]
pub enum ElementType {
    // Core Elements (0x00-0x0F)
    App = 0x00,
    Container = 0x01,
    Text = 0x02,
    EmbedView = 0x03,
    
    // Interactive Elements (0x10-0x1F)
    Button = 0x10,
    Input = 0x11,
    Link = 0x12,
    
    // Media Elements (0x20-0x2F)
    Image = 0x20,
    Video = 0x21,
    Canvas = 0x22,
    Svg = 0x23,
    
    // Form Controls (0x30-0x3F)
    Select = 0x30,
    Slider = 0x31,
    ProgressBar = 0x32,
    Checkbox = 0x33,
    RadioGroup = 0x34,
    Toggle = 0x35,
    DatePicker = 0x36,
    FilePicker = 0x37,
    
    // Semantic Elements (0x40-0x4F)
    Form = 0x40,
    List = 0x41,
    ListItem = 0x42,
    
    // Table Elements (0x50-0x5F)
    Table = 0x50,
    TableRow = 0x51,
    TableCell = 0x52,
    TableHeader = 0x53,
    
    // Special
    Custom(u8),
}

impl ElementType {
    /// Create ElementType from hex value
    pub fn from_u8(value: u8) -> Self {
        match value {
            // Core Elements (0x00-0x0F)
            0x00 => ElementType::App,
            0x01 => ElementType::Container,
            0x02 => ElementType::Text,
            0x03 => ElementType::EmbedView,
            
            // Interactive Elements (0x10-0x1F)
            0x10 => ElementType::Button,
            0x11 => ElementType::Input,
            0x12 => ElementType::Link,
            
            // Media Elements (0x20-0x2F)
            0x20 => ElementType::Image,
            0x21 => ElementType::Video,
            0x22 => ElementType::Canvas,
            0x23 => ElementType::Svg,
            
            // Form Controls (0x30-0x3F)
            0x30 => ElementType::Select,
            0x31 => ElementType::Slider,
            0x32 => ElementType::ProgressBar,
            0x33 => ElementType::Checkbox,
            0x34 => ElementType::RadioGroup,
            0x35 => ElementType::Toggle,
            0x36 => ElementType::DatePicker,
            0x37 => ElementType::FilePicker,
            
            // Semantic Elements (0x40-0x4F)
            0x40 => ElementType::Form,
            0x41 => ElementType::List,
            0x42 => ElementType::ListItem,
            
            // Table Elements (0x50-0x5F)
            0x50 => ElementType::Table,
            0x51 => ElementType::TableRow,
            0x52 => ElementType::TableCell,
            0x53 => ElementType::TableHeader,
            
            other => ElementType::Custom(other),
        }
    }
    
    /// Convert ElementType to hex value
    pub fn to_u8(self) -> u8 {
        match self {
            ElementType::Custom(value) => value,
            _ => self as u8,
        }
    }
    
    /// Create ElementType from string name
    pub fn from_name(name: &str) -> Self {
        match name {
            "App" => Self::App,
            "Container" => Self::Container,
            "Text" => Self::Text,
            "Link" => Self::Link,
            "Image" => Self::Image,
            "Canvas" => Self::Canvas,
            "Video" => Self::Video,
            "EmbedView" => Self::EmbedView,
            "Svg" => Self::Svg,
            "Button" => Self::Button,
            "Input" => Self::Input,
            "Select" => Self::Select,
            "Slider" => Self::Slider,
            "ProgressBar" => Self::ProgressBar,
            "Checkbox" => Self::Checkbox,
            "RadioGroup" => Self::RadioGroup,
            "Toggle" => Self::Toggle,
            "DatePicker" => Self::DatePicker,
            "FilePicker" => Self::FilePicker,
            "Form" => Self::Form,
            "List" => Self::List,
            "ListItem" => Self::ListItem,
            "Table" => Self::Table,
            "TableRow" => Self::TableRow,
            "TableCell" => Self::TableCell,
            "TableHeader" => Self::TableHeader,
            _ => Self::Custom(0xFF), // Unknown
        }
    }
    
    /// Get the string name of this element type
    pub fn to_name(self) -> &'static str {
        match self {
            ElementType::App => "App",
            ElementType::Container => "Container",
            ElementType::Text => "Text",
            ElementType::Link => "Link",
            ElementType::Image => "Image",
            ElementType::Canvas => "Canvas",
            ElementType::Video => "Video",
            ElementType::EmbedView => "EmbedView",
            ElementType::Svg => "Svg",
            ElementType::Button => "Button",
            ElementType::Input => "Input",
            ElementType::Select => "Select",
            ElementType::Slider => "Slider",
            ElementType::ProgressBar => "ProgressBar",
            ElementType::Checkbox => "Checkbox",
            ElementType::RadioGroup => "RadioGroup",
            ElementType::Toggle => "Toggle",
            ElementType::DatePicker => "DatePicker",
            ElementType::FilePicker => "FilePicker",
            ElementType::Form => "Form",
            ElementType::List => "List",
            ElementType::ListItem => "ListItem",
            ElementType::Table => "Table",
            ElementType::TableRow => "TableRow",
            ElementType::TableCell => "TableCell",
            ElementType::TableHeader => "TableHeader",
            ElementType::Custom(_) => "Custom",
        }
    }
}

impl From<u8> for ElementType {
    fn from(value: u8) -> Self {
        Self::from_u8(value)
    }
}

// =============================================================================
// PROPERTY IDS
// =============================================================================

/// Unified property IDs - the single source of truth for all properties
/// Used in the binary KRB format and throughout the system
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "serde_support", derive(Serialize, Deserialize))]
#[repr(u8)]
pub enum PropertyId {
    // Visual Properties (0x01-0x0F)
    BackgroundColor = 0x01,
    TextColor = 0x02,
    BorderColor = 0x03,
    BorderWidth = 0x04,
    BorderRadius = 0x05,
    LayoutFlags = 0x06,
    TextContent = 0x08,
    FontSize = 0x09,
    FontWeight = 0x0A,
    TextAlignment = 0x0B,
    FontFamily = 0x0C,
    ImageSource = 0x0D,
    Opacity = 0x0E,
    ZIndex = 0x0F,
    
    // Display Properties (0x10-0x1F)  
    Visibility = 0x10,
    Gap = 0x11,
    MinWidth = 0x12,
    MinHeight = 0x13,
    MaxWidth = 0x14,
    MaxHeight = 0x15,
    Transform = 0x16,
    Shadow = 0x18,
    Width = 0x19,
    Height = 0x1A,
    OldLayoutFlags = 0x1B,
    StyleId = 0x1D,
    ListStyleType = 0x1E,
    WhiteSpace = 0x1F,
    
    // Window Properties (0x20-0x2F)
    WindowWidth = 0x20,
    WindowHeight = 0x21,
    WindowTitle = 0x22,
    WindowResizable = 0x23,
    WindowFullscreen = 0x24,
    WindowVsync = 0x25,
    WindowTargetFps = 0x26,
    WindowAntialiasing = 0x27,
    WindowIcon = 0x28,
    Cursor = 0x29,
    
    // Flexbox Properties (0x40-0x4F)
    Display = 0x40,
    FlexDirection = 0x41,
    FlexWrap = 0x42,
    FlexGrow = 0x43,
    FlexShrink = 0x44,
    FlexBasis = 0x45,
    AlignItems = 0x46,
    AlignContent = 0x47,
    AlignSelf = 0x48,
    JustifyContent = 0x49,
    JustifyItems = 0x4A,
    JustifySelf = 0x4B,
    
    // Position Properties (0x50-0x5F)
    Position = 0x50,
    Left = 0x51,
    Top = 0x52,
    Right = 0x53,
    Bottom = 0x54,
    
    // Grid Properties (0x60-0x6F)
    GridTemplateColumns = 0x60,
    GridTemplateRows = 0x61,
    GridTemplateAreas = 0x62,
    GridAutoColumns = 0x63,
    GridAutoRows = 0x64,
    GridAutoFlow = 0x65,
    GridArea = 0x66,
    GridColumn = 0x67,
    GridRow = 0x68,
    GridColumnStart = 0x69,
    GridColumnEnd = 0x6A,
    GridRowStart = 0x6B,
    GridRowEnd = 0x6C,
    GridGap = 0x6D,
    GridColumnGap = 0x6E,
    GridRowGap = 0x6F,
    
    // Spacing Properties (0x70-0x8F)
    Padding = 0x70,
    PaddingTop = 0x71,
    PaddingRight = 0x72,
    PaddingBottom = 0x73,
    PaddingLeft = 0x74,
    Margin = 0x75,
    MarginTop = 0x76,
    MarginRight = 0x77,
    MarginBottom = 0x78,
    MarginLeft = 0x79,
    BorderTopWidth = 0x7A,
    BorderRightWidth = 0x7B,
    BorderBottomWidth = 0x7C,
    BorderLeftWidth = 0x7D,
    BorderTopColor = 0x7E,
    BorderRightColor = 0x7F,
    BorderBottomColor = 0x80,
    BorderLeftColor = 0x81,
    BorderTopLeftRadius = 0x82,
    BorderTopRightRadius = 0x83,
    BorderBottomRightRadius = 0x84,
    BorderBottomLeftRadius = 0x85,
    BoxSizing = 0x86,
    Outline = 0x87,
    OutlineColor = 0x88,
    OutlineWidth = 0x89,
    OutlineOffset = 0x8A,
    Overflow = 0x8B,
    OverflowX = 0x8C,
    OverflowY = 0x8D,
    Spans = 0x8E,
    
    // Window Properties (0xA0-0xAF) - moved from earlier range
    WindowWidthNew = 0xA0,
    WindowHeightNew = 0xA1,
    WindowTitleNew = 0xA2,
    WindowResizableNew = 0xA3,
    WindowFullscreenNew = 0xA4,
    WindowVsyncNew = 0xA5,
    WindowTargetFpsNew = 0xA6,
    WindowAntialiasingNew = 0xA7,
    WindowIconNew = 0xA8,
    
    // Transform Properties (0xB0-0xBF)
    TransformScale = 0xB0,
    TransformScaleX = 0xB1,
    TransformScaleY = 0xB2,
    TransformScaleZ = 0xB3,
    TransformTranslateX = 0xB4,
    TransformTranslateY = 0xB5,
    TransformTranslateZ = 0xB6,
    TransformRotate = 0xB7,
    TransformRotateX = 0xB8,
    TransformRotateY = 0xB9,
    TransformRotateZ = 0xBA,
    TransformSkewX = 0xBB,
    TransformSkewY = 0xBC,
    TransformPerspective = 0xBD,
    TransformMatrix = 0xBE,
    
    // Semantic Properties (0xC0-0xCF)
    SemanticRole = 0xC0,
    HeadingLevel = 0xC1,
    ListType = 0xC2,
    ListItemRole = 0xC3,
    TableSection = 0xC4,
    InteractiveType = 0xC5,
    MediaType = 0xC6,
    EmbedType = 0xC7,
    InputTypeProperty = 0xC8,
    
    // Text Formatting Properties (0xD0-0xDF)
    FontStyle = 0xD0,
    TextDecoration = 0xD1,
    VerticalAlign = 0xD2,
    LineHeight = 0xD3,
    LetterSpacing = 0xD4,
    WordSpacing = 0xD5,
    TextIndent = 0xD6,
    TextTransform = 0xD7,
    TextShadow = 0xD8,
    WordWrap = 0xD9,
    TextOverflow = 0xDA,
    WritingMode = 0xDB,
    
    // Extended Visual Properties (0xE0-0xEF)
    BackgroundImage = 0xE0,
    BackgroundRepeat = 0xE1,
    BackgroundPosition = 0xE2,
    BackgroundSize = 0xE3,
    BackgroundAttachment = 0xE4,
    BorderStyle = 0xE5,
    BorderImage = 0xE6,
    Filter = 0xE7,
    BackdropFilter = 0xE8,
    ClipPath = 0xE9,
    Mask = 0xEA,
    MixBlendMode = 0xEB,
    ObjectFit = 0xEC,
    ObjectPosition = 0xED,
    
    // Special Properties (0xF0-0xFF)
    RichTextContent = 0xF1,
    AccessibilityLabel = 0xF2,
    AccessibilityRole = 0xF3,
    AccessibilityDescription = 0xF4,
    DataAttributes = 0xF5,
    CustomProperties = 0xF6,
    
    // Custom properties (0xFF for unknown)
    Custom(u8),
}

impl PropertyId {
    /// Create PropertyId from hex value
    pub fn from_u8(value: u8) -> Self {
        match value {
            0x01 => PropertyId::BackgroundColor,
            0x02 => PropertyId::TextColor,
            0x03 => PropertyId::BorderColor,
            0x04 => PropertyId::BorderWidth,
            0x05 => PropertyId::BorderRadius,
            0x06 => PropertyId::LayoutFlags,
            0x08 => PropertyId::TextContent,
            0x09 => PropertyId::FontSize,
            0x0A => PropertyId::FontWeight,
            0x0B => PropertyId::TextAlignment,
            0x0C => PropertyId::FontFamily,
            0x0D => PropertyId::ImageSource,
            0x0E => PropertyId::Opacity,
            0x0F => PropertyId::ZIndex,
            0x1E => PropertyId::ListStyleType,
            0x1F => PropertyId::WhiteSpace,
            0x10 => PropertyId::Visibility,
            0x11 => PropertyId::Gap,
            0x12 => PropertyId::MinWidth,
            0x13 => PropertyId::MinHeight,
            0x14 => PropertyId::MaxWidth,
            0x15 => PropertyId::MaxHeight,
            0x16 => PropertyId::Transform,
            0x18 => PropertyId::Shadow,
            0x19 => PropertyId::Width,
            0x1A => PropertyId::Height,
            0x1B => PropertyId::OldLayoutFlags,
            0x1D => PropertyId::StyleId,
            0x20 => PropertyId::WindowWidth,
            0x21 => PropertyId::WindowHeight,
            0x22 => PropertyId::WindowTitle,
            0x23 => PropertyId::WindowResizable,
            0x24 => PropertyId::WindowFullscreen,
            0x25 => PropertyId::WindowVsync,
            0x26 => PropertyId::WindowTargetFps,
            0x27 => PropertyId::WindowAntialiasing,
            0x28 => PropertyId::WindowIcon,
            0x29 => PropertyId::Cursor,
            0x40 => PropertyId::Display,
            0x41 => PropertyId::FlexDirection,
            0x42 => PropertyId::FlexWrap,
            0x43 => PropertyId::FlexGrow,
            0x44 => PropertyId::FlexShrink,
            0x45 => PropertyId::FlexBasis,
            0x46 => PropertyId::AlignItems,
            0x47 => PropertyId::AlignContent,
            0x48 => PropertyId::AlignSelf,
            0x49 => PropertyId::JustifyContent,
            0x4A => PropertyId::JustifyItems,
            0x4B => PropertyId::JustifySelf,
            0x50 => PropertyId::Position,
            0x51 => PropertyId::Left,
            0x52 => PropertyId::Top,
            0x53 => PropertyId::Right,
            0x54 => PropertyId::Bottom,
            0x60 => PropertyId::GridTemplateColumns,
            0x61 => PropertyId::GridTemplateRows,
            0x62 => PropertyId::GridTemplateAreas,
            0x63 => PropertyId::GridAutoColumns,
            0x64 => PropertyId::GridAutoRows,
            0x65 => PropertyId::GridAutoFlow,
            0x66 => PropertyId::GridArea,
            0x67 => PropertyId::GridColumn,
            0x68 => PropertyId::GridRow,
            0x69 => PropertyId::GridColumnStart,
            0x6A => PropertyId::GridColumnEnd,
            0x6B => PropertyId::GridRowStart,
            0x6C => PropertyId::GridRowEnd,
            0x6D => PropertyId::GridGap,
            0x6E => PropertyId::GridColumnGap,
            0x6F => PropertyId::GridRowGap,
            0x70 => PropertyId::Padding,
            0x71 => PropertyId::PaddingTop,
            0x72 => PropertyId::PaddingRight,
            0x73 => PropertyId::PaddingBottom,
            0x74 => PropertyId::PaddingLeft,
            0x75 => PropertyId::Margin,
            0x76 => PropertyId::MarginTop,
            0x77 => PropertyId::MarginRight,
            0x78 => PropertyId::MarginBottom,
            0x79 => PropertyId::MarginLeft,
            0x7A => PropertyId::BorderTopWidth,
            0x7B => PropertyId::BorderRightWidth,
            0x7C => PropertyId::BorderBottomWidth,
            0x7D => PropertyId::BorderLeftWidth,
            0x7E => PropertyId::BorderTopColor,
            0x7F => PropertyId::BorderRightColor,
            0x80 => PropertyId::BorderBottomColor,
            0x81 => PropertyId::BorderLeftColor,
            0x82 => PropertyId::BorderTopLeftRadius,
            0x83 => PropertyId::BorderTopRightRadius,
            0x84 => PropertyId::BorderBottomRightRadius,
            0x85 => PropertyId::BorderBottomLeftRadius,
            0x86 => PropertyId::BoxSizing,
            0x87 => PropertyId::Outline,
            0x88 => PropertyId::OutlineColor,
            0x89 => PropertyId::OutlineWidth,
            0x8A => PropertyId::OutlineOffset,
            0x8B => PropertyId::Overflow,
            0x8C => PropertyId::OverflowX,
            0x8D => PropertyId::OverflowY,
            0x8E => PropertyId::Spans,
            
            // Window Properties (0xA0-0xAF)
            0xA0 => PropertyId::WindowWidthNew,
            0xA1 => PropertyId::WindowHeightNew,
            0xA2 => PropertyId::WindowTitleNew,
            0xA3 => PropertyId::WindowResizableNew,
            0xA4 => PropertyId::WindowFullscreenNew,
            0xA5 => PropertyId::WindowVsyncNew,
            0xA6 => PropertyId::WindowTargetFpsNew,
            0xA7 => PropertyId::WindowAntialiasingNew,
            0xA8 => PropertyId::WindowIconNew,
            
            // Transform Properties (0xB0-0xBF)
            0xB0 => PropertyId::TransformScale,
            0xB1 => PropertyId::TransformScaleX,
            0xB2 => PropertyId::TransformScaleY,
            0xB3 => PropertyId::TransformScaleZ,
            0xB4 => PropertyId::TransformTranslateX,
            0xB5 => PropertyId::TransformTranslateY,
            0xB6 => PropertyId::TransformTranslateZ,
            0xB7 => PropertyId::TransformRotate,
            0xB8 => PropertyId::TransformRotateX,
            0xB9 => PropertyId::TransformRotateY,
            0xBA => PropertyId::TransformRotateZ,
            0xBB => PropertyId::TransformSkewX,
            0xBC => PropertyId::TransformSkewY,
            0xBD => PropertyId::TransformPerspective,
            0xBE => PropertyId::TransformMatrix,
            
            // Semantic Properties (0xC0-0xCF)
            0xC0 => PropertyId::SemanticRole,
            0xC1 => PropertyId::HeadingLevel,
            0xC2 => PropertyId::ListType,
            0xC3 => PropertyId::ListItemRole,
            0xC4 => PropertyId::TableSection,
            0xC5 => PropertyId::InteractiveType,
            0xC6 => PropertyId::MediaType,
            0xC7 => PropertyId::EmbedType,
            0xC8 => PropertyId::InputTypeProperty,
            
            // Text Formatting Properties (0xD0-0xDF)
            0xD0 => PropertyId::FontStyle,
            0xD1 => PropertyId::TextDecoration,
            0xD2 => PropertyId::VerticalAlign,
            0xD3 => PropertyId::LineHeight,
            0xD4 => PropertyId::LetterSpacing,
            0xD5 => PropertyId::WordSpacing,
            0xD6 => PropertyId::TextIndent,
            0xD7 => PropertyId::TextTransform,
            0xD8 => PropertyId::TextShadow,
            0xD9 => PropertyId::WordWrap,
            0xDA => PropertyId::TextOverflow,
            0xDB => PropertyId::WritingMode,
            
            // Extended Visual Properties (0xE0-0xEF)
            0xE0 => PropertyId::BackgroundImage,
            0xE1 => PropertyId::BackgroundRepeat,
            0xE2 => PropertyId::BackgroundPosition,
            0xE3 => PropertyId::BackgroundSize,
            0xE4 => PropertyId::BackgroundAttachment,
            0xE5 => PropertyId::BorderStyle,
            0xE6 => PropertyId::BorderImage,
            0xE7 => PropertyId::Filter,
            0xE8 => PropertyId::BackdropFilter,
            0xE9 => PropertyId::ClipPath,
            0xEA => PropertyId::Mask,
            0xEB => PropertyId::MixBlendMode,
            0xEC => PropertyId::ObjectFit,
            0xED => PropertyId::ObjectPosition,
            
            // Special Properties (0xF0-0xFF)
            0xF1 => PropertyId::RichTextContent,
            0xF2 => PropertyId::AccessibilityLabel,
            0xF3 => PropertyId::AccessibilityRole,
            0xF4 => PropertyId::AccessibilityDescription,
            0xF5 => PropertyId::DataAttributes,
            0xF6 => PropertyId::CustomProperties,
            
            other => PropertyId::Custom(other),
        }
    }
    
    /// Convert PropertyId to hex value
    pub fn to_u8(self) -> u8 {
        match self {
            PropertyId::Custom(value) => value,
            _ => self as u8,
        }
    }
}

impl From<u8> for PropertyId {
    fn from(value: u8) -> Self {
        Self::from_u8(value)
    }
}

// =============================================================================
// TRANSFORM PROPERTIES
// =============================================================================

/// Transform property types with their hex values (0xB0-0xBF range)
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[cfg_attr(feature = "serde_support", derive(Serialize, Deserialize))]
#[repr(u8)]
pub enum TransformPropertyType {
    Scale = 0xB0,
    ScaleX = 0xB1,
    ScaleY = 0xB2,
    ScaleZ = 0xB3,
    TranslateX = 0xB4,
    TranslateY = 0xB5,
    TranslateZ = 0xB6,
    Rotate = 0xB7,
    RotateX = 0xB8,
    RotateY = 0xB9,
    RotateZ = 0xBA,
    SkewX = 0xBB,
    SkewY = 0xBC,
    Perspective = 0xBD,
    Matrix = 0xBE,
}

impl TransformPropertyType {
    pub fn from_u8(value: u8) -> Self {
        match value {
            0xB0 => Self::Scale,
            0xB1 => Self::ScaleX,
            0xB2 => Self::ScaleY,
            0xB3 => Self::ScaleZ,
            0xB4 => Self::TranslateX,
            0xB5 => Self::TranslateY,
            0xB6 => Self::TranslateZ,
            0xB7 => Self::Rotate,
            0xB8 => Self::RotateX,
            0xB9 => Self::RotateY,
            0xBA => Self::RotateZ,
            0xBB => Self::SkewX,
            0xBC => Self::SkewY,
            0xBD => Self::Perspective,
            0xBE => Self::Matrix,
            _ => Self::Scale, // Default fallback
        }
    }
}

// =============================================================================
// OTHER SHARED TYPES
// =============================================================================

/// Interactive states for UI elements
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "serde_support", derive(Serialize, Deserialize))]
pub enum InteractionState {
    Normal = 0,
    Hover = 1,
    Active = 2,
    Focus = 4,
    Disabled = 8,
    Checked = 16,
}

/// Font weight values
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[cfg_attr(feature = "serde_support", derive(Serialize, Deserialize))]
pub enum FontWeight {
    Light = 300,
    Normal = 400,
    Bold = 700,
    Heavy = 900,
}

/// Text alignment options
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[cfg_attr(feature = "serde_support", derive(Serialize, Deserialize))]
pub enum TextAlignment {
    Start,
    Center,
    End,
    Justify,
}

/// Cursor types
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[cfg_attr(feature = "serde_support", derive(Serialize, Deserialize))]
pub enum CursorType {
    Default,
    Pointer,
    Text,
    Move,
    NotAllowed,
}

/// Input types for Input elements
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[cfg_attr(feature = "serde_support", derive(Serialize, Deserialize))]
#[repr(u8)]
pub enum InputType {
    // Textual inputs
    Text = 0x00,
    Password = 0x01,
    Email = 0x02,
    Number = 0x03,
    Tel = 0x04,
    Url = 0x05,
    Search = 0x06,
    
    // Selection inputs
    Checkbox = 0x10,
    Radio = 0x11,
    
    // Range input
    Range = 0x20,
    
    // Date and time inputs
    Date = 0x30,
    DatetimeLocal = 0x31,
    Month = 0x32,
    Time = 0x33,
    Week = 0x34,
    
    // Specialized inputs
    Color = 0x40,
    File = 0x41,
    Hidden = 0x42,
    
    // Button inputs
    Submit = 0x50,
    Reset = 0x51,
    Button = 0x52,
    Image = 0x53,
}

impl InputType {
    pub fn from_name(name: &str) -> Option<Self> {
        match name {
            "text" => Some(Self::Text),
            "password" => Some(Self::Password),
            "email" => Some(Self::Email),
            "number" => Some(Self::Number),
            "tel" => Some(Self::Tel),
            "url" => Some(Self::Url),
            "search" => Some(Self::Search),
            "checkbox" => Some(Self::Checkbox),
            "radio" => Some(Self::Radio),
            "range" => Some(Self::Range),
            "date" => Some(Self::Date),
            "datetime-local" => Some(Self::DatetimeLocal),
            "month" => Some(Self::Month),
            "time" => Some(Self::Time),
            "week" => Some(Self::Week),
            "color" => Some(Self::Color),
            "file" => Some(Self::File),
            "hidden" => Some(Self::Hidden),
            "submit" => Some(Self::Submit),
            "reset" => Some(Self::Reset),
            "button" => Some(Self::Button),
            "image" => Some(Self::Image),
            _ => None,
        }
    }
    
    pub fn to_name(self) -> &'static str {
        match self {
            Self::Text => "text",
            Self::Password => "password",
            Self::Email => "email",
            Self::Number => "number",
            Self::Tel => "tel",
            Self::Url => "url",
            Self::Search => "search",
            Self::Checkbox => "checkbox",
            Self::Radio => "radio",
            Self::Range => "range",
            Self::Date => "date",
            Self::DatetimeLocal => "datetime-local",
            Self::Month => "month",
            Self::Time => "time",
            Self::Week => "week",
            Self::Color => "color",
            Self::File => "file",
            Self::Hidden => "hidden",
            Self::Submit => "submit",
            Self::Reset => "reset",
            Self::Button => "button",
            Self::Image => "image",
        }
    }
}

/// EmbedView types
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[cfg_attr(feature = "serde_support", derive(Serialize, Deserialize))]
#[repr(u8)]
pub enum EmbedViewType {
    WebView = 0x00,
    NativeRenderer = 0x01,
    WasmView = 0x02,
    UxnView = 0x03,
    GbaView = 0x04,
    DosView = 0x05,
    CodeEditor = 0x06,
    Terminal = 0x07,
    ModelViewer = 0x08,
}

impl EmbedViewType {
    pub fn from_name(name: &str) -> Option<Self> {
        match name {
            "webview" | "web" => Some(Self::WebView),
            "native_renderer" | "native" => Some(Self::NativeRenderer),
            "wasm_view" | "wasm" => Some(Self::WasmView),
            "uxn_view" | "uxn" => Some(Self::UxnView),
            "gba_view" | "gba" => Some(Self::GbaView),
            "dos_view" | "dos" => Some(Self::DosView),
            "code_editor" | "editor" => Some(Self::CodeEditor),
            "terminal" | "term" => Some(Self::Terminal),
            "model_viewer" | "model" => Some(Self::ModelViewer),
            _ => None,
        }
    }
    
    pub fn to_name(self) -> &'static str {
        match self {
            Self::WebView => "webview",
            Self::NativeRenderer => "native_renderer",
            Self::WasmView => "wasm_view",
            Self::UxnView => "uxn_view",
            Self::GbaView => "gba_view",
            Self::DosView => "dos_view",
            Self::CodeEditor => "code_editor",
            Self::Terminal => "terminal",
            Self::ModelViewer => "model_viewer",
        }
    }
}

/// Event types
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "serde_support", derive(Serialize, Deserialize))]
pub enum EventType {
    Click,
    Press,
    Release,
    Hover,
    Focus,
    Blur,
    Change,
    Submit,
}