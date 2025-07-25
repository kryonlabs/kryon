//! Property Registry - Single source of truth for all property IDs and enums

#[cfg(feature = "serde_support")]
use serde::{Deserialize, Serialize};

/// All property IDs used in the Kryon binary format
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, serde::Serialize, serde::Deserialize)]
#[repr(u8)]
pub enum PropertyId {
    // Style properties (0x01-0x0F)
    BackgroundColor = 0x01,
    ForegroundColor = 0x02,
    BorderColor = 0x03,
    BorderWidth = 0x04,
    BorderRadius = 0x05,
    Padding = 0x06,
    Margin = 0x07,
    TextContent = 0x08,
    FontSize = 0x09,
    FontWeight = 0x0A,
    TextAlignment = 0x0B,
    TextDecoration = 0x0C,
    LineHeight = 0x0D,
    LetterSpacing = 0x0E,
    
    // Layout properties (0x10-0x2F)
    Width = 0x19,
    Height = 0x1A,
    MinWidth = 0x1B,
    MaxWidth = 0x1C,
    MinHeight = 0x1D,
    MaxHeight = 0x1E,
    
    // Window properties (0x20-0x2F)
    WindowWidth = 0x20,
    WindowHeight = 0x21,
    WindowTitle = 0x22,
    Resizable = 0x23,
    KeepAspect = 0x24,
    ScaleFactor = 0x25,
    Icon = 0x26,
    Version = 0x27,
    Author = 0x28,
    
    // Flexbox properties (0x40-0x4F)
    Display = 0x40,
    FlexDirection = 0x41,
    FlexWrap = 0x42,
    FlexGrow = 0x43,
    FlexShrink = 0x44,
    FlexBasis = 0x45,
    Flex = 0x46,
    AlignItems = 0x47,
    AlignSelf = 0x48,
    AlignContent = 0x49,
    JustifyContent = 0x4A,
    JustifyItems = 0x4B,
    JustifySelf = 0x4C,
    Order = 0x4D,
    Gap = 0x4E,
    RowGap = 0x4F,
    
    // Positioning properties (0x50-0x5F)
    Position = 0x50,
    Top = 0x51,
    Right = 0x52,
    Bottom = 0x53,
    Left = 0x54,
    ZIndex = 0x55,
    Transform = 0x56,
    TransformOrigin = 0x57,
    Overflow = 0x58,
    OverflowX = 0x59,
    OverflowY = 0x5C,
    PosX = 0x5A,
    PosY = 0x5B,
    
    // CSS Grid Properties (0xC0-0xCF)  
    GridTemplateColumns = 0xC0,
    GridTemplateRows = 0xC1,
    GridTemplateAreas = 0xC2,
    GridAutoColumns = 0xC3,
    GridAutoRows = 0xC4,
    GridAutoFlow = 0xC5,
    GridArea = 0xC6,
    GridColumn = 0xC7,
    GridRow = 0xC8,
    GridColumnStart = 0xC9,
    GridColumnEnd = 0xCA,
    GridRowStart = 0xCB,
    GridRowEnd = 0xCC,
    GridGap = 0xCD,
    GridColumnGap = 0xCE,
    GridRowGap = 0xCF,
    
    // Animation properties (0x70-0x7F)
    Transition = 0x70,
    Animation = 0x71,
    AnimationDelay = 0x72,
    AnimationDuration = 0x73,
    AnimationFillMode = 0x74,
    AnimationIterationCount = 0x75,
    AnimationPlayState = 0x76,
    AnimationTimingFunction = 0x77,
    
    // Box Model Properties (0x78-0x8F)
    PaddingTop = 0x78,
    PaddingRight = 0x79,
    PaddingBottom = 0x7A,
    PaddingLeft = 0x7B,
    MarginTop = 0x7C,
    MarginRight = 0x7D,
    MarginBottom = 0x7E,
    MarginLeft = 0x7F,
    BorderTopWidth = 0x80,
    BorderRightWidth = 0x81,
    BorderBottomWidth = 0x82,
    BorderLeftWidth = 0x83,
    BorderTopColor = 0x84,
    BorderRightColor = 0x85,
    BorderBottomColor = 0x86,
    BorderLeftColor = 0x87,
    BorderTopLeftRadius = 0x88,
    BorderTopRightRadius = 0x89,
    BorderBottomRightRadius = 0x8A,
    BorderBottomLeftRadius = 0x8B,
    BoxSizing = 0x8C,
    Outline = 0x8D,
    OutlineColor = 0x8E,
    OutlineWidth = 0x8F,
    
