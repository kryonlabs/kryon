//! Unified Type System for Kryon
//! 
//! This is the single source of truth for all element types, property IDs, 
//! and other type definitions used throughout the Kryon ecosystem.
//! 
//! ALL OTHER CRATES MUST IMPORT FROM HERE - DO NOT DUPLICATE DEFINITIONS!


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
    
    // Extended Widgets (0x60-0x7F) - Feature gated
    #[cfg(feature = "dropdown")]
    Dropdown = 0x60,
    #[cfg(feature = "data-grid")]
    DataGrid = 0x61,
    #[cfg(feature = "date-picker")]
    EnhancedDatePicker = 0x62,
    #[cfg(feature = "rich-text")]
    RichText = 0x63,
    #[cfg(feature = "color-picker")]
    ColorPicker = 0x64,
    #[cfg(feature = "file-upload")]
    FileUpload = 0x65,
    #[cfg(feature = "number-input")]
    NumberInput = 0x66,
    #[cfg(feature = "range-slider")]
    RangeSlider = 0x67,
    
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
            
            // Extended Widgets (0x60-0x7F) - Feature gated
            #[cfg(feature = "dropdown")]
            0x60 => ElementType::Dropdown,
            #[cfg(feature = "data-grid")]
            0x61 => ElementType::DataGrid,
            #[cfg(feature = "date-picker")]
            0x62 => ElementType::EnhancedDatePicker,
            #[cfg(feature = "rich-text")]
            0x63 => ElementType::RichText,
            #[cfg(feature = "color-picker")]
            0x64 => ElementType::ColorPicker,
            #[cfg(feature = "file-upload")]
            0x65 => ElementType::FileUpload,
            #[cfg(feature = "number-input")]
            0x66 => ElementType::NumberInput,
            #[cfg(feature = "range-slider")]
            0x67 => ElementType::RangeSlider,
            
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
            ElementType::App => 0x00,
            ElementType::Container => 0x01,
            ElementType::Text => 0x02,
            ElementType::EmbedView => 0x03,
            ElementType::Button => 0x10,
            ElementType::Input => 0x11,
            ElementType::Link => 0x12,
            ElementType::Image => 0x20,
            ElementType::Video => 0x21,
            ElementType::Canvas => 0x22,
            ElementType::Svg => 0x23,
            ElementType::Select => 0x30,
            ElementType::Slider => 0x31,
            ElementType::ProgressBar => 0x32,
            ElementType::Checkbox => 0x33,
            ElementType::RadioGroup => 0x34,
            ElementType::Toggle => 0x35,
            ElementType::DatePicker => 0x36,
            ElementType::FilePicker => 0x37,
            
            // Extended Widgets (0x60-0x7F) - Feature gated
            #[cfg(feature = "dropdown")]
            ElementType::Dropdown => 0x60,
            #[cfg(feature = "data-grid")]
            ElementType::DataGrid => 0x61,
            #[cfg(feature = "date-picker")]
            ElementType::EnhancedDatePicker => 0x62,
            #[cfg(feature = "rich-text")]
            ElementType::RichText => 0x63,
            #[cfg(feature = "color-picker")]
            ElementType::ColorPicker => 0x64,
            #[cfg(feature = "file-upload")]
            ElementType::FileUpload => 0x65,
            #[cfg(feature = "number-input")]
            ElementType::NumberInput => 0x66,
            #[cfg(feature = "range-slider")]
            ElementType::RangeSlider => 0x67,
            
            ElementType::Form => 0x40,
            ElementType::List => 0x41,
            ElementType::ListItem => 0x42,
            ElementType::Table => 0x50,
            ElementType::TableRow => 0x51,
            ElementType::TableCell => 0x52,
            ElementType::TableHeader => 0x53,
            ElementType::Custom(value) => value,
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
            
            // Extended Widgets
            #[cfg(feature = "dropdown")]
            "Dropdown" => Self::Dropdown,
            #[cfg(feature = "data-grid")]
            "DataGrid" => Self::DataGrid,
            #[cfg(feature = "date-picker")]
            "EnhancedDatePicker" => Self::EnhancedDatePicker,
            #[cfg(feature = "rich-text")]
            "RichText" => Self::RichText,
            #[cfg(feature = "color-picker")]
            "ColorPicker" => Self::ColorPicker,
            #[cfg(feature = "file-upload")]
            "FileUpload" => Self::FileUpload,
            #[cfg(feature = "number-input")]
            "NumberInput" => Self::NumberInput,
            #[cfg(feature = "range-slider")]
            "RangeSlider" => Self::RangeSlider,
            
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
            
            // Extended Widgets
            #[cfg(feature = "dropdown")]
            ElementType::Dropdown => "Dropdown",
            #[cfg(feature = "data-grid")]
            ElementType::DataGrid => "DataGrid",
            #[cfg(feature = "date-picker")]
            ElementType::EnhancedDatePicker => "EnhancedDatePicker",
            #[cfg(feature = "rich-text")]
            ElementType::RichText => "RichText",
            #[cfg(feature = "color-picker")]
            ElementType::ColorPicker => "ColorPicker",
            #[cfg(feature = "file-upload")]
            ElementType::FileUpload => "FileUpload",
            #[cfg(feature = "number-input")]
            ElementType::NumberInput => "NumberInput",
            #[cfg(feature = "range-slider")]
            ElementType::RangeSlider => "RangeSlider",
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
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
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
    
    // New legacy properties with unique values
    CustomData = 0xF7, // For legacy compatibility
    Author = 0xF8, // New legacy property
    Version = 0xF9, // New legacy property
    BoxShadow = 0x97, // New property
    MinViewportWidth = 0xFA,
    MaxViewportWidth = 0xFB,
    Order = 0xFC, // New unique value
    
    // Custom properties (0xFF for unknown)
    Custom(u8),
}

