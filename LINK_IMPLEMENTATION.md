# Link Element & Navigation System Implementation Guide

## Overview

This document outlines the implementation of a Link element for Kryon that enables seamless navigation between `.kry` and `.krb` files, external URLs, and supports custom overlay components for enhanced navigation experiences.

## Core Features

### 1. Link Element Syntax

```kry
// Basic navigation
Link {
    to: "examples/button.kry"         // Auto-compiles .kry on-the-fly
    text: "Button Example"
}

Link {
    to: "compiled/dashboard.krb"      // Direct .krb loading (faster)  
    text: "Dashboard"
}

// External URLs
Link {
    to: "https://github.com/kryon"
    text: "Kryon GitHub"
    external: true                    // Opens in system browser
}

// With custom overlay
Link {
    to: "settings.kry"
    text: "Settings"
    overlay: BackButton {
        position: "top-left"
        onClick: "navigateBack"
    }
}
```

### 2. Component Inheritance System

```kry
// Custom component extending base element
@component BackButton(position: "top-left") extends Button {
    // Inherits all Button properties: text, backgroundColor, onClick, etc.
    text: "← Back"
    width: 80
    height: 35
    backgroundColor: "#00000080"  // Semi-transparent
    textColor: "#FFFFFFFF"
    borderRadius: 5
    zIndex: 1000                  // Overlay behavior
    position: $position           // Custom positioning
    
    @function "lua" init() {
        -- Position button based on position property
        if self.position == "top-left" then
            self.x = 10
            self.y = 10
        elseif self.position == "top-right" then
            self.x = window.width - self.width - 10
            self.y = 10
        elseif self.position == "bottom-left" then
            self.x = 10
            self.y = window.height - self.height - 10
        elseif self.position == "bottom-right" then
            self.x = window.width - self.width - 10
            self.y = window.height - self.height - 10
        end
    end
}
```

### 3. Smart Directory Navigation

```kry
@function "lua" loadExampleFiles() {
    local examples = {}
    local files = kryon.fs.readdir("./")
    
    for _, file in ipairs(files) do
        if file:match("%.kry$") and file ~= "index.kry" then
            local name = file:gsub("%.kry$", "")
            local title = name:gsub("_", " "):gsub("([^%w])(%w)", function(sep, letter)
                return sep .. letter:upper()
            end)
            title = title:gsub("^%w", string.upper)
            table.insert(examples, {file = file, title = title})
        end
    end
    
    table.sort(examples, function(a, b) return a.title < b.title end)
    return examples
}
```

## Implementation Roadmap

### Phase 1: Core Link Element Structure

#### 1.1 Element Implementation
- **File**: `src/runtime/elements/link.c`
- **Properties**: `to`, `text`, `external`, `overlay`
- **Styling**: All standard text/container properties
- **Click handling**: Navigation logic

#### 1.2 Element Registration  
- Add `KRYON_ELEMENT_LINK` to element enum
- Register in `src/runtime/elements.c`
- Add to compiler mappings in `src/shared/kryon_mappings.c`

#### 1.3 Basic Structure
```c
// Forward declarations
static void link_render(KryonRuntime* runtime, KryonElement* element, 
                        KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static bool link_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
static void link_destroy(KryonRuntime* runtime, KryonElement* element);

// VTable for Link element
static const ElementVTable g_link_vtable = {
    .render = link_render,
    .handle_event = link_handle_event,
    .destroy = link_destroy
};

// Registration function
bool register_link_element(void) {
    return element_register_type("Link", &g_link_vtable);
}
```

### Phase 2: Navigation System

#### 2.1 Navigation Manager
- **Files**: 
  - `src/runtime/navigation/navigation.h`
  - `src/runtime/navigation/navigation.c`
- **Functions**: 
  - `kryon_navigate_to(const char* path)`
  - `kryon_navigation_back()`
  - `kryon_navigation_history_push(const char* path)`

#### 2.2 Navigation Logic
```c
typedef struct {
    char* path;
    struct NavigationHistoryItem* next;
} NavigationHistoryItem;

typedef struct {
    NavigationHistoryItem* history;
    size_t history_count;
    size_t max_history;
} NavigationManager;

// Core navigation function
KryonResult kryon_navigate_to(KryonRuntime* runtime, const char* path) {
    if (ends_with(path, ".krb")) {
        // Direct KRB loading
        return kryon_load_krb_file(runtime, path);
    } else if (ends_with(path, ".kry")) {
        // On-the-fly compilation
        char* compiled_path = compile_kry_to_temp_krb(path);
        KryonResult result = kryon_load_krb_file(runtime, compiled_path);
        // Cache compiled file for performance
        return result;
    } else if (is_url(path)) {
        // External browser
        return open_external_url(path);
    }
    return KRYON_ERROR_INVALID_PATH;
}
```

### Phase 3: Component Inheritance System

#### 3.1 Parser Extension
- Extend `@component` syntax to support `extends BaseElement`
- Parse inheritance hierarchy in compiler
- Property resolution from base element

