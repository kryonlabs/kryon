use kryon_core::{Alignment, AlignmentValue, ComponentType, IRComponent};

/// Builder trait for components
pub trait ComponentBuilder: Sized {
    /// Build the component into an IRComponent
    fn build(self, id: u32) -> IRComponent;

    /// Set width
    fn width(self, width: impl Into<String>) -> Self;

    /// Set height
    fn height(self, height: impl Into<String>) -> Self;

    /// Set background color
    fn background(self, color: impl Into<String>) -> Self;

    /// Set text color
    fn color(self, color: impl Into<String>) -> Self;

    /// Set padding
    fn padding(self, padding: f32) -> Self;

    /// Set margin
    fn margin(self, margin: f32) -> Self;

    /// Set gap (spacing between children)
    fn gap(self, gap: f32) -> Self;

    /// Set justify content alignment
    fn justify_content(self, align: Alignment) -> Self;

    /// Set align items alignment
    fn align_items(self, align: Alignment) -> Self;

    /// Set flex grow factor
    fn flex_grow(self, grow: u8) -> Self;

    /// Set flex shrink factor
    fn flex_shrink(self, shrink: u8) -> Self;

    /// Set opacity
    fn opacity(self, opacity: f32) -> Self;
}

/// Helper macro to implement common builder methods
macro_rules! impl_builder_methods {
    ($builder:ty) => {
        impl $builder {
            pub fn width(mut self, width: impl Into<String>) -> Self {
                self.width = Some(width.into());
                self
            }

            pub fn height(mut self, height: impl Into<String>) -> Self {
                self.height = Some(height.into());
                self
            }

            pub fn background(mut self, color: impl Into<String>) -> Self {
                self.background = Some(color.into());
                self
            }

            pub fn color(mut self, color: impl Into<String>) -> Self {
                self.color = Some(color.into());
                self
            }

            pub fn padding(mut self, padding: f32) -> Self {
                self.padding = Some(padding);
                self
            }

            pub fn margin(mut self, margin: f32) -> Self {
                self.margin = Some(margin);
                self
            }

            pub fn gap(mut self, gap: f32) -> Self {
                self.gap = Some(gap);
                self
            }

            pub fn justify_content(mut self, align: Alignment) -> Self {
                self.justify_content = Some(AlignmentValue::Constant(align));
                self
            }

            pub fn align_items(mut self, align: Alignment) -> Self {
                self.align_items = Some(AlignmentValue::Constant(align));
                self
            }

            pub fn flex_grow(mut self, grow: u8) -> Self {
                self.flex_grow = Some(grow);
                self
            }

            pub fn flex_shrink(mut self, shrink: u8) -> Self {
                self.flex_shrink = Some(shrink);
                self
            }

            pub fn opacity(mut self, opacity: f32) -> Self {
                self.opacity = Some(opacity);
                self
            }
        }
    };
}

/// Builder for Container components
#[derive(Debug, Default)]
pub struct ContainerBuilder {
    width: Option<String>,
    height: Option<String>,
    padding: Option<f32>,
    margin: Option<f32>,
    gap: Option<f32>,
    background: Option<String>,
    color: Option<String>,
    justify_content: Option<AlignmentValue>,
    align_items: Option<AlignmentValue>,
    flex_grow: Option<u8>,
    flex_shrink: Option<u8>,
    opacity: Option<f32>,
    children: Vec<IRComponent>,
}

impl_builder_methods!(ContainerBuilder);

impl ContainerBuilder {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn child(mut self, child: IRComponent) -> Self {
        self.children.push(child);
        self
    }

    pub fn children(mut self, children: Vec<IRComponent>) -> Self {
        self.children.extend(children);
        self
    }

    pub fn build(self, id: u32) -> IRComponent {
        IRComponent {
            id,
            component_type: ComponentType::Container,
            width: self.width,
            height: self.height,
            padding: self.padding,
            margin: self.margin,
            gap: self.gap,
            background: self.background,
            color: self.color,
            justify_content: self.justify_content,
            align_items: self.align_items,
            flex_grow: self.flex_grow,
            flex_shrink: self.flex_shrink,
            opacity: self.opacity,
            children: self.children,
            ..Default::default()
        }
    }
}

/// Builder for Row components
#[derive(Debug, Default)]
pub struct RowBuilder {
    width: Option<String>,
    height: Option<String>,
    padding: Option<f32>,
    margin: Option<f32>,
    gap: Option<f32>,
    background: Option<String>,
    color: Option<String>,
    justify_content: Option<AlignmentValue>,
    align_items: Option<AlignmentValue>,
    flex_grow: Option<u8>,
    flex_shrink: Option<u8>,
    opacity: Option<f32>,
    children: Vec<IRComponent>,
}

impl_builder_methods!(RowBuilder);

impl RowBuilder {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn child(mut self, child: IRComponent) -> Self {
        self.children.push(child);
        self
    }

