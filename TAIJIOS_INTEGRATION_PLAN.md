# Plan: Kryon Runtime Integration into TaijiOS

## Executive Summary

**Transform TaijiOS to natively support Kryon apps** by implementing the Kryon Runtime as a built-in TaijiOS component. Instead of linking external libraries, TaijiOS will have native Kryon support baked in.

**Vision**: TaijiOS becomes the Kryon OS - Kryon apps (KRY) compile to DIS and run natively with first-class system support.

---

## Architecture Overview

### Before (External Runtime)
```
KRY → KIR → DIS → TaijiOS → External Runtime → X11
                          └─ Links to kryonrt.dis (slow, complex)
```

### After (Built-in Runtime)
```
KRY → KIR → DIS → TaijiOS (with KryonRT built-in) → X11
                          └─ Native runtime calls (fast, simple)
```

**Benefits:**
- ✅ No external linking - runtime is part of TaijiOS
- ✅ Better performance - direct function calls
- ✅ Simpler deployment - runtime always available
- ✅ System integration - deep access to TaijiOS APIs
- ✅ Smaller app .dis files - no runtime bloat

---

## Implementation Strategy

### Phase 1: Add Kryon Runtime to TaijiOS

**Location**: `/home/wao/Projects/TaijiOS/`

Create new directory structure:
```
/home/wao/Projects/TaijiOS/
├── dis/
│   └── lib/
│       └── kryonrt/          # NEW: Kryon Runtime Library
│           ├── kryonrt.b     # Main runtime module
│           ├── window.b      # Window management
│           ├── graphics.b    # Drawing primitives
│           ├── components.b  # UI components
│           ├── events.b      # Event handling
│           ├── layout.b      # Layout engine
│           └── mkfile        # Build configuration
└── ...
```

**Goal**: TaijiOS ships with `/dis/lib/kryonrt.dis` pre-installed.

### Phase 2: Implement Runtime Modules

#### 2.1 Core Runtime (kryonrt.b)

**Purpose**: Main entry point and initialization

```limbo
# dis/lib/kryonrt/kryonrt.b
implement KryonRT;
include "sys.m";
include "draw.m";
include "tk.m";
include "tkclient.m";

sys : Sys;
draw : Draw;
tk   : Tk;
tkclient : Tkclient;

# Type definitions
Window : adt {
    tk    : ref Tk->Toplevel;
    image : ref Draw->Image;
    screen: ref Draw->Screen;
};

# Module initialization
init(d : ref Draw->Context, argv : list of string) {
    sys = load Sys Sys->PATH;
    draw = load Draw Draw->PATH;
    tk = load Tk Tk->PATH;
    tkclient = load Tkclient Tkclient->PATH;

    sys->print("KryonRT initialized\n");
}

# Window management
window_create(title : string, width : int, height : int) : ref Window {
    if (d == nil) {
        sys->fprint(sys->fildes(2), "KryonRT: draw context not initialized\n");
        return nil;
    }

    win := ref Window(nil, d.screen.image, d.screen);

    # Create Tk window
    opts := " -x " + string width + " -y " + string height;
    win.tk = tkclient->toplevel(d.screen, title, Tkclient->Appl);

    # Configure window
    tk->name(win.tk, "win");
    tk->cmd(win.tk, "pack propagate win 0");

    return win;
}

# Main event loop
event_loop(win : ref Window) {
    if (win == nil) return;

    tk->cmd(win.tk, "update");
    tk->cmd(win.tk, "waitvariable win;destroy");

    # Simple event loop
    for (;;) {
        alt {
            p := <-win.tk.ctxt.pointer =>
                if (p.buttons & 1) {
                    # Left click - handle in events.b
                    handle_click(p.xy);
                }

            c := <-win.tk.ctxt.kbd =>
                if (c == 'q' || c == '\u007F') {
                    return;
                }
        }
    }
}

# Text rendering
text_draw(win : ref Window, text : string, x : int, y : int,
          color : int, fontname : string) {
    if (win == nil || win.image == nil) return;

    font := win.display.open(fontname);
    if (font == nil) font = win.display.open("/lib/font/bit/lucidasans/unicode.8.font");

    # Parse color to RGB
    r := (color >> 16) & 16rFF;
    g := (color >> 8) & 16rFF;
    b := color & 16rFF;

    # Set drawing color
    win.image.draw(
        win.image.rect,
        draw->Display.rgb(r, g, b),
        nil,
        (x, y)
    );
}

# Rectangle drawing
rect_draw(win : ref Window, x : int, y : int, w : int, h : int, color : int) {
    if (win == nil || win.image == nil) return;

    r := (color >> 16) & 16rFF;
    g := (color >> 8) & 16rFF;
    b := color & 16rFF;

    rect := Rect((x, y), (x + w, y + h));
    win.image.draw(
        rect,
        draw->Display.rgb(r, g, b),
        nil,
        rect.min
    );
}
```