#### 3.2 Inheritance Syntax
```kry
@component ComponentName(param1: default1, param2: default2) extends BaseElement {
    // Inherits all BaseElement properties
    // Can override properties
    inherited_property: "new_value"
    
    // Can add new properties
    custom_property: "custom_value"
    
    // Can define functions
    @function "lua" customFunction() {
        -- Custom logic
    end
    
    // Component UI definition
    BaseElement {
        // Use inherited and custom properties
        text: $inherited_property
        customProp: $custom_property
    }
}
```

#### 3.3 Property Resolution
1. Check component's custom properties
2. Check inherited base element properties  
3. Use default values if not found

### Phase 4: On-the-fly Compilation

#### 4.1 Runtime Compiler Integration
- **File**: `src/runtime/compilation/runtime_compiler.c`
- Include compiler functions in runtime
- Memory-based compilation for performance

#### 4.2 Compilation Cache
```c
typedef struct {
    char* kry_path;
    char* krb_path;
    time_t kry_mtime;
    time_t krb_mtime;
} CompilationCacheEntry;

// Cache management
KryonResult compile_kry_to_temp_krb(const char* kry_path, char** output_krb_path) {
    // Check cache first
    CompilationCacheEntry* cached = find_cached_compilation(kry_path);
    if (cached && cached->krb_mtime >= cached->kry_mtime) {
        *output_krb_path = kryon_strdup(cached->krb_path);
        return KRYON_SUCCESS;
    }
    
    // Compile to temporary file
    char temp_path[512];
    snprintf(temp_path, sizeof(temp_path), "/tmp/kryon_cache/%s.krb", 
             basename_without_ext(kry_path));
    
    KryonResult result = kryon_compile_file(kry_path, temp_path);
    if (result == KRYON_SUCCESS) {
        add_to_compilation_cache(kry_path, temp_path);
        *output_krb_path = kryon_strdup(temp_path);
    }
    
    return result;
}
```

### Phase 5: Overlay System

#### 5.1 Overlay Injection
Link element checks for `overlay` property and renders child component:

```c
static void link_render(KryonRuntime* runtime, KryonElement* element, 
                        KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    // 1. Render link text with styling
    render_link_text(runtime, element, commands, command_count, max_commands);
    
    // 2. Handle overlay injection
    KryonElement* overlay = get_element_property_element(element, "overlay");
    if (overlay) {
        // Render overlay component with high z-index
        overlay->z_index = 1000;  // Ensure overlay behavior
        render_element(runtime, overlay, commands, command_count, max_commands);
    }
}
```

#### 5.2 Overlay Positioning
Custom components can define positioning logic:

```c
// In BackButton component init function
@function "lua" positionOverlay() {
    local pos = self.position or "top-left"
    local margin = 10
    
    if pos == "top-left" then
        self.x = margin
        self.y = margin
    elseif pos == "top-right" then
        self.x = window.width - self.width - margin
        self.y = margin
    -- ... other positions
    end
end
```

### Phase 6: External URL Handling

#### 6.1 Platform-specific Browser Opening
```c
#ifdef __linux__
    char command[512];
    snprintf(command, sizeof(command), "xdg-open '%s'", url);
    system(command);
#elif __APPLE__
    char command[512];
    snprintf(command, sizeof(command), "open '%s'", url);
    system(command);
#elif _WIN32
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
#endif
```

#### 6.2 URL Validation
```c
bool is_valid_url(const char* path) {
    return (strncmp(path, "http://", 7) == 0 || 
            strncmp(path, "https://", 8) == 0 ||
            strncmp(path, "mailto:", 7) == 0 ||
            strncmp(path, "file://", 7) == 0);
}
```

## File Structure

### New Files
```
src/runtime/elements/link.c                 - Link element implementation
src/runtime/navigation/navigation.h         - Navigation interface
src/runtime/navigation/navigation.c         - Navigation manager
src/runtime/compilation/runtime_compiler.c  - On-the-fly compilation
examples/index.kry                          - Smart example browser
```

### Modified Files
```
src/runtime/elements.c                      - Register Link element
src/runtime/CMakeLists.txt                  - Add new source files
src/shared/kryon_mappings.c                 - Add Link to compiler mappings
src/compiler/parser/parser.c                - Component inheritance syntax
```

## Development Order

1. **Link Element Core** - Basic rendering and click handling
2. **Simple Navigation** - Direct .krb loading
3. **Component Inheritance** - Extends syntax and property resolution
4. **On-the-fly Compilation** - .kry → .krb compilation
5. **Overlay System** - Custom component injection
6. **External URLs** - Browser integration
7. **Smart Index** - Directory discovery and navigation
8. **Polish & Testing** - Error handling, performance optimization

## Example Applications

### Example Browser (index.kry)
A smart file browser that automatically discovers and presents all `.kry` examples in a directory with navigation overlays.

### Documentation Navigator
Link between different documentation pages with contextual back buttons.

### Multi-screen Applications
Navigate between different application screens with transition overlays and navigation history.

## Benefits

1. **Native Navigation** - Stay within Kryon ecosystem
2. **Developer Productivity** - Live compilation during development
3. **Performance** - Direct .krb loading for production
4. **Flexibility** - Custom overlay components for rich UX
5. **Component Reuse** - Inheritance system for shared functionality
6. **Smart Discovery** - Automatic file system integration

This Link element system transforms Kryon from a UI toolkit into a complete application framework with powerful navigation capabilities.