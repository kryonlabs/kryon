// FILE: src/core/properties.rs


// Property IDs
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum PropertyId {
    Invalid = 0x00,
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
    FontFamily = 0x0C,
    ImageSource = 0x0D,
    ListStyleType = 0x1E,
    WhiteSpace = 0x1F,
    Opacity = 0x0E,
    ZIndex = 0x0F,
    Visibility = 0x10,
    Gap = 0x11,
    MinWidth = 0x12,
    MinHeight = 0x13,
    MaxWidth = 0x14,
    MaxHeight = 0x15,
    AspectRatio = 0x16,
    Transform = 0x17,
    Shadow = 0x18,
    Width = 0x19,
    Height = 0x1A,
    CustomData = 0x1D,
    // App-specific properties
    WindowWidth = 0x20,
    WindowHeight = 0x21,
    WindowTitle = 0x22,
    Resizable = 0x23,
    KeepAspect = 0x24,
    ScaleFactor = 0x25,
    Icon = 0x26,
    Version = 0x27,
    Author = 0x28,
    Cursor = 0x29,
    Checked = 0x2A,
    InputType = 0x2B,
    EmbedViewType = 0x2C,
    
    // CSS Grid Properties (0x60-0x6F - matching renderer PropertyRegistry)
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
    
    // Box Model Properties (0x70-0x8F)
    PaddingTop = 0x71,
    PaddingRight = 0x72,
    PaddingBottom = 0x73,
    PaddingLeft = 0x74,
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
    
    // Overflow properties
    Overflow = 0x8B,
    OverflowX = 0x8C,
    OverflowY = 0x8D,
    
    // Typography properties
    LineHeight = 0x8E,
    LetterSpacing = 0x8F,
    TextDecoration = 0x93,
    TextTransform = 0x94,
    TextIndent = 0x95,
    TextOverflow = 0x96,
    FontStyle = 0x97,
    FontVariant = 0x98,
    WordSpacing = 0x99,
    
    // Visual Effects Properties
    BoxShadow = 0x9A,
    TextShadow = 0x9B,
    Filter = 0x9C,
    BackdropFilter = 0x9D,
    
    // Responsive Properties
    MinViewportWidth = 0x9E,
    MaxViewportWidth = 0x9F,
    
    // Rich Text Properties
    Spans = 0xA0,
    
    // Taffy Modern Flexbox Properties (0x40-0x4F)
    Display = 0x40,
    FlexDirection = 0x41,
    FlexWrap = 0x42,
    FlexGrow = 0x43,
    FlexShrink = 0x44,
    FlexBasis = 0x45,
    AlignItems = 0x46,
    AlignSelf = 0x47,
    AlignContent = 0x48,
    JustifyContent = 0x49,
    JustifyItems = 0x4A,
    JustifySelf = 0x4B,
    Order = 0x4C,
    
    // Taffy Positioning Properties (0x50-0x5F)
    Position = 0x50,
    Left = 0x51,
    Top = 0x52,
    Right = 0x53,
    Bottom = 0x54,
    Inset = 0x55,
    
    // Taffy Sizing Properties (0x90-0x9F)
    MinSize = 0x90,
    MaxSize = 0x91,
    PreferredSize = 0x92,
}