impl PropertyId {
    /// Create PropertyId from string name
    pub fn from_name(name: &str) -> Self {
        let result = match name {
            // Visual Properties (PascalCase and snake_case)
            "BackgroundColor" | "background_color" => PropertyId::BackgroundColor,
            "TextColor" | "ForegroundColor" | "text_color" | "foreground_color" => PropertyId::TextColor,
            "BorderColor" | "border_color" => PropertyId::BorderColor,
            "BorderWidth" | "border_width" => PropertyId::BorderWidth,
            "BorderRadius" | "border_radius" => PropertyId::BorderRadius,
            "LayoutFlags" | "layout_flags" => PropertyId::LayoutFlags,
            "TextContent" | "text_content" | "text" => PropertyId::TextContent,
            "FontSize" | "font_size" => PropertyId::FontSize,
            "FontWeight" | "font_weight" => PropertyId::FontWeight,
            "TextAlignment" | "text_alignment" => PropertyId::TextAlignment,
            "FontFamily" | "font_family" => PropertyId::FontFamily,
            "ImageSource" => PropertyId::ImageSource,
            "Opacity" => PropertyId::Opacity,
            "ZIndex" => PropertyId::ZIndex,
            
            // Display Properties (PascalCase and snake_case)
            "Visibility" | "visibility" => PropertyId::Visibility,
            "Gap" | "gap" => PropertyId::Gap,
            "Padding" | "padding" => PropertyId::Padding,
            "PaddingTop" | "padding_top" => PropertyId::PaddingTop,
            "PaddingRight" | "padding_right" => PropertyId::PaddingRight,
            "PaddingBottom" | "padding_bottom" => PropertyId::PaddingBottom,
            "PaddingLeft" | "padding_left" => PropertyId::PaddingLeft,
            "Margin" | "margin" => PropertyId::Margin,
            "MarginTop" | "margin_top" => PropertyId::MarginTop,
            "MarginRight" | "margin_right" => PropertyId::MarginRight,
            "MarginBottom" | "margin_bottom" => PropertyId::MarginBottom,
            "MarginLeft" | "margin_left" => PropertyId::MarginLeft,
            "MinWidth" | "min_width" => PropertyId::MinWidth,
            "MinHeight" | "min_height" => PropertyId::MinHeight,
            "MaxWidth" | "max_width" => PropertyId::MaxWidth,
            "MaxHeight" | "max_height" => PropertyId::MaxHeight,
            "Transform" | "transform" => PropertyId::Transform,
            "Shadow" | "shadow" => PropertyId::Shadow,
            "Width" | "width" => PropertyId::Width,
            "Height" | "height" => PropertyId::Height,
            "StyleId" | "style_id" => PropertyId::StyleId,
            "ListStyleType" | "list_style_type" => PropertyId::ListStyleType,
            "WhiteSpace" | "white_space" => PropertyId::WhiteSpace,
            
            // Window Properties (PascalCase and snake_case)
            "WindowWidth" | "window_width" => PropertyId::WindowWidth,
            "WindowHeight" | "window_height" => PropertyId::WindowHeight,
            "WindowTitle" | "window_title" => PropertyId::WindowTitle,
            "WindowResizable" | "Resizable" | "resizable" => PropertyId::WindowResizable,
            "WindowFullscreen" | "window_fullscreen" => PropertyId::WindowFullscreen,
            "WindowVsync" | "window_vsync" => PropertyId::WindowVsync,
            "WindowTargetFps" | "window_target_fps" => PropertyId::WindowTargetFps,
            "WindowAntialiasing" | "window_antialiasing" => PropertyId::WindowAntialiasing,
            "WindowIcon" | "window_icon" | "Icon" | "icon" => PropertyId::WindowIcon,
            "Cursor" | "cursor" => PropertyId::Cursor,
            
            // Flexbox Properties (PascalCase and snake_case)
            "Display" | "display" => PropertyId::Display,
            "FlexDirection" | "flex_direction" => PropertyId::FlexDirection,
            "FlexWrap" | "flex_wrap" => PropertyId::FlexWrap,
            "FlexGrow" | "flex_grow" => PropertyId::FlexGrow,
            "FlexShrink" | "flex_shrink" => PropertyId::FlexShrink,
            "FlexBasis" | "flex_basis" => PropertyId::FlexBasis,
            "AlignItems" | "align_items" => PropertyId::AlignItems,
            "AlignContent" | "align_content" => PropertyId::AlignContent,
            "AlignSelf" | "align_self" => PropertyId::AlignSelf,
            "JustifyContent" | "justify_content" => PropertyId::JustifyContent,
            "JustifyItems" | "justify_items" => PropertyId::JustifyItems,
            "JustifySelf" | "justify_self" => PropertyId::JustifySelf,
            
            // Position Properties (PascalCase and snake_case)
            "Position" | "position" => PropertyId::Position,
            "Left" | "left" => PropertyId::Left,
            "Top" | "top" => PropertyId::Top,
            "Right" | "right" => PropertyId::Right,
            "Bottom" | "bottom" => PropertyId::Bottom,
            "PosX" | "pos_x" => PropertyId::Left,  // Alias
            "PosY" | "pos_y" => PropertyId::Top,   // Alias
            
            // Grid Properties (PascalCase and snake_case)
            "GridTemplateColumns" | "grid_template_columns" => PropertyId::GridTemplateColumns,
            "GridTemplateRows" | "grid_template_rows" => PropertyId::GridTemplateRows,
            "GridTemplateAreas" | "grid_template_areas" => PropertyId::GridTemplateAreas,
            "GridAutoColumns" | "grid_auto_columns" => PropertyId::GridAutoColumns,
            "GridAutoRows" | "grid_auto_rows" => PropertyId::GridAutoRows,
            "GridAutoFlow" | "grid_auto_flow" => PropertyId::GridAutoFlow,
            "GridArea" | "grid_area" => PropertyId::GridArea,
            "GridColumn" | "grid_column" => PropertyId::GridColumn,
            "GridRow" | "grid_row" => PropertyId::GridRow,
            "GridColumnStart" | "grid_column_start" => PropertyId::GridColumnStart,
            "GridColumnEnd" | "grid_column_end" => PropertyId::GridColumnEnd,
            "GridRowStart" | "grid_row_start" => PropertyId::GridRowStart,
            "GridRowEnd" | "grid_row_end" => PropertyId::GridRowEnd,
            "GridGap" | "grid_gap" => PropertyId::GridGap,
            "GridColumnGap" | "grid_column_gap" => PropertyId::GridColumnGap,
            "GridRowGap" | "grid_row_gap" => PropertyId::GridRowGap,
            
            // Border Properties (PascalCase and snake_case)
            "BorderTopWidth" | "border_top_width" => PropertyId::BorderTopWidth,
            "BorderRightWidth" | "border_right_width" => PropertyId::BorderRightWidth,
            "BorderBottomWidth" | "border_bottom_width" => PropertyId::BorderBottomWidth,
            "BorderLeftWidth" | "border_left_width" => PropertyId::BorderLeftWidth,
            "BorderTopColor" | "border_top_color" => PropertyId::BorderTopColor,
            "BorderRightColor" | "border_right_color" => PropertyId::BorderRightColor,
            "BorderBottomColor" | "border_bottom_color" => PropertyId::BorderBottomColor,
            "BorderLeftColor" | "border_left_color" => PropertyId::BorderLeftColor,
            "BorderTopLeftRadius" | "border_top_left_radius" => PropertyId::BorderTopLeftRadius,
            "BorderTopRightRadius" | "border_top_right_radius" => PropertyId::BorderTopRightRadius,
            "BorderBottomRightRadius" | "border_bottom_right_radius" => PropertyId::BorderBottomRightRadius,
            "BorderBottomLeftRadius" | "border_bottom_left_radius" => PropertyId::BorderBottomLeftRadius,
            "BoxSizing" | "box_sizing" => PropertyId::BoxSizing,
            "Outline" | "outline" => PropertyId::Outline,
            "OutlineColor" | "outline_color" => PropertyId::OutlineColor,
            "OutlineWidth" | "outline_width" => PropertyId::OutlineWidth,
            "OutlineOffset" | "outline_offset" => PropertyId::OutlineOffset,
            "Overflow" | "overflow" => PropertyId::Overflow,
            "OverflowX" | "overflow_x" => PropertyId::OverflowX,
            "OverflowY" | "overflow_y" => PropertyId::OverflowY,
            "Spans" | "spans" => PropertyId::Spans,
            
            // Transform Properties (PascalCase and snake_case)
            "TransformScale" | "transform_scale" => PropertyId::TransformScale,
            "TransformScaleX" | "transform_scale_x" => PropertyId::TransformScaleX,
            "TransformScaleY" | "transform_scale_y" => PropertyId::TransformScaleY,
            "TransformScaleZ" | "transform_scale_z" => PropertyId::TransformScaleZ,
            "TransformTranslateX" | "transform_translate_x" => PropertyId::TransformTranslateX,
            "TransformTranslateY" | "transform_translate_y" => PropertyId::TransformTranslateY,
            "TransformTranslateZ" | "transform_translate_z" => PropertyId::TransformTranslateZ,
            "TransformRotate" | "transform_rotate" => PropertyId::TransformRotate,
            "TransformRotateX" | "transform_rotate_x" => PropertyId::TransformRotateX,
            "TransformRotateY" | "transform_rotate_y" => PropertyId::TransformRotateY,
            "TransformRotateZ" | "transform_rotate_z" => PropertyId::TransformRotateZ,
            "TransformSkewX" | "transform_skew_x" => PropertyId::TransformSkewX,
            "TransformSkewY" | "transform_skew_y" => PropertyId::TransformSkewY,
            "TransformPerspective" | "transform_perspective" => PropertyId::TransformPerspective,
            "TransformMatrix" | "transform_matrix" => PropertyId::TransformMatrix,
            
            // Semantic Properties (PascalCase and snake_case)
            "SemanticRole" | "semantic_role" => PropertyId::SemanticRole,
            "HeadingLevel" | "heading_level" => PropertyId::HeadingLevel,
            "ListType" | "list_type" => PropertyId::ListType,
            "ListItemRole" | "list_item_role" => PropertyId::ListItemRole,
            "TableSection" | "table_section" => PropertyId::TableSection,
            "InteractiveType" | "interactive_type" => PropertyId::InteractiveType,
            "MediaType" | "media_type" => PropertyId::MediaType,
            "EmbedType" | "embed_type" => PropertyId::EmbedType,
            "InputTypeProperty" | "input_type_property" => PropertyId::InputTypeProperty,
            
            // Text Formatting Properties (PascalCase and snake_case)
            "FontStyle" | "font_style" => PropertyId::FontStyle,
            "TextDecoration" | "text_decoration" => PropertyId::TextDecoration,
            "VerticalAlign" | "vertical_align" => PropertyId::VerticalAlign,
            "LineHeight" | "line_height" => PropertyId::LineHeight,
            "LetterSpacing" | "letter_spacing" => PropertyId::LetterSpacing,
            "WordSpacing" | "word_spacing" => PropertyId::WordSpacing,
            "TextIndent" | "text_indent" => PropertyId::TextIndent,
            "TextTransform" | "text_transform" => PropertyId::TextTransform,
            "TextShadow" | "text_shadow" => PropertyId::TextShadow,
            "WordWrap" | "word_wrap" => PropertyId::WordWrap,
            "TextOverflow" | "text_overflow" => PropertyId::TextOverflow,
            "WritingMode" | "writing_mode" => PropertyId::WritingMode,
            
            // Extended Visual Properties (PascalCase and snake_case)
            "BackgroundImage" | "background_image" => PropertyId::BackgroundImage,
            "BackgroundRepeat" | "background_repeat" => PropertyId::BackgroundRepeat,
            "BackgroundPosition" | "background_position" => PropertyId::BackgroundPosition,
            "BackgroundSize" | "background_size" => PropertyId::BackgroundSize,
            "BackgroundAttachment" | "background_attachment" => PropertyId::BackgroundAttachment,
            "BorderStyle" | "border_style" => PropertyId::BorderStyle,
            "BorderImage" | "border_image" => PropertyId::BorderImage,
            "Filter" | "filter" => PropertyId::Filter,
            "BackdropFilter" | "backdrop_filter" => PropertyId::BackdropFilter,
            "ClipPath" | "clip_path" => PropertyId::ClipPath,
            "Mask" | "mask" => PropertyId::Mask,
            "MixBlendMode" | "mix_blend_mode" => PropertyId::MixBlendMode,
            "ObjectFit" | "object_fit" => PropertyId::ObjectFit,
            "ObjectPosition" | "object_position" => PropertyId::ObjectPosition,
            
            // Special Properties (PascalCase and snake_case)
            "RichTextContent" | "rich_text_content" => PropertyId::RichTextContent,
            "AccessibilityLabel" | "accessibility_label" => PropertyId::AccessibilityLabel,
            "AccessibilityRole" | "accessibility_role" => PropertyId::AccessibilityRole,
            "AccessibilityDescription" | "accessibility_description" => PropertyId::AccessibilityDescription,
            "DataAttributes" | "data_attributes" => PropertyId::DataAttributes,
            "CustomProperties" | "custom_properties" => PropertyId::CustomProperties,
            "BoxShadow" | "box_shadow" => PropertyId::BoxShadow,
            "MinViewportWidth" | "min_viewport_width" => PropertyId::MinViewportWidth,
            "MaxViewportWidth" | "max_viewport_width" => PropertyId::MaxViewportWidth,
            "Order" | "order" => PropertyId::Order,
            
            // Special aliases (PascalCase and snake_case)
            "KeepAspect" | "keep_aspect_ratio" => PropertyId::WindowFullscreen, // Map to proper property
            "ScaleFactor" | "scale_factor" => PropertyId::WindowTargetFps, // Map to proper property
            "Version" | "version" => PropertyId::Version, // Use proper enum variant
            "Author" | "author" => PropertyId::Author, // Use proper enum variant
            
            _ => PropertyId::CustomData, // Unknown properties fall back to CustomData
        };
        
        
        result
    }
    