    pub fn children(mut self, children: Vec<IRComponent>) -> Self {
        self.children.extend(children);
        self
    }

    pub fn build(self, id: u32) -> IRComponent {
        IRComponent {
            id,
            component_type: ComponentType::Row,
            width: self.width,
            height: self.height,
            padding: self.padding,
            margin: self.margin,
            gap: self.gap,
            background: self.background,
            color: self.color,
            justify_content: self.justify_content,
            align_items: self.align_items,
            flex_grow: self.flex_grow,
            flex_shrink: self.flex_shrink,
            opacity: self.opacity,
            children: self.children,
            ..Default::default()
        }
    }
}

/// Builder for Column components
#[derive(Debug, Default)]
pub struct ColumnBuilder {
    width: Option<String>,
    height: Option<String>,
    padding: Option<f32>,
    margin: Option<f32>,
    gap: Option<f32>,
    background: Option<String>,
    color: Option<String>,
    justify_content: Option<AlignmentValue>,
    align_items: Option<AlignmentValue>,
    flex_grow: Option<u8>,
    flex_shrink: Option<u8>,
    opacity: Option<f32>,
    children: Vec<IRComponent>,
}

impl_builder_methods!(ColumnBuilder);

impl ColumnBuilder {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn child(mut self, child: IRComponent) -> Self {
        self.children.push(child);
        self
    }

    pub fn children(mut self, children: Vec<IRComponent>) -> Self {
        self.children.extend(children);
        self
    }

    pub fn build(self, id: u32) -> IRComponent {
        IRComponent {
            id,
            component_type: ComponentType::Column,
            width: self.width,
            height: self.height,
            padding: self.padding,
            margin: self.margin,
            gap: self.gap,
            background: self.background,
            color: self.color,
            justify_content: self.justify_content,
            align_items: self.align_items,
            flex_grow: self.flex_grow,
            flex_shrink: self.flex_shrink,
            opacity: self.opacity,
            children: self.children,
            ..Default::default()
        }
    }
}

/// Builder for Text components
#[derive(Debug, Default)]
pub struct TextBuilder {
    content: String,
    font_size: Option<f32>,
    color: Option<String>,
    opacity: Option<f32>,
}

impl TextBuilder {
    pub fn new(content: impl Into<String>) -> Self {
        Self {
            content: content.into(),
            ..Default::default()
        }
    }

    pub fn font_size(mut self, size: f32) -> Self {
        self.font_size = Some(size);
        self
    }

    pub fn color(mut self, color: impl Into<String>) -> Self {
        self.color = Some(color.into());
        self
    }

    pub fn opacity(mut self, opacity: f32) -> Self {
        self.opacity = Some(opacity);
        self
    }

    pub fn build(self, id: u32) -> IRComponent {
        IRComponent {
            id,
            component_type: ComponentType::Text,
            content: Some(self.content),
            font_size: self.font_size,
            color: self.color,
            opacity: self.opacity,
            ..Default::default()
        }
    }
}

/// Builder for Button components
#[derive(Debug, Default)]
pub struct ButtonBuilder {
    text: String,
    width: Option<String>,
    height: Option<String>,
    padding: Option<f32>,
    background: Option<String>,
    color: Option<String>,
    opacity: Option<f32>,
}

impl ButtonBuilder {
    pub fn width(mut self, width: impl Into<String>) -> Self {
        self.width = Some(width.into());
        self
    }

    pub fn height(mut self, height: impl Into<String>) -> Self {
        self.height = Some(height.into());
        self
    }

    pub fn padding(mut self, padding: f32) -> Self {
        self.padding = Some(padding);
        self
    }

    pub fn background(mut self, color: impl Into<String>) -> Self {
        self.background = Some(color.into());
        self
    }

    pub fn color(mut self, color: impl Into<String>) -> Self {
        self.color = Some(color.into());
        self
    }

    pub fn opacity(mut self, opacity: f32) -> Self {
        self.opacity = Some(opacity);
        self
    }
    pub fn new(text: impl Into<String>) -> Self {
        Self {
            text: text.into(),
            ..Default::default()
        }
    }

    pub fn build(self, id: u32) -> IRComponent {
        IRComponent {
            id,
            component_type: ComponentType::Button,
            text: Some(self.text),
            width: self.width,
            height: self.height,
            padding: self.padding,
            background: self.background,
            color: self.color,
            opacity: self.opacity,
            ..Default::default()
        }
    }
}

// Convenience constructors
pub fn container() -> ContainerBuilder {
    ContainerBuilder::new()
}

pub fn row() -> RowBuilder {
    RowBuilder::new()
}

pub fn column() -> ColumnBuilder {
    ColumnBuilder::new()
}

pub fn text(content: impl Into<String>) -> TextBuilder {
    TextBuilder::new(content)
}

pub fn button(text: impl Into<String>) -> ButtonBuilder {
    ButtonBuilder::new(text)
}
