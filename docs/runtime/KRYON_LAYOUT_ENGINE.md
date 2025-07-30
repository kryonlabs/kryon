# Kryon Layout Engine & Taffy Integration

Complete documentation of Kryon's layout system, built on the Taffy flexbox engine with performance optimizations and caching.

> **🔗 Related Documentation**: 
> - [KRYON_RUNTIME_SYSTEM.md](KRYON_RUNTIME_SYSTEM.md) - Runtime system architecture
> - [KRYON_REFERENCE.md](KRYON_REFERENCE.md) - Element properties and inheritance

## Table of Contents
- [Layout Engine Overview](#layout-engine-overview)
- [Taffy Integration](#taffy-integration)
- [Performance Optimizations](#performance-optimizations)
- [Layout Configuration](#layout-configuration)
- [Flexbox Support](#flexbox-support)
- [Position Systems](#position-systems)
- [Layout Caching](#layout-caching)
- [Incremental Updates](#incremental-updates)
- [Style Conversion](#style-conversion)
- [Debugging & Profiling](#debugging--profiling)

## Layout Engine Overview

Kryon's layout engine is built on [Taffy](https://github.com/DioxusLabs/taffy), a high-performance flexbox layout library, with extensive optimizations for real-time UI rendering across multiple backends.

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                OptimizedTaffyLayoutEngine                   │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐│
│  │   TaffyTree     │ │  Layout Cache   │ │  Style Convert  ││
│  │ (Flex Engine)   │ │ (Performance)   │ │ (KRB → Taffy)   ││
│  └─────────────────┘ └─────────────────┘ └─────────────────┘│
└─────────────────────────────────────────────────────────────┘
            │                    │                    │
            ▼                    ▼                    ▼
    ┌───────────────┐    ┌───────────────┐    ┌───────────────┐
    │   Flexbox     │    │   Change      │    │   Element     │
    │  Computation  │    │  Detection    │    │ Properties    │
    │              │    │              │    │              │
    └───────────────┘    └───────────────┘    └───────────────┘
```

### Core Components

```rust
pub struct OptimizedTaffyLayoutEngine {
    /// Main Taffy layout tree
    taffy: TaffyTree<ElementId>,
    /// Element ID to Taffy node mapping
    element_to_node: HashMap<ElementId, taffy::NodeId>,
    /// Taffy node to Element ID reverse mapping
    node_to_element: HashMap<taffy::NodeId, ElementId>,
    /// Cached layout results for performance
    layout_cache: HashMap<ElementId, Layout>,
    /// Configuration and performance settings
    config: LayoutConfig,
    /// Invalidation regions for partial updates
    invalidation_regions: Vec<InvalidationRegion>,
}
```

## Taffy Integration

### Taffy Layout Tree

Kryon uses Taffy's tree structure to represent the UI hierarchy:

```rust
// Create Taffy nodes for each element
let style = self.krb_to_taffy_style(element);
let node = self.taffy.new_leaf(style)?;

self.element_to_node.insert(element_id, node);
self.node_to_element.insert(node, element_id);

// Build parent-child relationships
for &child_id in &element.children {
    if let Some(&child_node) = self.element_to_node.get(&child_id) {
        child_nodes.push(child_node);
    }
}
self.taffy.set_children(parent_node, &child_nodes)?;
```

### Layout Computation Process

```rust
pub fn compute_layout_optimized(
    &mut self,
    elements: &HashMap<ElementId, Element>,
    root_element_id: ElementId,
    available_space: Size<f32>,
) -> Result<(), taffy::TaffyError> {
    // 1. Check if cached results can be used
    if self.config.enable_caching && !self.needs_layout_update(elements) {
        return Ok(()); // Use cached layout
    }
    
    // 2. Determine update strategy (full vs incremental)
    let use_incremental = self.config.incremental_updates && 
                         !self.invalidation_regions.is_empty();
    
    // 3. Compute layout using appropriate strategy
    if use_incremental {
        self.compute_incremental_layout(elements, root_element_id, available_space)?;
    } else {
        self.compute_full_layout(elements, root_element_id, available_space)?;
    }
    
    // 4. Cache results and clear invalidation regions
    self.cache_layouts(elements)?;
    self.clear_invalidation_regions();
    
    Ok(())
}
```

## Performance Optimizations

### Layout Configuration

```rust
#[derive(Debug, Clone)]
pub struct LayoutConfig {
    /// Enable debug logging (disabled by default for performance)
    pub debug_logging: bool,
    /// Enable layout caching across frames
    pub enable_caching: bool,
    /// Maximum cache size to prevent memory growth
    pub max_cache_size: usize,
    /// Enable incremental layout updates
    pub incremental_updates: bool,
}

impl Default for LayoutConfig {
    fn default() -> Self {
        Self {
            debug_logging: false,  // Optimized for production
            enable_caching: true,
            max_cache_size: 1024,
            incremental_updates: true,
        }
    }
}
```

### Change Detection

```rust
pub fn needs_layout_update(&self, elements: &HashMap<ElementId, Element>) -> bool {
    if !self.config.enable_caching {
        return true;
    }
    
    // Fast hash-based change detection
    let current_hash = self.compute_elements_hash(elements);
    current_hash != self.last_elements_hash
}

fn compute_elements_hash(&self, elements: &HashMap<ElementId, Element>) -> u64 {
    let mut hasher = DefaultHasher::new();
    
    for (&id, element) in elements {
        id.hash(&mut hasher);
        // Hash layout-affecting properties
        element.layout_size.width.to_pixels(1.0).to_bits().hash(&mut hasher);
        element.layout_size.height.to_pixels(1.0).to_bits().hash(&mut hasher);
        element.layout_flags.hash(&mut hasher);
        element.children.len().hash(&mut hasher);
    }
    
    hasher.finish()
}
```

## Layout Caching

### Cache Management

```rust
fn cache_layouts(&mut self, elements: &HashMap<ElementId, Element>) -> Result<(), taffy::TaffyError> {
    self.layout_cache.clear();
    self.layout_cache.reserve(elements.len());
    
    for (&element_id, _) in elements {
        if let Some(&node_id) = self.element_to_node.get(&element_id) {
            let layout = *self.taffy.layout(node_id)?;
            self.layout_cache.insert(element_id, layout);
        }
    }
    
    // Enforce cache size limits
    if self.layout_cache.len() > self.config.max_cache_size {
        self.layout_cache.clear();
    }
    
    Ok(())
}
```

### Cache Access

```rust
pub fn get_layout(&self, element_id: ElementId) -> Option<&Layout> {
    self.layout_cache.get(&element_id)
}

pub fn get_cache_stats(&self) -> (usize, usize) {
    (self.layout_cache.len(), self.config.max_cache_size)
}
```

## Incremental Updates

### Invalidation Regions

```rust
#[derive(Debug, Clone)]
pub struct InvalidationRegion {
    /// Elements that need layout recomputation
    pub elements: Vec<ElementId>,
    /// Whether this affects the entire tree
    pub full_tree: bool,
    /// Affected viewport region
    pub bounds: Option<taffy::Rect<f32>>,
}
```

### Incremental Layout Process

```rust
fn compute_incremental_layout(
    &mut self,
    elements: &HashMap<ElementId, Element>,
    root_element_id: ElementId,
    available_space: Size<f32>,
) -> Result<(), taffy::TaffyError> {
    // 1. Collect elements that need updates
    let mut elements_to_update = HashSet::new();
    
    for region in &self.invalidation_regions {
        if region.full_tree {
            // Fall back to full layout for tree-wide changes
            return self.compute_full_layout(elements, root_element_id, available_space);
        }
        
        // Add region elements and their children
        for &element_id in &region.elements {
            elements_to_update.insert(element_id);
            self.add_children_to_update_set(elements, element_id, &mut elements_to_update);
        }
    }
    
    // 2. Update styles for affected elements only
    for &element_id in &elements_to_update {
        if let Some(element) = elements.get(&element_id) {
            if let Some(&node_id) = self.element_to_node.get(&element_id) {
                let new_style = self.krb_to_taffy_style(element);
                
                // Only update if style actually changed
                if let Ok(current_style) = self.taffy.style(node_id) {
                    if !self.styles_equal(&current_style, &new_style) {
                        self.taffy.set_style(node_id, new_style)?;
                    }
                }
            }
        }
    }
    
    // 3. Recompute layout (Taffy optimizes internally)
    let root_node = self.element_to_node.get(&root_element_id).copied()
        .ok_or(taffy::TaffyError::InvalidParentNode(taffy::NodeId::from(0u64)))?;
    
    self.taffy.compute_layout(root_node, available_space)?;
    
    // 4. Update cache for affected elements only
    self.update_cache_for_elements(elements, &elements_to_update)?;
    
    Ok(())
}
```

## Flexbox Support

### Flexbox Property Mapping

Kryon supports all CSS flexbox properties through Taffy:

| KRY Property | Taffy Property | Description | Example |
|--------------|----------------|-------------|---------|
| `display` | `Display` | Layout type | `"flex"`, `"block"`, `"none"` |
| `flexDirection` | `FlexDirection` | Main axis direction | `"row"`, `"column"` |
| `justifyContent` | `JustifyContent` | Main axis alignment | `"center"`, `"space-between"` |
| `alignItems` | `AlignItems` | Cross axis alignment | `"center"`, `"stretch"` |
| `flexGrow` | `flex_grow` | Flex grow factor | `1.0`, `2.5` |
| `flexShrink` | `flex_shrink` | Flex shrink factor | `0.0`, `1.0` |
| `gap` | `gap` | Spacing between items | `10.0` |

### Style Conversion

```rust
fn apply_custom_properties(&self, style: &mut Style, element: &Element) {
    use kryon_core::PropertyValue;
    
    // Display property
    if let Some(PropertyValue::String(value)) = element.custom_properties.get("display") {
        style.display = match value.as_str() {
            "grid" => Display::Grid,
            "flex" => Display::Flex,
            "block" => Display::Block,
            "none" => Display::None,
            _ => style.display,
        };
    }
    
    // Flexbox direction
    if let Some(PropertyValue::String(value)) = element.custom_properties.get("flex_direction") {
        style.flex_direction = match value.as_str() {
            "row" => FlexDirection::Row,
            "column" => FlexDirection::Column,
            "row-reverse" => FlexDirection::RowReverse,
            "column-reverse" => FlexDirection::ColumnReverse,
            _ => style.flex_direction,
        };
    }
    
    // Justify content (main axis)
    if let Some(PropertyValue::String(value)) = element.custom_properties.get("justify_content") {
        style.justify_content = match value.as_str() {
            "flex-start" | "start" => Some(JustifyContent::FlexStart),
            "flex-end" | "end" => Some(JustifyContent::FlexEnd),
            "center" => Some(JustifyContent::Center),
            "space-between" => Some(JustifyContent::SpaceBetween),
            "space-around" => Some(JustifyContent::SpaceAround),
            "space-evenly" => Some(JustifyContent::SpaceEvenly),
            _ => style.justify_content,
        };
    }
    
    // Numeric flex properties
    if let Some(PropertyValue::Float(value)) = element.custom_properties.get("flex_grow") {
        style.flex_grow = *value;
    }
    
    if let Some(PropertyValue::Float(value)) = element.custom_properties.get("flex_shrink") {
        style.flex_shrink = *value;
    }
}
```

### Flexbox Examples

```kry
Container {
    display: "flex"
    flexDirection: "row"
    justifyContent: "space-between"
    alignItems: "center"
    gap: 10
    
    Button {
        flexGrow: 1
        text: "Button 1"
    }
    
    Button {
        flexGrow: 2
        text: "Button 2 (Wider)"
    }
    
    Button {
        flexShrink: 0
        width: 100px
        text: "Fixed Width"
    }
}
```

## Position Systems

### Absolute Positioning

```rust
fn apply_layout_flags(&self, style: &mut Style, element: &Element) {
    let flags = element.layout_flags;
    
    // Absolute positioning (flag 0x02)
    if flags & 0x02 != 0 {
        style.position = Position::Absolute;
        
        // Set position from element properties
        match &element.layout_position.x {
            LayoutDimension::Pixels(px) if *px != 0.0 => {
                style.inset.left = LengthPercentageAuto::Length(*px);
            }
            LayoutDimension::Percentage(pct) => {
                style.inset.left = LengthPercentageAuto::Percent(*pct);
            }
            _ => {}
        }
        
        match &element.layout_position.y {
            LayoutDimension::Pixels(px) if *px != 0.0 => {
                style.inset.top = LengthPercentageAuto::Length(*px);
            }
            LayoutDimension::Percentage(pct) => {
                style.inset.top = LengthPercentageAuto::Percent(*pct);
            }
            _ => {}
        }
    }
}
```

### Position Calculation

```rust
fn compute_absolute_positions(
    &self,
    elements: &HashMap<ElementId, Element>,
    element_id: ElementId,
    parent_offset: Vec2,
    computed_positions: &mut HashMap<ElementId, Vec2>,
    computed_sizes: &mut HashMap<ElementId, Vec2>,
) {
    if let Some(layout) = self.get_layout(element_id) {
        let taffy_position = Vec2::new(layout.location.x, layout.location.y);
        let taffy_size = Vec2::new(layout.size.width, layout.size.height);
        
        let final_position = if let Some(element) = elements.get(&element_id) {
            // Check for absolute positioning
            let has_absolute_position = element.custom_properties.get("position")
                .map(|p| matches!(p, PropertyValue::String(s) if s == "absolute"))
                .unwrap_or(false) || (element.layout_flags & 0x02 != 0);
            
            if has_absolute_position {
                // Use element's layout_position for absolute elements
                match (&element.layout_position.x, &element.layout_position.y) {
                    (LayoutDimension::Pixels(x), LayoutDimension::Pixels(y)) => {
                        Vec2::new(*x, *y)
                    }
                    _ => parent_offset + taffy_position // Fallback
                }
            } else {
                // Use Taffy computed position for flex layout
                parent_offset + taffy_position
            }
        } else {
            parent_offset + taffy_position
        };
        
        computed_positions.insert(element_id, final_position);
        computed_sizes.insert(element_id, taffy_size);
        
        // Process children recursively
        if let Some(element) = elements.get(&element_id) {
            for &child_id in &element.children {
                self.compute_absolute_positions(
                    elements, child_id, final_position,
                    computed_positions, computed_sizes
                );
            }
        }
    }
}
```

## Element Type Optimizations

### Type-Specific Layout Rules

```rust
fn apply_element_type_optimizations(&self, style: &mut Style, element: &Element) {
    match element.element_type {
        ElementType::Text | ElementType::Link => {
            // Intrinsic sizing for text elements
            let text_height = element.font_size.max(16.0);
            let text_length = element.text.len() as f32;
            let char_width = element.font_size * 0.6;
            let text_width = (text_length * char_width).max(50.0);
            
            // Apply dimensions if not explicitly set
            match element.layout_size.width {
                LayoutDimension::Auto => {
                    style.size.width = Dimension::Length(text_width);
                }
                LayoutDimension::Pixels(px) if px <= 0.0 => {
                    style.size.width = Dimension::Length(text_width);
                }
                _ => {}
            }
        }
        
        ElementType::Button => {
            // Buttons don't flex by default
            if !element.custom_properties.contains_key("display") {
                style.display = Display::Block;
            }
            style.flex_grow = 0.0;
            style.flex_shrink = 0.0;
        }
        
        ElementType::App => {
            // App elements are flex containers
            if !element.custom_properties.contains_key("display") {
                style.display = Display::Flex;
            }
            if !element.custom_properties.contains_key("flex_direction") {
                style.flex_direction = FlexDirection::Column;
            }
        }
        
        ElementType::Container => {
            // Smart flex detection for containers
            let should_be_flex = element.custom_properties.contains_key("flex_direction") ||
                               element.custom_properties.contains_key("justify_content") ||
                               element.custom_properties.contains_key("align_items") ||
                               element.layout_flags != 0;
            
            if should_be_flex {
                style.display = Display::Flex;
            }
        }
        
        _ => {}
    }
}
```

## Box Model Support

### Margin and Padding

```rust
// Uniform margin/padding
if let Some(PropertyValue::Float(value)) = element.custom_properties.get("margin") {
    let margin = LengthPercentageAuto::Length(*value);
    style.margin = taffy::Rect {
        left: margin, right: margin,
        top: margin, bottom: margin,
    };
}

// Individual padding properties
if let Some(PropertyValue::Float(value)) = element.custom_properties.get("padding_top") {
    style.padding.top = LengthPercentage::Length(*value);
}
if let Some(PropertyValue::Float(value)) = element.custom_properties.get("padding_right") {
    style.padding.right = LengthPercentage::Length(*value);
}
if let Some(PropertyValue::Float(value)) = element.custom_properties.get("padding_bottom") {
    style.padding.bottom = LengthPercentage::Length(*value);
}
if let Some(PropertyValue::Float(value)) = element.custom_properties.get("padding_left") {
    style.padding.left = LengthPercentage::Length(*value);
}
```

### Size Constraints

```rust
// Basic sizing from LayoutDimension
match &element.layout_size.width {
    LayoutDimension::Pixels(px) if *px > 0.0 => {
        style.size.width = Dimension::Length(*px);
    }
    LayoutDimension::Percentage(pct) => {
        style.size.width = Dimension::Percent(*pct);
    }
    LayoutDimension::MinPixels(px) => {
        style.min_size.width = Dimension::Length(*px);
    }
    LayoutDimension::MaxPixels(px) => {
        style.max_size.width = Dimension::Length(*px);
    }
    _ => {}
}
```

## Debugging & Profiling

### Debug Configuration

```rust
pub fn set_debug_logging(&mut self, enabled: bool) {
    self.config.debug_logging = enabled;
}

// Debug output during layout computation
if self.config.debug_logging {
    debug!("Layout computation completed for {} elements", elements.len());
    debug!("Element {}: Taffy position={:?}, size={:?}", 
           element_id, taffy_position, taffy_size);
}
```

### Performance Metrics

```rust
pub fn get_layout_stats(&self) -> LayoutStats {
    LayoutStats {
        cached_elements: self.layout_cache.len(),
        cache_capacity: self.config.max_cache_size,
        taffy_nodes: self.element_to_node.len(),
        invalidation_regions: self.invalidation_regions.len(),
    }
}

#[derive(Debug)]
pub struct LayoutStats {
    pub cached_elements: usize,
    pub cache_capacity: usize,
    pub taffy_nodes: usize,
    pub invalidation_regions: usize,
}
```

### Layout Validation

```rust
pub fn validate_layout(&self, elements: &HashMap<ElementId, Element>) -> Vec<String> {
    let mut issues = Vec::new();
    
    for (&element_id, element) in elements {
        // Check for common layout issues
        if element.layout_size.width == LayoutDimension::Pixels(0.0) &&
           element.layout_size.height == LayoutDimension::Pixels(0.0) {
            issues.push(format!("Element {} has zero size", element_id));
        }
        
        // Check for missing Taffy nodes
        if !self.element_to_node.contains_key(&element_id) {
            issues.push(format!("Element {} missing from Taffy tree", element_id));
        }
    }
    
    issues
}
```

### Common Layout Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| **Zero-sized elements** | No intrinsic size | Set explicit width/height or content |
| **Flex items not growing** | Missing `flexGrow` | Add `flexGrow: 1` |
| **Absolute positioning not working** | Missing position property | Set `position: "absolute"` |
| **Text not visible** | Zero calculated size | Set minimum dimensions |
| **Flexbox not centering** | Wrong alignment properties | Use `justifyContent: "center"` |

## Integration Examples

### Basic Layout

```kry
App {
    display: "flex"
    flexDirection: "column"
    width: 800px
    height: 600px
    
    # Header (fixed height)
    Container {
        height: 60px
        backgroundColor: "#333333"
        
        Text {
            text: "Header"
            color: "#FFFFFF"
        }
    }
    
    # Content (flexible)
    Container {
        flexGrow: 1
        display: "flex"
        flexDirection: "row"
        
        # Sidebar (fixed width)
        Container {
            width: 200px
            backgroundColor: "#F0F0F0"
        }
        
        # Main content (flexible)
        Container {
            flexGrow: 1
            padding: 20
        }
    }
    
    # Footer (fixed height)
    Container {
        height: 40px
        backgroundColor: "#333333"
    }
}
```

### Responsive Layout

```kry
Container {
    display: "flex"
    flexDirection: "row"
    flexWrap: "wrap"
    gap: 10
    
    Button {
        flexGrow: 1
        minWidth: 120px
        text: "Responsive Button 1"
    }
    
    Button {
        flexGrow: 1
        minWidth: 120px
        text: "Responsive Button 2"
    }
    
    Button {
        flexGrow: 1
        minWidth: 120px
        text: "Responsive Button 3"
    }
}
```

This layout engine provides high-performance, flexible layout capabilities through the Taffy integration while maintaining excellent performance through caching, incremental updates, and optimized style conversion.