    /// Get the string name of this property
    pub fn to_name(self) -> &'static str {
        match self {
            // Visual Properties
            PropertyId::BackgroundColor => "BackgroundColor",
            PropertyId::TextColor => "TextColor",
            PropertyId::BorderColor => "BorderColor",
            PropertyId::BorderWidth => "BorderWidth",
            PropertyId::BorderRadius => "BorderRadius",
            PropertyId::LayoutFlags => "LayoutFlags",
            PropertyId::TextContent => "TextContent",
            PropertyId::FontSize => "FontSize",
            PropertyId::FontWeight => "FontWeight",
            PropertyId::TextAlignment => "TextAlignment",
            PropertyId::FontFamily => "FontFamily",
            PropertyId::ImageSource => "ImageSource",
            PropertyId::Opacity => "Opacity",
            PropertyId::ZIndex => "ZIndex",
            
            // Display Properties
            PropertyId::Visibility => "Visibility",
            PropertyId::Gap => "Gap",
            PropertyId::MinWidth => "MinWidth",
            PropertyId::MinHeight => "MinHeight",
            PropertyId::MaxWidth => "MaxWidth",
            PropertyId::MaxHeight => "MaxHeight",
            PropertyId::Transform => "Transform",
            PropertyId::Shadow => "Shadow",
            PropertyId::Width => "Width",
            PropertyId::Height => "Height",
            PropertyId::StyleId => "StyleId",
            PropertyId::ListStyleType => "ListStyleType",
            PropertyId::WhiteSpace => "WhiteSpace",
            
            // Window Properties
            PropertyId::WindowWidth => "WindowWidth",
            PropertyId::WindowHeight => "WindowHeight",
            PropertyId::WindowTitle => "WindowTitle",
            PropertyId::WindowResizable => "WindowResizable",
            PropertyId::WindowFullscreen => "WindowFullscreen",
            PropertyId::WindowVsync => "WindowVsync",
            PropertyId::WindowTargetFps => "WindowTargetFps",
            PropertyId::WindowAntialiasing => "WindowAntialiasing",
            PropertyId::WindowIcon => "WindowIcon",
            PropertyId::Cursor => "Cursor",
            
            // Flexbox Properties
            PropertyId::Display => "Display",
            PropertyId::FlexDirection => "FlexDirection",
            PropertyId::FlexWrap => "FlexWrap",
            PropertyId::FlexGrow => "FlexGrow",
            PropertyId::FlexShrink => "FlexShrink",
            PropertyId::FlexBasis => "FlexBasis",
            PropertyId::AlignItems => "AlignItems",
            PropertyId::AlignContent => "AlignContent",
            PropertyId::AlignSelf => "AlignSelf",
            PropertyId::JustifyContent => "JustifyContent",
            PropertyId::JustifyItems => "JustifyItems",
            PropertyId::JustifySelf => "JustifySelf",
            
            // Position Properties
            PropertyId::Position => "Position",
            PropertyId::Left => "Left",
            PropertyId::Top => "Top",
            PropertyId::Right => "Right",
            PropertyId::Bottom => "Bottom",
            
            _ => "Custom",
        }
    }
    
    /// Convert PropertyId to CSS property name for HTML rendering
    pub fn to_css_name(self) -> &'static str {
        match self {
            // Visual Properties → CSS equivalents
            PropertyId::BackgroundColor => "background-color",
            PropertyId::TextColor => "color",
            PropertyId::BorderColor => "border-color", 
            PropertyId::BorderWidth => "border-width",
            PropertyId::BorderRadius => "border-radius",
            PropertyId::FontSize => "font-size",
            PropertyId::FontWeight => "font-weight",
            PropertyId::FontStyle => "font-style",
            PropertyId::TextAlignment => "text-align",
            PropertyId::FontFamily => "font-family",
            PropertyId::Opacity => "opacity",
            PropertyId::ZIndex => "z-index",
            
            // Layout Properties → CSS equivalents
            PropertyId::Display => "display",
            PropertyId::Visibility => "visibility",
            PropertyId::Position => "position",
            PropertyId::Left => "left",
            PropertyId::Top => "top", 
            PropertyId::Right => "right",
            PropertyId::Bottom => "bottom",
            PropertyId::Width => "width",
            PropertyId::Height => "height",
            PropertyId::MinWidth => "min-width",
            PropertyId::MinHeight => "min-height",
            PropertyId::MaxWidth => "max-width",
            PropertyId::MaxHeight => "max-height",
            
            // Spacing Properties → CSS equivalents
            PropertyId::Padding => "padding",
            PropertyId::PaddingTop => "padding-top",
            PropertyId::PaddingRight => "padding-right", 
            PropertyId::PaddingBottom => "padding-bottom",
            PropertyId::PaddingLeft => "padding-left",
            PropertyId::Margin => "margin",
            PropertyId::MarginTop => "margin-top",
            PropertyId::MarginRight => "margin-right",
            PropertyId::MarginBottom => "margin-bottom", 
            PropertyId::MarginLeft => "margin-left",
            PropertyId::Gap => "gap",
            
            // Flexbox Properties → CSS equivalents
            PropertyId::FlexDirection => "flex-direction",
            PropertyId::FlexWrap => "flex-wrap",
            PropertyId::FlexGrow => "flex-grow",
            PropertyId::FlexShrink => "flex-shrink",
            PropertyId::FlexBasis => "flex-basis",
            PropertyId::AlignItems => "align-items",
            PropertyId::AlignContent => "align-content",
            PropertyId::AlignSelf => "align-self",
            PropertyId::JustifyContent => "justify-content",
            PropertyId::JustifyItems => "justify-items",
            PropertyId::JustifySelf => "justify-self",
            
            // Transform & Effects → CSS equivalents
            PropertyId::Transform => "transform",
            PropertyId::Shadow => "box-shadow",
            
            // Non-CSS properties (handled specially by renderers)
            PropertyId::WindowWidth => "window-width",
            PropertyId::WindowHeight => "window-height", 
            PropertyId::WindowTitle => "window-title",
            PropertyId::TextContent => "content",
            PropertyId::ImageSource => "src",
            
            // Fallback for unknown properties
            _ => "unknown-property",
        }
    }
    
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
            
            // Legacy compatibility mappings
            0xF7 => PropertyId::CustomData,
            0xF8 => PropertyId::Author,
            0xF9 => PropertyId::Version,
            0x97 => PropertyId::BoxShadow,
            0xFA => PropertyId::MinViewportWidth,
            0xFB => PropertyId::MaxViewportWidth,
            0xFC => PropertyId::Order,
            
            other => PropertyId::Custom(other),
        }
    }
    
    /// Convert PropertyId to hex value
    pub fn to_u8(self) -> u8 {
        match self {
            PropertyId::BackgroundColor => 0x01,
            PropertyId::TextColor => 0x02,
            PropertyId::BorderColor => 0x03,
            PropertyId::BorderWidth => 0x04,
            PropertyId::BorderRadius => 0x05,
            PropertyId::LayoutFlags => 0x06,
            PropertyId::TextContent => 0x08,
            PropertyId::FontSize => 0x09,
            PropertyId::FontWeight => 0x0A,
            PropertyId::TextAlignment => 0x0B,
            PropertyId::FontFamily => 0x0C,
            PropertyId::ImageSource => 0x0D,
            PropertyId::Opacity => 0x0E,
            PropertyId::ZIndex => 0x0F,
            PropertyId::Visibility => 0x10,
            PropertyId::Gap => 0x11,
            PropertyId::MinWidth => 0x12,
            PropertyId::MinHeight => 0x13,
            PropertyId::MaxWidth => 0x14,
            PropertyId::MaxHeight => 0x15,
            PropertyId::Transform => 0x16,
            PropertyId::Shadow => 0x18,
            PropertyId::Width => 0x19,
            PropertyId::Height => 0x1A,
            PropertyId::OldLayoutFlags => 0x1B,
            PropertyId::StyleId => 0x1D,
            PropertyId::ListStyleType => 0x1E,
            PropertyId::WhiteSpace => 0x1F,
            PropertyId::WindowWidth => 0x20,
            PropertyId::WindowHeight => 0x21,
            PropertyId::WindowTitle => 0x22,
            PropertyId::WindowResizable => 0x23,
            PropertyId::WindowFullscreen => 0x24,
            PropertyId::WindowVsync => 0x25,
            PropertyId::WindowTargetFps => 0x26,
            PropertyId::WindowAntialiasing => 0x27,
            PropertyId::WindowIcon => 0x28,
            PropertyId::Cursor => 0x29,
            PropertyId::Display => 0x40,
            PropertyId::FlexDirection => 0x41,
            PropertyId::FlexWrap => 0x42,
            PropertyId::FlexGrow => 0x43,
            PropertyId::FlexShrink => 0x44,
            PropertyId::FlexBasis => 0x45,
            PropertyId::AlignItems => 0x46,
            PropertyId::AlignContent => 0x47,
            PropertyId::AlignSelf => 0x48,
            PropertyId::JustifyContent => 0x49,
            PropertyId::JustifyItems => 0x4A,
            PropertyId::JustifySelf => 0x4B,
            PropertyId::Position => 0x50,
            PropertyId::Left => 0x51,
            PropertyId::Top => 0x52,
            PropertyId::Right => 0x53,
            PropertyId::Bottom => 0x54,
            PropertyId::GridTemplateColumns => 0x60,
            PropertyId::GridTemplateRows => 0x61,
            PropertyId::GridTemplateAreas => 0x62,
            PropertyId::GridAutoColumns => 0x63,
            PropertyId::GridAutoRows => 0x64,
            PropertyId::GridAutoFlow => 0x65,
            PropertyId::GridArea => 0x66,
            PropertyId::GridColumn => 0x67,
            PropertyId::GridRow => 0x68,
            PropertyId::GridColumnStart => 0x69,
            PropertyId::GridColumnEnd => 0x6A,
            PropertyId::GridRowStart => 0x6B,
            PropertyId::GridRowEnd => 0x6C,
            PropertyId::GridGap => 0x6D,
            PropertyId::GridColumnGap => 0x6E,
            PropertyId::GridRowGap => 0x6F,
            PropertyId::Padding => 0x70,
            PropertyId::PaddingTop => 0x71,
            PropertyId::PaddingRight => 0x72,
            PropertyId::PaddingBottom => 0x73,
            PropertyId::PaddingLeft => 0x74,
            PropertyId::Margin => 0x75,
            PropertyId::MarginTop => 0x76,
            PropertyId::MarginRight => 0x77,
            PropertyId::MarginBottom => 0x78,
            PropertyId::MarginLeft => 0x79,
            PropertyId::BorderTopWidth => 0x7A,
            PropertyId::BorderRightWidth => 0x7B,
            PropertyId::BorderBottomWidth => 0x7C,
            PropertyId::BorderLeftWidth => 0x7D,
            PropertyId::BorderTopColor => 0x7E,
            PropertyId::BorderRightColor => 0x7F,
            PropertyId::BorderBottomColor => 0x80,
            PropertyId::BorderLeftColor => 0x81,
            PropertyId::BorderTopLeftRadius => 0x82,
            PropertyId::BorderTopRightRadius => 0x83,
            PropertyId::BorderBottomRightRadius => 0x84,
            PropertyId::BorderBottomLeftRadius => 0x85,
            PropertyId::BoxSizing => 0x86,
            PropertyId::Outline => 0x87,
            PropertyId::OutlineColor => 0x88,
            PropertyId::OutlineWidth => 0x89,
            PropertyId::OutlineOffset => 0x8A,
            PropertyId::Overflow => 0x8B,
            PropertyId::OverflowX => 0x8C,
            PropertyId::OverflowY => 0x8D,
            PropertyId::Spans => 0x8E,
            PropertyId::WindowWidthNew => 0xA0,
            PropertyId::WindowHeightNew => 0xA1,
            PropertyId::WindowTitleNew => 0xA2,
            PropertyId::WindowResizableNew => 0xA3,
            PropertyId::WindowFullscreenNew => 0xA4,
            PropertyId::WindowVsyncNew => 0xA5,
            PropertyId::WindowTargetFpsNew => 0xA6,
            PropertyId::WindowAntialiasingNew => 0xA7,
            PropertyId::WindowIconNew => 0xA8,
            PropertyId::TransformScale => 0xB0,
            PropertyId::TransformScaleX => 0xB1,
            PropertyId::TransformScaleY => 0xB2,
            PropertyId::TransformScaleZ => 0xB3,
            PropertyId::TransformTranslateX => 0xB4,
            PropertyId::TransformTranslateY => 0xB5,
            PropertyId::TransformTranslateZ => 0xB6,
            PropertyId::TransformRotate => 0xB7,
            PropertyId::TransformRotateX => 0xB8,
            PropertyId::TransformRotateY => 0xB9,
            PropertyId::TransformRotateZ => 0xBA,
            PropertyId::TransformSkewX => 0xBB,
            PropertyId::TransformSkewY => 0xBC,
            PropertyId::TransformPerspective => 0xBD,
            PropertyId::TransformMatrix => 0xBE,
            PropertyId::SemanticRole => 0xC0,
            PropertyId::HeadingLevel => 0xC1,
            PropertyId::ListType => 0xC2,
            PropertyId::ListItemRole => 0xC3,
            PropertyId::TableSection => 0xC4,
            PropertyId::InteractiveType => 0xC5,
            PropertyId::MediaType => 0xC6,
            PropertyId::EmbedType => 0xC7,
            PropertyId::InputTypeProperty => 0xC8,
            PropertyId::FontStyle => 0xD0,
            PropertyId::TextDecoration => 0xD1,
            PropertyId::VerticalAlign => 0xD2,
            PropertyId::LineHeight => 0xD3,
            PropertyId::LetterSpacing => 0xD4,
            PropertyId::WordSpacing => 0xD5,
            PropertyId::TextIndent => 0xD6,
            PropertyId::TextTransform => 0xD7,
            PropertyId::TextShadow => 0xD8,
            PropertyId::WordWrap => 0xD9,
            PropertyId::TextOverflow => 0xDA,
            PropertyId::WritingMode => 0xDB,
            PropertyId::BackgroundImage => 0xE0,
            PropertyId::BackgroundRepeat => 0xE1,
            PropertyId::BackgroundPosition => 0xE2,
            PropertyId::BackgroundSize => 0xE3,
            PropertyId::BackgroundAttachment => 0xE4,
            PropertyId::BorderStyle => 0xE5,
            PropertyId::BorderImage => 0xE6,
            PropertyId::Filter => 0xE7,
            PropertyId::BackdropFilter => 0xE8,
            PropertyId::ClipPath => 0xE9,
            PropertyId::Mask => 0xEA,
            PropertyId::MixBlendMode => 0xEB,
            PropertyId::ObjectFit => 0xEC,
            PropertyId::ObjectPosition => 0xED,
            PropertyId::RichTextContent => 0xF1,
            PropertyId::AccessibilityLabel => 0xF2,
            PropertyId::AccessibilityRole => 0xF3,
            PropertyId::AccessibilityDescription => 0xF4,
            PropertyId::DataAttributes => 0xF5,
            PropertyId::CustomProperties => 0xF6,
            
            // Legacy compatibility mappings
            PropertyId::CustomData => 0xF7,
            PropertyId::Author => 0xF8,
            PropertyId::Version => 0xF9,
            PropertyId::BoxShadow => 0x97,
            PropertyId::MinViewportWidth => 0xFA,
            PropertyId::MaxViewportWidth => 0xFB,
            PropertyId::Order => 0xFC,
            
            PropertyId::Custom(value) => value,
        }
    }
}