    // Typography extensions (0x90-0x9F)
    OutlineOffset = 0x90,
    TextTransform = 0x91,
    TextIndent = 0x92,
    TextOverflow = 0x93,
    FontStyle = 0x94,
    FontVariant = 0x95,
    WordSpacing = 0x96,
    BoxShadow = 0x97,
    TextShadow = 0x98,
    Filter = 0x99,
    
    // Additional compiler properties (0xA0-0xAF)
    Invalid = 0x00,
    FontFamily = 0x0F,        // Free slot after original LetterSpacing
    ImageSource = 0x15,       // Free slot
    Opacity = 0x16,           // Free slot 
    Visibility = 0x17,        // Free slot
    CustomData = 0x18,        // Free slot
    ListStyleType = 0xA0,     
    WhiteSpace = 0xA1,        
    Checked = 0xA2,           
    InputType = 0xA3,         
    EmbedViewType = 0xA4,
    Cursor = 0xA5,
    PointerEvents = 0xA6,
    UserSelect = 0xA7,
    BackdropFilter = 0xA8,
    MinViewportWidth = 0xA9,
    MaxViewportWidth = 0xAA,
    Spans = 0xAB,
    AspectRatio = 0xAC,
    TransformAlt = 0xAD,     // Alternative transform property
    Shadow = 0xAE,
    
    // Flex extensions (0xB0-0xBF)
    FlexWrapAlt = 0xB0,      // Alternative flex-wrap property
    FlexBasisAlt = 0xB1,     // Alternative flex-basis property  
    FlexAlt = 0xB2,          // Alternative flex property
    AlignSelfAlt = 0xB3,     // Alternative align-self property
    AlignContentAlt = 0xB4,  // Alternative align-content property
    JustifyItemsAlt = 0xB5,  // Alternative justify-items property
    JustifySelfAlt = 0xB6,   // Alternative justify-self property
    OrderAlt = 0xB7,         // Alternative order property
    Inset = 0xB8,
    MinSize = 0xB9,
    MaxSize = 0xBA,
    PreferredSize = 0xBB,
}

impl PropertyId {
    /// Get the property ID as a u8 value
    pub fn as_u8(self) -> u8 {
        self as u8
    }
    