#### 2.2 Component System (components.b)

**Purpose**: UI component rendering

```limbo
# dis/lib/kryonrt/components.b
implement Components;
include "sys.m";
include "draw.m";
include "kryonrt.m";

sys : Sys;
draw : Draw;

# Component rendering dispatch
render_component(win : ref KryonRT->Window, comp_json : string) {
    # Parse JSON component
    # Simplified JSON parsing (TaijiOS style)
    type := extract_type(comp_json);

    case type {
    "Text" =>
        render_text(win, comp_json);
    "Container" =>
        render_container(win, comp_json);
    "Column" =>
        render_column(win, comp_json);
    "Row" =>
        render_row(win, comp_json);
    "Button" =>
        render_button(win, comp_json);
    * =>
        sys->fprint(sys->fildes(2), "Unknown component: %s\n", type);
    }
}

# Text component
render_text(win : ref KryonRT->Window, comp : string) {
    text := extract_string(comp, "text");
    x := extract_int(comp, "left");
    y := extract_int(comp, "top");
    color := parse_color(extract_string(comp, "color"));

    KryonRT->text_draw(win, text, x, y, color, "default");
}

# Container component (box with background/border)
render_container(win : ref KryonRT->Window, comp : string) {
    x := extract_int(comp, "left");
    y := extract_int(comp, "top");
    width := extract_int(comp, "width");
    height := extract_int(comp, "height");
    bg := parse_color(extract_string(comp, "background"));

    # Draw background
    KryonRT->rect_draw(win, x, y, width, height, bg);

    # Draw border if present
    if (has_border(comp)) {
        border_color := parse_color(extract_border_color(comp));
        border_width := extract_border_width(comp);

        # Draw border (four rectangles)
        KryonRT->rect_draw(win, x, y, width, border_width, border_color);           # Top
        KryonRT->rect_draw(win, x, y + height - border_width, width, border_width, border_color);  # Bottom
        KryonRT->rect_draw(win, x, y, border_width, height, border_color);          # Left
        KryonRT->rect_draw(win, x + width - border_width, y, border_width, height, border_color);   # Right
    }

    # Render children
    children := extract_children(comp);
    for (i := 0; i < len children; i++) {
        render_component(win, children[i]);
    }
}

# Column layout (vertical stack)
render_column(win : ref KryonRT->Window, comp : string) {
    children := extract_children(comp);
    y_offset := extract_int(comp, "top");
    x := extract_int(comp, "left");
    padding := extract_int(comp, "padding");

    for (i := 0; i < len children; i++) {
        child := children[i];

        # Update child position
        set_position(child, x, y_offset);

        # Render child
        render_component(win, child);

        # Advance offset based on child height
        child_height := extract_int(child, "height");
        y_offset += child_height + padding;
    }
}

# Row layout (horizontal)
render_row(win : ref KryonRT->Window, comp : string) {
    children := extract_children(comp);
    x_offset := extract_int(comp, "left");
    y := extract_int(comp, "top");
    padding := extract_int(comp, "padding");

    for (i = 0; i < len children; i++) {
        child := children[i];

        # Update child position
        set_position(child, x_offset, y);

        # Render child
        render_component(win, child);

        # Advance offset
        child_width := extract_int(child, "width");
        x_offset += child_width + padding;
    }
}

# Button component
render_button(win : ref KryonRT->Window, comp : string) {
    x := extract_int(comp, "left");
    y := extract_int(comp, "top");
    width := extract_int(comp, "width");
    height := extract_int(comp, "height");
    bg := parse_color(extract_string(comp, "background"));

    # Draw button background
    KryonRT->rect_draw(win, x, y, width, height, bg);

    # Draw button text
    text := extract_string(comp, "text");
    KryonRT->text_draw(win, text, x + 10, y + 10, 16r000000, "default");

    # Register button click region
    register_click_region(x, y, width, height, comp);
}
```