impl PropertyId {
    /// Centralized property name to ID mapping - used by all compilation phases
    pub fn from_name(key: &str) -> Self {
        match key {
            "background_color" => PropertyId::BackgroundColor,     // 0x01
            "text_color" | "foreground_color" => PropertyId::ForegroundColor,  // 0x02  
            "border_color" => PropertyId::BorderColor,             // 0x03
            "border_width" => PropertyId::BorderWidth,             // 0x04
            "border_radius" => PropertyId::BorderRadius,           // 0x05
            "padding" => PropertyId::Padding,                      // 0x06
            "margin" => PropertyId::Margin,                        // 0x07
            "text" => PropertyId::TextContent,                     // 0x08
            "font_size" => PropertyId::FontSize,                   // 0x09
            "font_weight" => PropertyId::FontWeight,               // 0x0A
            "text_alignment" => PropertyId::TextAlignment,         // 0x0B
            "font_family" => PropertyId::FontFamily,               // 0x0C
            "src" | "image_source" => PropertyId::ImageSource,     // 0x0D
            "list_style_type" | "list-style-type" => PropertyId::ListStyleType, // 0x1E
            "white_space" | "white-space" => PropertyId::WhiteSpace, // 0x1F
            "opacity" => PropertyId::Opacity,                      // 0x0E
            "z_index" => PropertyId::ZIndex,                       // 0x0F
            "visibility" | "visible" => PropertyId::Visibility,    // 0x10
            "gap" => PropertyId::Gap,                              // 0x11
            "min_width" => PropertyId::MinWidth,                   // 0x12
            "min_height" => PropertyId::MinHeight,                 // 0x13
            "max_width" => PropertyId::MaxWidth,                   // 0x14
            "max_height" => PropertyId::MaxHeight,                 // 0x15
            "aspect_ratio" => PropertyId::AspectRatio,             // 0x16
            "transform" => PropertyId::Transform,                  // 0x17
            "shadow" => PropertyId::Shadow,                        // 0x18
            "box_shadow" => PropertyId::Shadow,                    // 0x18 (alias)
            "overflow" => PropertyId::Overflow,                    // 0x8B
            "overflow-x" => PropertyId::OverflowX,                 // 0x8C
            "overflow-y" => PropertyId::OverflowY,                 // 0x8D
            
            // Typography properties
            "line_height" | "line-height" => PropertyId::LineHeight,           // 0x8E
            "letter_spacing" | "letter-spacing" => PropertyId::LetterSpacing,  // 0x8F
            "text_decoration" | "text-decoration" => PropertyId::TextDecoration, // 0x93
            "text_transform" | "text-transform" => PropertyId::TextTransform,   // 0x94
            "text_indent" | "text-indent" => PropertyId::TextIndent,            // 0x95
            "text_overflow" | "text-overflow" => PropertyId::TextOverflow,      // 0x96
            "font_style" | "font-style" => PropertyId::FontStyle,               // 0x97
            "font_variant" | "font-variant" => PropertyId::FontVariant,         // 0x98
            "word_spacing" | "word-spacing" => PropertyId::WordSpacing,         // 0x99
            
            // Visual Effects properties
            "box_shadow" | "box-shadow" => PropertyId::BoxShadow,               // 0x9A
            "text_shadow" | "text-shadow" => PropertyId::TextShadow,            // 0x9B
            "filter" => PropertyId::Filter,                                     // 0x9C
            "backdrop_filter" | "backdrop-filter" => PropertyId::BackdropFilter, // 0x9D
            
            // Responsive properties
            "min_viewport_width" | "min-viewport-width" => PropertyId::MinViewportWidth, // 0x9E
            "max_viewport_width" | "max-viewport-width" => PropertyId::MaxViewportWidth, // 0x9F
            
            // Rich text properties
            "spans" => PropertyId::Spans, // 0xA0
            "width" => PropertyId::Width,                          // 0x19
            "height" => PropertyId::Height,                        // 0x1A
            "cursor" => PropertyId::Cursor,                        // 0x29
            "checked" => PropertyId::Checked,                      // 0x2A
            "type" => PropertyId::InputType,                       // 0x2B
            
            // App-specific properties (0x20-0x28)
            "window_width" => PropertyId::WindowWidth,             // 0x20
            "window_height" => PropertyId::WindowHeight,           // 0x21
            "window_title" => PropertyId::WindowTitle,             // 0x22
            "resizable" | "window_resizable" => PropertyId::Resizable,                  // 0x23
            "keep_aspect_ratio" => PropertyId::KeepAspect,         // 0x24
            "scale_factor" => PropertyId::ScaleFactor,             // 0x25
            "icon" => PropertyId::Icon,                            // 0x26
            "version" => PropertyId::Version,                      // 0x27
            "author" => PropertyId::Author,                        // 0x28
            
            // CSS Grid Properties (0x60-0x6F - matching renderer PropertyRegistry)
            "grid_template_columns" | "grid-template-columns" => PropertyId::GridTemplateColumns, // 0x60
            "grid_template_rows" | "grid-template-rows" => PropertyId::GridTemplateRows,           // 0x61
            "grid_template_areas" | "grid-template-areas" => PropertyId::GridTemplateAreas,       // 0x62
            "grid_auto_columns" | "grid-auto-columns" => PropertyId::GridAutoColumns,             // 0x63
            "grid_auto_rows" | "grid-auto-rows" => PropertyId::GridAutoRows,                      // 0x64
            "grid_auto_flow" | "grid-auto-flow" => PropertyId::GridAutoFlow,                      // 0x65
            "grid_area" | "grid-area" => PropertyId::GridArea,                                     // 0x66
            "grid_column" | "grid-column" => PropertyId::GridColumn,                               // 0x67
            "grid_row" | "grid-row" => PropertyId::GridRow,                                        // 0x68
            "grid_column_start" | "grid-column-start" => PropertyId::GridColumnStart,             // 0x69
            "grid_column_end" | "grid-column-end" => PropertyId::GridColumnEnd,                   // 0x6A
            "grid_row_start" | "grid-row-start" => PropertyId::GridRowStart,                      // 0x6B
            "grid_row_end" | "grid-row-end" => PropertyId::GridRowEnd,                            // 0x6C
            "grid_gap" | "grid-gap" => PropertyId::GridGap,                                        // 0x6D
            "grid_column_gap" | "grid-column-gap" | "column_gap" => PropertyId::GridColumnGap,    // 0x6E
            "grid_row_gap" | "grid-row-gap" | "row_gap" => PropertyId::GridRowGap,                // 0x6F
            
            // Modern Flexbox Properties (0x40-0x4F)
            "display" => PropertyId::Display,                                                     // 0x40
            "flex_direction" | "flex-direction" => PropertyId::FlexDirection,                     // 0x41
            "flex_wrap" | "flex-wrap" => PropertyId::FlexWrap,                                     // 0x42
            "flex_grow" | "flex-grow" => PropertyId::FlexGrow,                                     // 0x43
            "flex_shrink" | "flex-shrink" => PropertyId::FlexShrink,                              // 0x44
            "flex_basis" | "flex-basis" => PropertyId::FlexBasis,                                 // 0x45
            "align_items" | "align-items" => PropertyId::AlignItems,                              // 0x46
            "align_self" | "align-self" => PropertyId::AlignSelf,                                 // 0x47
            "align_content" | "align-content" => PropertyId::AlignContent,                        // 0x48
            "justify_content" | "justify-content" => PropertyId::JustifyContent,                  // 0x49
            "justify_items" | "justify-items" => PropertyId::JustifyItems,                        // 0x4A
            "justify_self" | "justify-self" => PropertyId::JustifySelf,                           // 0x4B
            "order" => PropertyId::Order,                                                          // 0x4C
            
            // Positioning Properties (0x50-0x5F)
            "position" => PropertyId::Position,                                                    // 0x50
            "left" => PropertyId::Left,                                                            // 0x51
            "top" => PropertyId::Top,                                                              // 0x52
            "right" => PropertyId::Right,                                                          // 0x53
            "bottom" => PropertyId::Bottom,                                                        // 0x54
            "inset" => PropertyId::Inset,                                                          // 0x55
            
            // Box Model Properties (0x70-0x8F)
            "padding_top" | "padding-top" => PropertyId::PaddingTop,                             // 0x71
            "padding_right" | "padding-right" => PropertyId::PaddingRight,                       // 0x72
            "padding_bottom" | "padding-bottom" => PropertyId::PaddingBottom,                    // 0x73
            "padding_left" | "padding-left" => PropertyId::PaddingLeft,                          // 0x74
            "margin_top" | "margin-top" => PropertyId::MarginTop,                                // 0x76
            "margin_right" | "margin-right" => PropertyId::MarginRight,                          // 0x77
            "margin_bottom" | "margin-bottom" => PropertyId::MarginBottom,                       // 0x78
            "margin_left" | "margin-left" => PropertyId::MarginLeft,                             // 0x79
            "border_top_width" | "border-top-width" => PropertyId::BorderTopWidth,               // 0x7A
            "border_right_width" | "border-right-width" => PropertyId::BorderRightWidth,         // 0x7B
            "border_bottom_width" | "border-bottom-width" => PropertyId::BorderBottomWidth,      // 0x7C
            "border_left_width" | "border-left-width" => PropertyId::BorderLeftWidth,            // 0x7D
            "border_top_color" | "border-top-color" => PropertyId::BorderTopColor,               // 0x7E
            "border_right_color" | "border-right-color" => PropertyId::BorderRightColor,         // 0x7F
            "border_bottom_color" | "border-bottom-color" => PropertyId::BorderBottomColor,      // 0x80
            "border_left_color" | "border-left-color" => PropertyId::BorderLeftColor,            // 0x81
            "border_top_left_radius" | "border-top-left-radius" => PropertyId::BorderTopLeftRadius,        // 0x82
            "border_top_right_radius" | "border-top-right-radius" => PropertyId::BorderTopRightRadius,     // 0x83
            "border_bottom_right_radius" | "border-bottom-right-radius" => PropertyId::BorderBottomRightRadius, // 0x84
            "border_bottom_left_radius" | "border-bottom-left-radius" => PropertyId::BorderBottomLeftRadius,     // 0x85
            "box_sizing" | "box-sizing" => PropertyId::BoxSizing,                                // 0x86
            "outline" => PropertyId::Outline,                                                     // 0x87
            "outline_color" | "outline-color" => PropertyId::OutlineColor,                       // 0x88
            "outline_width" | "outline-width" => PropertyId::OutlineWidth,                       // 0x89
            "outline_offset" | "outline-offset" => PropertyId::OutlineOffset,                    // 0x8A
            
            // Sizing Properties (0x90-0x9F)
            "min_size" | "min-size" => PropertyId::MinSize,                                       // 0x90
            "max_size" | "max-size" => PropertyId::MaxSize,                                       // 0x91
            "preferred_size" | "preferred-size" => PropertyId::PreferredSize,                     // 0x92
            
            _ => PropertyId::CustomData,
        }
    }
    
    /// Check if this property should be handled ONLY in the element header and never as a style property
    /// These are truly element-specific properties that don't make sense as inheritable styles
    pub fn is_element_header_property(key: &str) -> bool {
        matches!(key, "id" | "checked")
    }
}