    /// Create PropertyId from u8 value
    pub fn from_u8(value: u8) -> Option<Self> {
        match value {
            0x01 => Some(PropertyId::BackgroundColor),
            0x02 => Some(PropertyId::ForegroundColor),
            0x03 => Some(PropertyId::BorderColor),
            0x04 => Some(PropertyId::BorderWidth),
            0x05 => Some(PropertyId::BorderRadius),
            0x06 => Some(PropertyId::Padding),
            0x07 => Some(PropertyId::Margin),
            0x08 => Some(PropertyId::TextContent),
            0x09 => Some(PropertyId::FontSize),
            0x0A => Some(PropertyId::FontWeight),
            0x0B => Some(PropertyId::TextAlignment),
            0x0C => Some(PropertyId::TextDecoration),
            0x0D => Some(PropertyId::LineHeight),
            0x0E => Some(PropertyId::LetterSpacing),
            0x19 => Some(PropertyId::Width),
            0x1A => Some(PropertyId::Height),
            0x1B => Some(PropertyId::MinWidth),
            0x1C => Some(PropertyId::MaxWidth),
            0x1D => Some(PropertyId::MinHeight),
            0x1E => Some(PropertyId::MaxHeight),
            0x20 => Some(PropertyId::WindowWidth),
            0x21 => Some(PropertyId::WindowHeight),
            0x22 => Some(PropertyId::WindowTitle),
            0x23 => Some(PropertyId::Resizable),
            0x24 => Some(PropertyId::KeepAspect),
            0x25 => Some(PropertyId::ScaleFactor),
            0x26 => Some(PropertyId::Icon),
            0x27 => Some(PropertyId::Version),
            0x28 => Some(PropertyId::Author),
            0x40 => Some(PropertyId::Display),
            0x41 => Some(PropertyId::FlexDirection),
            0x42 => Some(PropertyId::FlexWrap),
            0x43 => Some(PropertyId::FlexGrow),
            0x44 => Some(PropertyId::FlexShrink),
            0x45 => Some(PropertyId::FlexBasis),
            0x46 => Some(PropertyId::Flex),
            0x47 => Some(PropertyId::AlignItems),
            0x48 => Some(PropertyId::AlignSelf),
            0x49 => Some(PropertyId::AlignContent),
            0x4A => Some(PropertyId::JustifyContent),
            0x4B => Some(PropertyId::JustifyItems),
            0x4C => Some(PropertyId::JustifySelf),
            0x4D => Some(PropertyId::Order),
            0x4E => Some(PropertyId::Gap),
            0x4F => Some(PropertyId::RowGap),
            0x50 => Some(PropertyId::Position),
            0x51 => Some(PropertyId::Top),
            0x52 => Some(PropertyId::Right),
            0x53 => Some(PropertyId::Bottom),
            0x54 => Some(PropertyId::Left),
            0x55 => Some(PropertyId::ZIndex),
            0x56 => Some(PropertyId::Transform),
            0x57 => Some(PropertyId::TransformOrigin),
            0x58 => Some(PropertyId::Overflow),
            0x59 => Some(PropertyId::OverflowX),
            0x5C => Some(PropertyId::OverflowY),
            0x5A => Some(PropertyId::PosX),
            0x5B => Some(PropertyId::PosY),
            0x60 => Some(PropertyId::Cursor),
            0x61 => Some(PropertyId::PointerEvents),
            0x62 => Some(PropertyId::UserSelect),
            0x70 => Some(PropertyId::Transition),
            0x71 => Some(PropertyId::Animation),
            0x72 => Some(PropertyId::AnimationDelay),
            0x73 => Some(PropertyId::AnimationDuration),
            0x74 => Some(PropertyId::AnimationFillMode),
            0x75 => Some(PropertyId::AnimationIterationCount),
            0x76 => Some(PropertyId::AnimationPlayState),
            0x77 => Some(PropertyId::AnimationTimingFunction),
            0x00 => Some(PropertyId::Invalid),
            0x0F => Some(PropertyId::FontFamily),
            0x15 => Some(PropertyId::ImageSource),
            0x16 => Some(PropertyId::Opacity),
            0x17 => Some(PropertyId::Visibility),
            0x18 => Some(PropertyId::CustomData),
            
            // Grid properties
            0xC0 => Some(PropertyId::GridTemplateColumns),
            0xC1 => Some(PropertyId::GridTemplateRows),
            0xC2 => Some(PropertyId::GridTemplateAreas),
            0xC3 => Some(PropertyId::GridAutoColumns),
            0xC4 => Some(PropertyId::GridAutoRows),
            0xC5 => Some(PropertyId::GridAutoFlow),
            0xC6 => Some(PropertyId::GridArea),
            0xC7 => Some(PropertyId::GridColumn),
            0xC8 => Some(PropertyId::GridRow),
            0xC9 => Some(PropertyId::GridColumnStart),
            0xCA => Some(PropertyId::GridColumnEnd),
            0xCB => Some(PropertyId::GridRowStart),
            0xCC => Some(PropertyId::GridRowEnd),
            0xCD => Some(PropertyId::GridGap),
            0xCE => Some(PropertyId::GridColumnGap),
            0xCF => Some(PropertyId::GridRowGap),
            
            // Additional properties
            0xA0 => Some(PropertyId::ListStyleType),
            0xA1 => Some(PropertyId::WhiteSpace),
            0xA2 => Some(PropertyId::Checked),
            0xA3 => Some(PropertyId::InputType),
            0xA4 => Some(PropertyId::EmbedViewType),
            0xA5 => Some(PropertyId::Cursor),
            0xA6 => Some(PropertyId::PointerEvents),
            0xA7 => Some(PropertyId::UserSelect),
            0xA8 => Some(PropertyId::BackdropFilter),
            0xA9 => Some(PropertyId::MinViewportWidth),
            0xAA => Some(PropertyId::MaxViewportWidth),
            0xAB => Some(PropertyId::Spans),
            0xAC => Some(PropertyId::AspectRatio),
            0xAD => Some(PropertyId::TransformAlt),
            0xAE => Some(PropertyId::Shadow),
            
            // Box model properties
            0x78 => Some(PropertyId::PaddingTop),
            0x79 => Some(PropertyId::PaddingRight),
            0x7A => Some(PropertyId::PaddingBottom),
            0x7B => Some(PropertyId::PaddingLeft),
            0x7C => Some(PropertyId::MarginTop),
            0x7D => Some(PropertyId::MarginRight),
            0x7E => Some(PropertyId::MarginBottom),
            0x7F => Some(PropertyId::MarginLeft),
            0x80 => Some(PropertyId::BorderTopWidth),
            0x81 => Some(PropertyId::BorderRightWidth),
            0x82 => Some(PropertyId::BorderBottomWidth),
            0x83 => Some(PropertyId::BorderLeftWidth),
            0x84 => Some(PropertyId::BorderTopColor),
            0x85 => Some(PropertyId::BorderRightColor),
            0x86 => Some(PropertyId::BorderBottomColor),
            0x87 => Some(PropertyId::BorderLeftColor),
            0x88 => Some(PropertyId::BorderTopLeftRadius),
            0x89 => Some(PropertyId::BorderTopRightRadius),
            0x8A => Some(PropertyId::BorderBottomRightRadius),
            0x8B => Some(PropertyId::BorderBottomLeftRadius),
            0x8C => Some(PropertyId::BoxSizing),
            0x8D => Some(PropertyId::Outline),
            0x8E => Some(PropertyId::OutlineColor),
            0x8F => Some(PropertyId::OutlineWidth),
            
            // Typography extensions
            0x90 => Some(PropertyId::OutlineOffset),
            0x91 => Some(PropertyId::TextTransform),
            0x92 => Some(PropertyId::TextIndent),
            0x93 => Some(PropertyId::TextOverflow),
            0x94 => Some(PropertyId::FontStyle),
            0x95 => Some(PropertyId::FontVariant),
            0x96 => Some(PropertyId::WordSpacing),
            0x97 => Some(PropertyId::BoxShadow),
            0x98 => Some(PropertyId::TextShadow),
            0x99 => Some(PropertyId::Filter),
            
            // Flex extensions
            0xB0 => Some(PropertyId::FlexWrapAlt),
            0xB1 => Some(PropertyId::FlexBasisAlt),
            0xB2 => Some(PropertyId::FlexAlt),
            0xB3 => Some(PropertyId::AlignSelfAlt),
            0xB4 => Some(PropertyId::AlignContentAlt),
            0xB5 => Some(PropertyId::JustifyItemsAlt),
            0xB6 => Some(PropertyId::JustifySelfAlt),
            0xB7 => Some(PropertyId::OrderAlt),
            0xB8 => Some(PropertyId::Inset),
            0xB9 => Some(PropertyId::MinSize),
            0xBA => Some(PropertyId::MaxSize),
            0xBB => Some(PropertyId::PreferredSize),
            _ => None,
        }
    }
    