#### 2.3 Event Handling (events.b)

```limbo
# dis/lib/kryonrt/events.b
implement Events;
include "sys.m";

sys : Sys;

# Click region tracking
ClickRegion : adt {
    x, y, w, h : int;
    handler_id : int;
};

regions : list of ref ClickRegion = nil;

# Register a clickable region
register_click_region(x : int, y : int, w : int, h : int, handler_id : int) {
    region := ref ClickRegion(x, y, w, h, handler_id);
    regions = region :: regions;
}

# Handle mouse click
handle_click(p : draw->Point) {
    # Check all registered regions
    for (l := regions; l != nil; l = tl l) {
        r : ref ClickRegion = hd l;
        if (p.x >= r.x && p.x < r.x + r.w &&
            p.y >= r.y && p.y < r.y + r.h) {
            # Click is in this region
            invoke_handler(r.handler_id);
            break;
        }
    }
}

# Invoke event handler (call back into user code)
invoke_handler(handler_id : int) {
    # This will call back to the user's DIS code
    # For now, just print
    sys->print("Event handler %d invoked\n", handler_id);
}
```

#### 2.4 JSON Helper (jsonutils.b)

```limbo
# dis/lib/kryonrt/jsonutils.b
implement JSONUtils;
include "sys.m";

sys : Sys;

# Simple JSON extraction functions
# TaijiOS doesn't have a JSON library, so we use string parsing

extract_type(json : string) : string {
    # Find "type": "value"
    return extract_string_key(json, "type");
}

extract_string(json : string, key : string) : string {
    return extract_string_key(json, key);
}

extract_int(json : string, key : string) : int {
    str := extract_string_key(json, key);
    if (str == nil) return 0;

    # Parse "123px" or just "123"
    if (len str > 2 && str[len str - 2:] == "px") {
        str = str[:len str - 2];
    }

    return int str;
}

extract_string_key(json : string, key : string) : string {
    # Find "key": "value"
    search := "\"" + key + "\":\"";
    start := sys->strstr(json, search);
    if (start == -1) return nil;

    start += len search;

    # Find closing quote
    end := sys->strstr(json[start:], "\"");
    if (end == -1) return nil;

    return json[start:start + end];
}

parse_color(color_str : string) : int {
    if (color_str == nil) return 16rFFFFFF;

    # Parse #RRGGBB
    if (color_str[0] == '#') {
        r := int color_str[1:3];
        g := int color_str[3:5];
        b := int color_str[5:7];
        return (r << 16) | (g << 8) | b;
    }

    return 16rFFFFFF;
}
```

### Phase 3: Build System Integration

#### 3.1 Create mkfile

**File**: `/home/wao/Projects/TaijiOS/dis/lib/kryonrt/mkfile`

```limbo
# Kryon Runtime Library Build Configuration

LIB=libkryonrt.dis

MODULES=\
    kryonrt.dis\
    window.dis\
    components.dis\
    events.dis\
    jsonutils.dis\
    layout.dis

all:V: $LIB

$LIB: $MODULES
    # Create library archive
    cat $MODULES > $LIB

%.dis: %.b
    limbo -I../../include $stem.b

clean:V:
    rm -f *.dis *.sbl

install:V: $LIB
    # Copy to system lib directory
    cp $LIB /dis/lib/kryonrt.dis
```

