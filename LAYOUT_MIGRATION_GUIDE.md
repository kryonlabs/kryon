# Layout System Migration Guide

## Overview

This guide explains the migration from the current CSS Flexbox layout system to the new Flutter-inspired element system.

## Current System Issues

### Problems with CSS Flexbox Approach
- **Complex API**: `justify-content` and `align-items` confusion
- **Redundant Systems**: Multiple layout systems competing
- **Performance Overhead**: Full CSS flexbox calculations for simple stacking
- **Inconsistent Usage**: Some places use justify/align, others use basic positioning

### Current Implementation
```c
// Old complex system from layout_engine.c
typedef enum {
    KRYON_LAYOUT_FLEX,
    KRYON_LAYOUT_GRID,
    KRYON_LAYOUT_ABSOLUTE,
} KryonLayoutType;

// Confusing properties
typedef struct {
    char* justify_content;    // "flex-start", "center", etc.
    char* align_items;        // "stretch", "center", etc.
    char* flex_direction;     // "row", "column"
} KryonFlexProperties;
```

## New Flutter-Inspired System

### Core Elements
```c
typedef enum {
    KRYON_WIDGET_COLUMN,     // Vertical stack
    KRYON_WIDGET_ROW,        // Horizontal stack  
    KRYON_WIDGET_STACK,      // Overlapping (z-index)
    KRYON_WIDGET_CONTAINER,  // Single child with styling
    KRYON_WIDGET_EXPANDED,   // Takes available space
    KRYON_WIDGET_FLEXIBLE,   // Takes proportional space
} KryonElementType;

// Clear alignment system
typedef enum {
    KRYON_MAIN_START,
    KRYON_MAIN_CENTER, 
    KRYON_MAIN_END,
    KRYON_MAIN_SPACE_BETWEEN,
    KRYON_MAIN_SPACE_AROUND,
    KRYON_MAIN_SPACE_EVENLY,
} KryonMainAxisAlignment;
```

## Migration Examples

### Current (CSS Flexbox) - WORKING
```kry
Container {
    display: "flex"
    flexDirection: "column"
    justifyContent: "center"  
    alignItems: "stretch"
    gap: 10px
    
    Text { text: "Header" }
    
    Container {
        display: "flex"
        flexDirection: "row"
        justifyContent: "space-between"
        
        Button { text: "Left" }
        Button { text: "Right" }
    }
    
    Text { text: "Footer" }
}
```

### Proposed (Flutter-Inspired) - NEW SYSTEM
```c
// This would be handled by the new C element system internally
// The .kry syntax stays the same, but internally uses Flutter-inspired elements
```

## Dynamic Transformation Examples

### Responsive Layout Changes
```lua
-- Old system: Required rebuilding entire layout
-- New system: Transform existing elements

local content_area = kryon.get_element("content")

if screen_width < 600 then
    -- Mobile: vertical layout
    kryon.transform_element(content_area, "column")
    kryon.set_property(content_area, "main_axis", "start")
else
    -- Desktop: horizontal layout  
    kryon.transform_element(content_area, "row")
    kryon.set_property(content_area, "main_axis", "space_between")
end
```

### Interactive UI Changes
```lua
-- Transform button into expandable menu
local menu_button = kryon.get_element("menu_button")

if menu_expanded then
    -- Transform to column with multiple options
    kryon.transform_widget(menu_button, "column")
    
    -- Add child buttons
    for i, option in ipairs(menu_options) do
        kryon.add_child(menu_button, kryon.create_widget("button", {
            text = option.text,
            on_click = option.handler
        }))
    end
else
    -- Transform back to single button
    kryon.transform_widget(menu_button, "button")
    kryon.set_property(menu_button, "text", "Menu")
end
```

## Migration Steps

### Phase 1: Update Widget Definitions
1. Replace CSS Flexbox properties with widget types
2. Convert `justify-content`/`align-items` to `main_axis`/`cross_axis`
3. Use `Column`/`Row` instead of `flex-direction`

### Phase 2: Implement Transformation API
1. Add widget transformation functions to Lua
2. Update runtime to support dynamic widget changes
3. Implement layout invalidation and recalculation

### Phase 3: Update Examples
1. Convert all `.kry` files to new syntax
2. Add dynamic transformation examples
3. Update documentation and tutorials

## Benefits of New System

### Simplicity
- **Mental Model**: Column/Row instead of flex-direction + justify/align
- **Fewer Concepts**: No need to learn CSS Flexbox specifics
- **Consistent API**: Same alignment system across all widgets

### Performance
- **Direct Calculations**: No CSS parsing or complex flexbox algorithms
- **Efficient Updates**: Only recalculate affected widgets
- **Memory Efficient**: Smaller widget structures

### Dynamic Capabilities
- **Runtime Changes**: Transform any widget to any other type
- **Responsive Design**: Easy screen size adaptations
- **Interactive UI**: Widgets can morph based on user interaction

### Developer Experience
- **Familiar**: Flutter developers can immediately understand
- **Predictable**: Clear widget hierarchy and behavior
- **Debuggable**: Easy to inspect widget tree and properties

## Files to Update

### Core Implementation
- `kryon-c/src/layout/layout_engine.c` → Replace with widget system
- `kryon-c/include/kryon/layout_engine.h` → New widget definitions
- `kryon-c/src/script-engines/lua/` → Add transformation API

### Examples and Documentation
- `kryon-c/examples/*.kry` → Convert to new syntax
- `kryon-c/README.md` → Update layout system description
- `kryon-c/docs/` → Update all layout documentation

### Tests
- `kryon-c/tests/layout/` → Update all layout tests
- Add dynamic transformation tests
- Performance comparison tests

This migration will result in a much cleaner, more performant, and more flexible layout system that's easier to understand and use.