    /// Get human-readable name for debugging
    pub fn name(self) -> &'static str {
        match self {
            PropertyId::BackgroundColor => "BackgroundColor",
            PropertyId::ForegroundColor => "ForegroundColor",
            PropertyId::BorderColor => "BorderColor",
            PropertyId::BorderWidth => "BorderWidth",
            PropertyId::BorderRadius => "BorderRadius",
            PropertyId::Padding => "Padding",
            PropertyId::Margin => "Margin",
            PropertyId::TextContent => "TextContent",
            PropertyId::FontSize => "FontSize",
            PropertyId::FontWeight => "FontWeight",
            PropertyId::TextAlignment => "TextAlignment",
            PropertyId::TextDecoration => "TextDecoration",
            PropertyId::LineHeight => "LineHeight",
            PropertyId::LetterSpacing => "LetterSpacing",
            PropertyId::Width => "Width",
            PropertyId::Height => "Height",
            PropertyId::MinWidth => "MinWidth",
            PropertyId::MaxWidth => "MaxWidth",
            PropertyId::MinHeight => "MinHeight",
            PropertyId::MaxHeight => "MaxHeight",
            PropertyId::WindowWidth => "WindowWidth",
            PropertyId::WindowHeight => "WindowHeight",
            PropertyId::WindowTitle => "WindowTitle",
            PropertyId::Resizable => "Resizable",
            PropertyId::KeepAspect => "KeepAspect",
            PropertyId::ScaleFactor => "ScaleFactor",
            PropertyId::Icon => "Icon",
            PropertyId::Version => "Version",
            PropertyId::Author => "Author",
            PropertyId::Display => "Display",
            PropertyId::FlexDirection => "FlexDirection",
            PropertyId::FlexWrap => "FlexWrap",
            PropertyId::FlexGrow => "FlexGrow",
            PropertyId::FlexShrink => "FlexShrink",
            PropertyId::FlexBasis => "FlexBasis",
            PropertyId::Order => "Order",
            PropertyId::AlignItems => "AlignItems",
            PropertyId::AlignSelf => "AlignSelf",
            PropertyId::AlignContent => "AlignContent",
            PropertyId::JustifyContent => "JustifyContent",
            PropertyId::JustifyItems => "JustifyItems",
            PropertyId::JustifySelf => "JustifySelf",
            PropertyId::Gap => "Gap",
            PropertyId::RowGap => "RowGap",
            PropertyId::Position => "Position",
            PropertyId::Top => "Top",
            PropertyId::Right => "Right",
            PropertyId::Bottom => "Bottom",
            PropertyId::Left => "Left",
            PropertyId::ZIndex => "ZIndex",
            PropertyId::Transform => "Transform",
            PropertyId::TransformOrigin => "TransformOrigin",
            PropertyId::Overflow => "Overflow",
            PropertyId::OverflowX => "OverflowX",
            PropertyId::PosX => "PosX",
            PropertyId::PosY => "PosY",
            PropertyId::Cursor => "Cursor",
            PropertyId::PointerEvents => "PointerEvents",
            PropertyId::UserSelect => "UserSelect",
            PropertyId::Transition => "Transition",
            PropertyId::Animation => "Animation",
            PropertyId::AnimationDelay => "AnimationDelay",
            PropertyId::AnimationDuration => "AnimationDuration",
            PropertyId::AnimationFillMode => "AnimationFillMode",
            PropertyId::AnimationIterationCount => "AnimationIterationCount",
            PropertyId::AnimationPlayState => "AnimationPlayState",
            PropertyId::AnimationTimingFunction => "AnimationTimingFunction",
            PropertyId::Invalid => "Invalid",
            PropertyId::FontFamily => "FontFamily",
            PropertyId::ImageSource => "ImageSource",
            PropertyId::Opacity => "Opacity",
            PropertyId::Visibility => "Visibility",
            PropertyId::CustomData => "CustomData",
            PropertyId::ListStyleType => "ListStyleType",
            PropertyId::WhiteSpace => "WhiteSpace",
            PropertyId::Checked => "Checked",
            PropertyId::InputType => "InputType",
            PropertyId::EmbedViewType => "EmbedViewType",
            _ => "UnknownProperty",
        }
    }
    
    /// Centralized property name to ID mapping - used by all compilation phases
    pub fn from_name(key: &str) -> Self {
        match key {
            "background_color" => PropertyId::BackgroundColor,
            "text_color" | "foreground_color" | "color" => PropertyId::ForegroundColor,
            "border_color" => PropertyId::BorderColor,
            "border_width" | "border" => PropertyId::BorderWidth,
            "border_radius" => PropertyId::BorderRadius,
            "padding" => PropertyId::Padding,
            "margin" => PropertyId::Margin,
            "text" => PropertyId::TextContent,
            "font_size" => PropertyId::FontSize,
            "font_weight" => PropertyId::FontWeight,
            "text_alignment" => PropertyId::TextAlignment,
            "font_family" => PropertyId::FontFamily,
            "src" | "image_source" => PropertyId::ImageSource,
            "opacity" => PropertyId::Opacity,
            "z_index" => PropertyId::ZIndex,
            "visibility" | "visible" => PropertyId::Visibility,
            "list_style_type" | "list-style-type" => PropertyId::ListStyleType,
            "white_space" | "white-space" => PropertyId::WhiteSpace,
            "gap" => PropertyId::Gap,
            "min_width" => PropertyId::MinWidth,
            "min_height" => PropertyId::MinHeight,
            "max_width" => PropertyId::MaxWidth,
            "max_height" => PropertyId::MaxHeight,
            "width" => PropertyId::Width,
            "height" => PropertyId::Height,
            "pos_x" | "x" | "position_x" => PropertyId::PosX,
            "pos_y" | "y" | "position_y" => PropertyId::PosY,
            "cursor" => PropertyId::Cursor,
            "checked" => PropertyId::Checked,
            "type" => PropertyId::InputType,
            "window_width" => PropertyId::WindowWidth,
            "window_height" => PropertyId::WindowHeight,
            "window_title" => PropertyId::WindowTitle,
            "resizable" | "window_resizable" => PropertyId::Resizable,
            "keep_aspect_ratio" => PropertyId::KeepAspect,
            "scale_factor" => PropertyId::ScaleFactor,
            "icon" => PropertyId::Icon,
            "version" => PropertyId::Version,
            "author" => PropertyId::Author,
            "display" => PropertyId::Display,
            "flex_direction" | "flex-direction" => PropertyId::FlexDirection,
            "align_items" | "align-items" => PropertyId::AlignItems,
            "justify_content" | "justify-content" => PropertyId::JustifyContent,
            "position" => PropertyId::Position,
            "left" => PropertyId::Left,
            "top" => PropertyId::Top,
            "right" => PropertyId::Right,
            "bottom" => PropertyId::Bottom,
            _ => PropertyId::CustomData,
        }
    }
    
    /// Check if this property should be handled ONLY in the element header and never as a style property
    pub fn is_element_header_property(key: &str) -> bool {
        matches!(key, "id" | "checked")
    }
}