#### 3.2 Build and Install

```bash
cd /home/wao/Projects/TaijiOS/dis/lib/kryonrt
mk
mk install

# Verify installation
ls -l /dis/lib/kryonrt.dis
```

**Result**: TaijiOS now has `/dis/lib/kryonrt.dis` as a built-in library.

---

## Phase 4: Update Kryon DIS Codegen

### 4.1 Import KryonRT Functions

**File**: `codegens/dis/src/kir_translator.c`

Update to use built-in runtime:

```c
// Add runtime imports at module initialization
static bool add_kryonrt_imports(DISModuleBuilder* builder) {
    // Import: KryonRT module
    add_import(builder, "kryonrt", "init", 0, 0);
    add_import(builder, "kryonrt", "window_create", 0, 0);
    add_import(builder, "kryonrt", "text_draw", 0, 0);
    add_import(builder, "kryonrt", "rect_draw", 0, 0);
    add_import(builder, "kryonrt", "event_loop", 0, 0);

    // Import: Components module
    add_import(builder, "components", "render_component", 0, 0);

    return true;
}
```

### 4.2 Generate Main Entry Point

```c
static bool generate_main_entry(DISModuleBuilder* builder, cJSON* kir_json) {
    // Get window config from KIR
    cJSON* app = cJSON_GetObjectItem(kir_json, "app");
    const char* title = cJSON_GetObjectItem(app, "windowTitle")->valuestring;
    int width = cJSON_GetObjectItem(app, "windowWidth")->valueint;
    int height = cJSON_GetObjectItem(app, "windowHeight")->valueint;

    // Entry point
    uint32_t entry_pc = builder->current_pc;

    // 1. Call runtime init
    emit_call_import(builder, "kryonrt", "init");

    // 2. Create window
    emit_mov_string(builder, title, R0);
    emit_mov_imm(builder, width, R1);
    emit_mov_imm(builder, height, R2);
    emit_call_import(builder, "kryonrt", "window_create");
    // Result (window ref) is now in R0

    // 3. Serialize component tree to JSON
    cJSON* root = cJSON_GetObjectItem(kir_json, "root");
    char* component_json = cJSON_Print(root);

    // 4. Store component JSON in data section
    uint32_t comp_offset = module_builder_allocate_global(builder, strlen(component_json) + 1);
    emit_data_string(builder, component_json, comp_offset);

    // 5. Call render_component
    emit_mov_address(builder, comp_offset, R1);  // Component JSON
    emit_call_import(builder, "components", "render_component");

    // 6. Enter event loop
    emit_call_import(builder, "kryonrt", "event_loop");

    // 7. Exit
    emit_exit(builder, 0);

    // Set entry point
    module_builder_set_entry(builder, entry_pc, 0);

    free(component_json);
    return true;
}
```

### 4.3 Complete Translation

```c
bool translate_kir_to_dis(DISModuleBuilder* builder, const char* kir_json) {
    // Parse KIR with cJSON
    cJSON* root = cJSON_Parse(kir_json);
    if (!root) {
        dis_codegen_set_error("Failed to parse KIR JSON");
        return false;
    }

    // Add KryonRT imports
    add_kryonrt_imports(builder);

    // Generate main entry point
    if (!generate_main_entry(builder, root)) {
        cJSON_Delete(root);
        return false;
    }

    cJSON_Delete(root);
    return true;
}
```

---

## Phase 5: Testing

### 5.1 Test Application

**File**: `examples/hello_taiji.kry`

```kry
App {
    windowTitle = "Hello TaijiOS"
    windowWidth = 800
    windowHeight = 600
    backgroundColor = "#1a1a2e"

    Container {
        posX = 200
        posY = 100
        width = 400
        height = 300
        backgroundColor = "#16213e"
        borderColor = "#0f3460"
        borderWidth = 3

        Text {
            text = "Hello from TaijiOS!"
            color = "#e94560"
            fontSize = 24
        }
    }
}
```