impl From<u8> for PropertyId {
    fn from(value: u8) -> Self {
        Self::from_u8(value)
    }
}

#[cfg(feature = "serde_support")]
impl<'de> serde::Deserialize<'de> for PropertyId {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        use serde::de::Error;
        
        struct PropertyIdVisitor;
        
        impl<'de> serde::de::Visitor<'de> for PropertyIdVisitor {
            type Value = PropertyId;
            
            fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
                formatter.write_str("a property ID string or number")
            }
            
            fn visit_str<E>(self, value: &str) -> Result<PropertyId, E>
            where
                E: serde::de::Error,
            {
                Ok(PropertyId::from_name(value))
            }
            
            fn visit_u8<E>(self, value: u8) -> Result<PropertyId, E>
            where
                E: serde::de::Error,
            {
                Ok(PropertyId::from_u8(value))
            }
            
            fn visit_u64<E>(self, value: u64) -> Result<PropertyId, E>
            where
                E: serde::de::Error,
            {
                if value <= u8::MAX as u64 {
                    Ok(PropertyId::from_u8(value as u8))
                } else {
                    Err(E::custom(format!("property ID value {} is too large", value)))
                }
            }
        }
        
        deserializer.deserialize_any(PropertyIdVisitor)
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
    Textarea = 0x07,
    
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
            "textarea" => Some(Self::Textarea),
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
            Self::Textarea => "textarea",
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