// Custom serialization for PropertyId to use numeric values instead of variant names
#[cfg(feature = "serde_support")]
impl Serialize for PropertyId {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.serialize_u8(*self as u8)
    }
}

#[cfg(feature = "serde_support")]
impl<'de> Deserialize<'de> for PropertyId {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let value = u8::deserialize(deserializer)?;
        PropertyId::from_u8(value).ok_or_else(|| {
            serde::de::Error::custom(format!("Invalid PropertyId value: 0x{:02X}", value))
        })
    }
}

/// Text alignment values - CRITICAL: These must match exactly between compiler and renderer
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[cfg_attr(feature = "serde_support", derive(Serialize, Deserialize))]
#[repr(u8)]
pub enum TextAlignment {
    Start = 0,
    Center = 1,
    End = 2,
    Justify = 3,
}

/// Flexbox justify-content values
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[cfg_attr(feature = "serde_support", derive(Serialize, Deserialize))]
#[repr(u8)]
pub enum JustifyContent {
    FlexStart = 0,
    FlexEnd = 1,
    Center = 2,
    SpaceBetween = 3,
    SpaceAround = 4,
    SpaceEvenly = 5,
}

/// Flexbox align-items values
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[cfg_attr(feature = "serde_support", derive(Serialize, Deserialize))]
#[repr(u8)]
pub enum AlignItems {
    FlexStart = 0,
    FlexEnd = 1,
    Center = 2,
    Stretch = 3,
    Baseline = 4,
}