### 5.2 Build and Run

```bash
# Compile KRY to KIR
./cli/kryon compile examples/hello_taiji.kry

# Generate DIS (calls TaijiOS runtime)
./cli/kryon build --target=dis examples/hello_taiji.kry

# Run in TaijiOS
/home/wao/Projects/TaijiOS/run-app.sh build/hello_taiji.dis
```

**Expected Result**:
- Window opens in X11
- Dark blue container with border
- Red/pink text "Hello from TaijiOS!"
- Window stays open

---

## Implementation Checklist

### TaijiOS Integration (In TaijiOS Repo)

- [ ] Create `/home/wao/Projects/TaijiOS/dis/lib/kryonrt/` directory
- [ ] Implement `kryonrt.b` (core runtime)
- [ ] Implement `window.b` (window management)
- [ ] Implement `components.b` (UI components)
- [ ] Implement `events.b` (event handling)
- [ ] Implement `jsonutils.b` (JSON parsing)
- [ ] Implement `layout.b` (layout engine)
- [ ] Create `mkfile` (build configuration)
- [ ] Build all modules to `.dis`
- [ ] Install to `/dis/lib/kryonrt.dis`
- [ ] Test runtime loads correctly

### Kryon Codegen Updates (In Kryon Repo)

- [ ] Update `kir_translator.c` to use cJSON properly
- [ ] Add runtime import functions
- [ ] Implement main entry generation
- [ ] Serialize component tree to JSON
- [ ] Update instruction emitter for import calls
- [ ] Test file writing produces valid .dis

### Testing

- [ ] hello_taiji.kry displays text
- [ ] Containers render with borders
- [ ] Text displays at correct positions
- [ ] Colors render correctly
- [ ] Window closes cleanly
- [ ] Multiple components render
- [ ] Event handlers can be registered

---

## Directory Structure

### TaijiOS (New Files)

```
/home/wao/Projects/TaijiOS/
├── dis/
│   └── lib/
│       └── kryonrt/              ← NEW DIRECTORY
│           ├── kryonrt.b         ← Main runtime (200 lines)
│           ├── window.b          ← Window mgmt (150 lines)
│           ├── components.b      ← UI components (300 lines)
│           ├── events.b          ← Event handling (150 lines)
│           ├── jsonutils.b       ← JSON helpers (200 lines)
│           ├── layout.b          ← Layout engine (200 lines)
│           └── mkfile            ← Build config (30 lines)
└── ...
```

### Kryon (Modified Files)

```
/mnt/storage/Projects/KryonLabs/kryon/
├── codegens/dis/
│   └── src/
│       ├── kir_translator.c      ← Add proper JSON parsing (+200 lines)
│       ├── module_builder.c      ← Complete file writing (+150 lines)
│       └── instruction_emitter.c ← Add import call support (+50 lines)
└── examples/
    ├── hello_taiji.kry           ← NEW test file
    └── counter_taiji.kry         ← NEW test file
```

---

## Summary

This plan makes Kryon a **first-class citizen in TaijiOS** by:

1. **Building runtime into TaijiOS** - No external dependencies
2. **Standard library approach** - Like libdraw or tkclient
3. **Simple codegen** - Just emit import calls to runtime
4. **Better performance** - Direct function calls
5. **Ecosystem growth** - TaijiOS becomes the Kryon OS

**Next Steps:**
1. Start with TaijiOS runtime implementation
2. Build and test in TaijiOS first
3. Update Kryon codegen to use built-in runtime
4. Test end-to-end with real apps

**Total Effort**: ~1,200 lines of Limbo code + 400 lines of C codegen updates = ~1,600 lines
**Timeline**: 2-3 days of focused work

---

Let's make TaijiOS the ultimate Kryon platform! 🚀
