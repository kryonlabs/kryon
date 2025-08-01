# Flutter-Inspired Layout System Roadmap

## Overview
Replace the current complex CSS Flexbox system with a Flutter-inspired widget-based layout system that supports dynamic element transformation via Lua scripts.

## Core Widget System

```c
// Widget types that can be dynamically transformed
typedef enum {
    // Layout widgets
    KRYON_WIDGET_COLUMN,
    KRYON_WIDGET_ROW, 
    KRYON_WIDGET_STACK,
    KRYON_WIDGET_CONTAINER,
    KRYON_WIDGET_EXPANDED,
    KRYON_WIDGET_FLEXIBLE,
    
    // UI widgets  
    KRYON_WIDGET_TEXT,
    KRYON_WIDGET_BUTTON,
    KRYON_WIDGET_IMAGE,
    KRYON_WIDGET_INPUT,
} KryonWidgetType;

// Widget transformation capabilities
typedef struct KryonWidget {
    KryonWidgetType type;
    char* id;                    // Unique ID for Lua reference
    
    // Transformation support
    KryonWidgetType previous_type; // For undo/animation
    bool is_transforming;          // Mid-transformation state
    
    // Layout properties (preserved during transformation)
    KryonMainAxisAlignment main_axis;
    KryonCrossAxisAlignment cross_axis;
    float spacing;
    KryonEdgeInsets padding;
    KryonEdgeInsets margin;
    
    // Flex properties
    int flex;
    
    // Dynamic properties (can change based on widget type)
    union {
        struct { // Text widget
            char* text;
            KryonTextStyle style;
        } text_props;
        
        struct { // Button widget
            char* label;
            void (*on_click)(struct KryonWidget*);
        } button_props;
        
        struct { // Container widget
            KryonColor background_color;
            float border_radius;
            KryonBorder border;
        } container_props;
    } props;
    
    // Children (for layout widgets)
    struct KryonWidget** children;
    size_t child_count;
    size_t child_capacity;
    
    // Parent reference for efficient updates
    struct KryonWidget* parent;
} KryonWidget;

// Simple alignment (no justify/align confusion)
typedef enum {
    KRYON_MAIN_START,
    KRYON_MAIN_CENTER, 
    KRYON_MAIN_END,
    KRYON_MAIN_SPACE_BETWEEN,
    KRYON_MAIN_SPACE_AROUND,
    KRYON_MAIN_SPACE_EVENLY,
} KryonMainAxisAlignment;

typedef enum {
    KRYON_CROSS_START,
    KRYON_CROSS_CENTER,
    KRYON_CROSS_END,
    KRYON_CROSS_STRETCH,
} KryonCrossAxisAlignment;
```

## Lua API for Dynamic Transformation

```c
// Core transformation functions
int kryon_widget_transform(lua_State* L);
int kryon_widget_get_type(lua_State* L);
int kryon_widget_set_property(lua_State* L);
int kryon_widget_add_child(lua_State* L);
int kryon_widget_remove_child(lua_State* L);

// Widget factory functions
KryonWidget* kryon_widget_create(KryonWidgetType type, const char* id);
bool kryon_widget_transform_to(KryonWidget* widget, KryonWidgetType new_type);
void kryon_widget_preserve_layout_props(KryonWidget* widget);
```

## Lua Script Examples

```lua
-- Transform a button into a column of buttons
local main_button = kryon.get_widget("main_button")

if user_wants_more_options then
    -- Transform button to column
    kryon.transform_widget(main_button, "column")
    
    -- Add multiple buttons as children
    kryon.add_child(main_button, kryon.create_widget("button", {
        text = "Option 1",
        on_click = handle_option1
    }))
    
    kryon.add_child(main_button, kryon.create_widget("button", {
        text = "Option 2", 
        on_click = handle_option2
    }))
else
    -- Transform back to single button
    kryon.transform_widget(main_button, "button")
    kryon.set_property(main_button, "text", "Main Action")
end

-- Dynamic layout switching
local content_area = kryon.get_widget("content")

if screen_width < 600 then
    -- Mobile: vertical layout
    kryon.transform_widget(content_area, "column")
    kryon.set_property(content_area, "main_axis", "start")
else
    -- Desktop: horizontal layout
    kryon.transform_widget(content_area, "row")
    kryon.set_property(content_area, "main_axis", "space_between")
end
```

## Widget Lifecycle & Memory Management

```c
typedef struct {
    KryonWidget** widgets;
    size_t count;
    size_t capacity;
    // Hash map for O(1) ID lookups
    KryonHashMap* id_map;
} KryonWidgetRegistry;

// Reference counting for safe transformations
typedef struct {
    int ref_count;
    bool marked_for_deletion;
    KryonWidget* widget;
} KryonWidgetRef;

// Lifecycle functions
KryonWidget* kryon_widget_retain(KryonWidget* widget);
void kryon_widget_release(KryonWidget* widget);
void kryon_widget_mark_dirty(KryonWidget* widget);
void kryon_layout_invalidate_from(KryonWidget* widget);
```

## Implementation Roadmap

### Phase 1: Core Widget System (Week 1-2)
1. **Widget Structure**: Implement base `KryonWidget` struct with union for type-specific properties
2. **Memory Management**: Create/destroy/reference counting system
3. **Basic Layout**: Column, Row, Container widgets only
4. **Simple Rendering**: Basic positioning without animations

### Phase 2: Transformation Engine (Week 3-4)  
1. **Widget Factory**: `kryon_widget_create()` and type registration
2. **Transformation Core**: `kryon_widget_transform_to()` with property preservation
3. **Layout Recalculation**: Efficient re-layout after transformations
4. **Parent/Child Management**: Safe adding/removing children during transforms

### Phase 3: Lua Integration (Week 5-6)
1. **Lua Bindings**: Export widget functions to Lua state
2. **Widget Registry**: ID-based widget lookup system  
3. **Property System**: Generic property setting from Lua
4. **Event Handling**: Lua callbacks for widget events

### Phase 4: Advanced Features (Week 7-8)
1. **Stack Widget**: Z-index layering system
2. **Expanded/Flexible**: Flex-based space distribution  
3. **Animation Support**: Smooth transitions during transformations
4. **Performance Optimization**: Dirty flagging, layout caching

### Phase 5: Migration & Testing (Week 9-10)
1. **Replace Old System**: Remove CSS Flexbox implementation
2. **Convert Examples**: Update all .kry files to new system
3. **Performance Testing**: Benchmark against old system
4. **Documentation**: API docs and migration guide

## Example Usage in .kry files

```kry
Container {
    display: "flex"
    flexDirection: "column"
    justifyContent: "center"
    alignItems: "stretch"
    gap: 10px
    
    Text { 
        text: "Header" 
    }
    
    Container {
        flex: 1
        display: "flex"
        flexDirection: "row" 
        justifyContent: "space-between"
        
        Button { text: "Left" }
        Button { text: "Right" }
    }
    
    Text { 
        text: "Footer" 
    }
}
```

## Key Benefits

- **Simple Mental Model**: Column/Row instead of flex-direction + justify/align
- **Dynamic Layouts**: Lua can completely restructure UI at runtime
- **Performance**: No complex CSS calculations, direct widget manipulation
- **Memory Efficient**: Union-based properties, reference counting
- **Familiar API**: Flutter developers can immediately understand the system

## Files to Replace/Modify

- `kryon-c/src/layout/layout_engine.c` - Replace with new widget system
- `kryon-c/include/kryon/layout_engine.h` - New widget definitions
- `kryon-c/examples/*.kry` - Convert to new syntax
- Lua bindings - Add transformation API