/// Flexbox flex-direction values
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[cfg_attr(feature = "serde_support", derive(Serialize, Deserialize))]
#[repr(u8)]
pub enum FlexDirection {
    Row = 0,
    Column = 1,
    RowReverse = 2,
    ColumnReverse = 3,
}

/// Display values
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[cfg_attr(feature = "serde_support", derive(Serialize, Deserialize))]
#[repr(u8)]
pub enum Display {
    None = 0,
    Block = 1,
    Flex = 2,
    Grid = 3,
    Inline = 4,
    InlineBlock = 5,
}

/// Position values
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[cfg_attr(feature = "serde_support", derive(Serialize, Deserialize))]
#[repr(u8)]
pub enum Position {
    Static = 0,
    Relative = 1,
    Absolute = 2,
    Fixed = 3,
    Sticky = 4,
}

/// Property registry for managing property inheritance rules
#[derive(Debug, Clone)]
pub struct PropertyRegistry;

impl PropertyRegistry {
    /// Create a new property registry
    pub fn new() -> Self {
        Self
    }
    
    /// Check if a property should be inherited from parent to child
    pub fn is_inheritable(&self, property: PropertyId) -> bool {
        match property {
            // Inheritable text properties
            PropertyId::ForegroundColor
            | PropertyId::FontSize
            | PropertyId::FontWeight
            | PropertyId::TextAlignment
            | PropertyId::FontFamily
            | PropertyId::LineHeight
            | PropertyId::LetterSpacing
            | PropertyId::TextDecoration
            // Inheritable display properties
            | PropertyId::Opacity
            | PropertyId::Visibility
            | PropertyId::Cursor => true,
            
            // Non-inheritable properties
            _ => false,
        }
    }
}

impl Default for PropertyRegistry {
    fn default() -> Self {
        Self::new()
    }
}