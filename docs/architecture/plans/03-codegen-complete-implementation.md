# Complete Codegen Implementation for All Frontends

## Executive Summary

Implement full code generation from KIR to ALL frontend languages (Nim, Lua, C, TSX, HTML) with complete event handler, reactive state, and logic support.

## Current State

### Codegen Coverage Analysis

```
┌──────────┬───────────┬──────────┬──────────┬────────────┬────────┐
│ Language │ Component │ Events   │ State    │ Styling    │ Status │
├──────────┼───────────┼──────────┼──────────┼────────────┼────────┤
│ HTML/CSS │ ✅ 70%    │ ⚠️ Stubs │ ❌ None  │ ✅ Complete│ Good   │
│ TSX/JSX  │ ✅ 60%    │ ⚠️ Basic │ ⚠️ Basic │ ⚠️ Inline  │ Medium │
│ Nim      │ ⚠️ 20%    │ ❌ None  │ ❌ None  │ ⚠️ Basic   │ Poor   │
│ Lua      │ ⚠️ 30%    │ ❌ None  │ ❌ None  │ ⚠️ Basic   │ Poor   │
│ C        │ ⚠️ 10%    │ ❌ None  │ ❌ None  │ ⚠️ Metadata│ Stub   │
│ Rust     │ ❌ None   │ ❌ None  │ ❌ None  │ ❌ None    │ None   │
└──────────┴───────────┴──────────┴──────────┴────────────┴────────┘
```

### What Works

#### HTML/CSS Codegen ✅ (Most Complete)

**Location:** `/mnt/storage/Projects/kryon/codegens/web/`

**Files:**
- `html_generator.c` (748 lines) - Generates HTML structure
- `css_generator.c` (complete) - Generates CSS styles
- `ir_web_renderer.c` - JavaScript runtime

**Example:**
```json
// KIR input
{
  "type": "Button",
  "text": "Click",
  "background": "#404080"
}
```

```html
<!-- Generated HTML -->
<button class="kryon-button-1" style="background: #404080">
  Click
</button>
```

```css
/* Generated CSS */
.kryon-button-1 {
  background: #404080;
  /* ... other properties */
}
```

**Missing:**
- Event handler generation (TODO in code)
- Reactive state bindings
- WASM bridge integration

#### TSX/JSX Codegen ⚠️ (Partial)

**Location:** `/mnt/storage/Projects/kryon/codegens/tsx/tsx_codegen.c`

**Generates:**
```tsx
// From KIR
import React from 'react';

function App() {
  return (
    <Container>
      <Button text="Click" />
    </Container>
  );
}
```

**Missing:**
- Event handlers (onClick, onHover)
- useState() declarations
- useEffect() for side effects
- Proper TypeScript types

### What's Broken

#### Nim Codegen ❌ (20% Coverage)

**Location:** `/mnt/storage/Projects/kryon/codegens/nim/nim_codegen.c` (239 lines)

**Current Output:**
```nim
import kryon_dsl

let app = kryonApp:
  Container:
    background = "#191970"

    Button:
      text = "Click Me!"
      # ❌ Missing: onClick handler
      # ❌ Missing: reactive state
```

**Problems:**
- Only generates basic properties (6 types)
- No event handlers
- No reactive state
- No proc definitions
- Limited component types

#### Lua Codegen ❌ (30% Coverage)

**Location:** `/mnt/storage/Projects/kryon/codegens/lua/lua_codegen.c` (202 lines)

**Current Output:**
```lua
local UI = require("kryon.dsl")
local Reactive = require("kryon.reactive")

local root = UI.Container {
  local root_child0 = UI.Button {
    text = "Click Me!",
    color = "pink",
    -- ❌ Missing: onClick handler
  }
  root_child0,
}

return root
```

**Problems:**
- No event handlers (function() ... end)
- No Reactive.state() declarations
- No state bindings to UI

#### C Codegen ❌ (10% Coverage - Stub)

**Location:** `/mnt/storage/Projects/kryon/codegens/c/ir_c_codegen.c`

**Desired Output:**
```c
#include "kryon.h"

static int count = 0;

void on_click_handler(kryon_event_t* evt) {
    count++;
    kryon_invalidate();  // Trigger re-render
}

void create_ui(void) {
    kryon_container_t* root = kryon_container_new();

    kryon_button_t* btn = kryon_button_new();
    kryon_button_set_text(btn, "Click");
    kryon_event_bind(btn, KRYON_EVT_CLICK, on_click_handler);

    kryon_container_add_child(root, btn);
    kryon_set_root(root);
}
```

**Current:** Barely implemented stub.

## Complete Codegen Architecture

### Unified Codegen Pipeline

```
KIR JSON
    ↓
┌───────────────────────────────────────────┐
│ KIR Parser & Validator                    │
│ - Parse JSON to IR structures             │
│ - Validate component tree                 │
│ - Extract logic blocks                    │
│ - Extract reactive manifest               │
└───────────────────────────────────────────┘
    ↓
┌───────────────────────────────────────────┐
│ Language-Specific Code Generator          │
│                                           │
│ 1. Generate imports/requires              │
│ 2. Generate reactive state declarations   │
│ 3. Generate event handler functions       │
│ 4. Generate component tree                │
│ 5. Generate state→UI bindings             │
│ 6. Generate entry point/main              │
└───────────────────────────────────────────┘
    ↓
Target Language Source Code
```

## Implementation Plan

### Phase 1: Complete Nim Codegen

**Goal:** Generate complete, idiomatic Nim DSL code from KIR

**File:** `/mnt/storage/Projects/kryon/codegens/nim/nim_codegen.c`

**Implementation:**

```c
bool nim_codegen_generate(const char* kir_path, const char* output_path) {
    // 1. Parse KIR JSON
    char* kir_json = read_file(kir_path);
    KIRDocument* doc = parse_kir_v3(kir_json);

    FILE* out = fopen(output_path, "w");

    // 2. Generate imports
    fprintf(out, "## Generated from KIR\n");
    fprintf(out, "## Kryon Nim DSL Code Generator\n\n");
    fprintf(out, "import kryon_dsl\n");
    if (doc->reactive_manifest && doc->reactive_manifest->state_count > 0) {
        fprintf(out, "import kryon_reactive\n");
    }
    fprintf(out, "\n");

    // 3. Generate reactive state declarations
    if (doc->reactive_manifest) {
        nim_generate_reactive_state(out, doc->reactive_manifest);
    }

    // 4. Generate event handler procs
    if (doc->logic_manifest) {
        nim_generate_event_handlers(out, doc->logic_manifest);
    }

    // 5. Generate component tree
    fprintf(out, "# Application\n");
    fprintf(out, "let app = kryonApp:\n");
    nim_generate_component_tree(out, doc->root_component, 1);

    // 6. Generate main
    fprintf(out, "\nwhen isMainModule:\n");
    fprintf(out, "  app.run()\n");

    fclose(out);
    free(kir_json);
    return true;
}

static void nim_generate_reactive_state(FILE* out, IRReactiveManifest* manifest) {
    fprintf(out, "# Reactive State\n");

    for (size_t i = 0; i < manifest->state_count; i++) {
        IRReactiveState* state = &manifest->states[i];

        // Generate: var count = reactive(0)
        fprintf(out, "var %s = reactive[%s](%s)\n",
                state->name,
                state->type,
                state->initial_value);
    }

    fprintf(out, "\n");
}

static void nim_generate_event_handlers(FILE* out, IRLogicManifest* logic) {
    fprintf(out, "# Event Handlers\n");

    IRLogicBlock* block = logic->blocks;
    while (block) {
        if (block->type == LOGIC_EVENT_HANDLER) {
            if (strcmp(block->language, "nim") == 0) {
                // Source is already Nim - emit as-is
                fprintf(out, "%s\n\n", block->source_code);
            } else {
                // Need to translate from another language (e.g., TypeScript → Nim)
                char* nim_code = translate_logic_to_nim(block);
                fprintf(out, "%s\n\n", nim_code);
                free(nim_code);
            }
        }
        block = block->next;
    }

    fprintf(out, "\n");
}

static void nim_generate_component_tree(FILE* out, IRComponent* comp, int indent) {
    // Indentation
    for (int i = 0; i < indent; i++) fprintf(out, "  ");

    // Component type
    fprintf(out, "%s:\n", ir_component_type_to_string(comp->type));

    // Properties
    nim_generate_properties(out, comp, indent + 1);

    // Event bindings
    if (comp->event_bindings) {
        nim_generate_event_bindings(out, comp->event_bindings, indent + 1);
    }

    // Children
    if (comp->children) {
        fprintf(out, "\n");
        for (size_t i = 0; i < comp->child_count; i++) {
            nim_generate_component_tree(out, comp->children[i], indent + 1);
        }
    }
}

static void nim_generate_properties(FILE* out, IRComponent* comp, int indent) {
    IRStyle* style = comp->style;
    if (!style) return;

    // Generate each property
    if (style->background) {
        for (int i = 0; i < indent; i++) fprintf(out, "  ");
        fprintf(out, "background = \"%s\"\n", style->background);
    }

    if (style->color) {
        for (int i = 0; i < indent; i++) fprintf(out, "  ");
        fprintf(out, "color = \"%s\"\n", style->color);
    }

    // ... all other properties
}

static void nim_generate_event_bindings(FILE* out, IREventBindings* bindings, int indent) {
    IREventBinding* binding = bindings->bindings;
    while (binding) {
        for (int i = 0; i < indent; i++) fprintf(out, "  ");

        // Generate: onClick = handleClick
        fprintf(out, "%s = %s\n",
                binding->event_name,
                binding->logic_block_id); // Logic block should have been named properly

        binding = binding->next;
    }
}

// Logic translation engine
static char* translate_logic_to_nim(IRLogicBlock* block) {
    // Convert TypeScript/C/Lua code to Nim
    if (strcmp(block->language, "typescript") == 0) {
        return translate_typescript_to_nim(block->source_code);
    } else if (strcmp(block->language, "c") == 0) {
        return translate_c_to_nim(block->source_code);
    } else if (strcmp(block->language, "lua") == 0) {
        return translate_lua_to_nim(block->source_code);
    }
    return strdup(block->source_code); // Fallback: use as-is
}

static char* translate_typescript_to_nim(const char* ts_code) {
    // Example: "() => setCount(count + 1)" → "proc(): void = count.set(count.get() + 1)"

    // Simple regex-based translation for now
    // TODO: Use proper AST transformation

    char* nim_code = malloc(strlen(ts_code) * 2);

    // Pattern: () => expr
    if (strstr(ts_code, "() =>")) {
        sprintf(nim_code, "proc(): void =\n  %s",
                extract_expression_after_arrow(ts_code));
    } else {
        strcpy(nim_code, ts_code);
    }

    return nim_code;
}
```

**Expected Output:**

```nim
## Generated from KIR
## Kryon Nim DSL Code Generator

import kryon_dsl
import kryon_reactive

# Reactive State
var count = reactive[int](0)

# Event Handlers
proc handleClick(): void =
  count.set(count.get() + 1)

# Application
let app = kryonApp:
  Center:
    Column:
      gap = 16

      Text:
        text = "Count: " & $count.get()

      Button:
        text = "Increment"
        onClick = handleClick

when isMainModule:
  app.run()
```

### Phase 2: Complete Lua Codegen

**File:** `/mnt/storage/Projects/kryon/codegens/lua/lua_codegen.c`

```c
bool lua_codegen_generate(const char* kir_path, const char* output_path) {
    char* kir_json = read_file(kir_path);
    KIRDocument* doc = parse_kir_v3(kir_json);

    FILE* out = fopen(output_path, "w");

    // 1. Header
    fprintf(out, "-- Generated from KIR by Kryon Code Generator\n");
    fprintf(out, "local Reactive = require(\"kryon.reactive\")\n");
    fprintf(out, "local UI = require(\"kryon.dsl\")\n\n");

    // 2. Reactive state
    if (doc->reactive_manifest) {
        lua_generate_reactive_state(out, doc->reactive_manifest);
    }

    // 3. Event handlers
    if (doc->logic_manifest) {
        lua_generate_event_handlers(out, doc->logic_manifest);
    }

    // 4. UI tree
    fprintf(out, "-- UI Component Tree\n");
    fprintf(out, "local root = ");
    lua_generate_component_tree(out, doc->root_component, 0);

    // 5. Return
    fprintf(out, "\nreturn root\n");

    fclose(out);
    free(kir_json);
    return true;
}

static void lua_generate_reactive_state(FILE* out, IRReactiveManifest* manifest) {
    fprintf(out, "-- Reactive State\n");

    for (size_t i = 0; i < manifest->state_count; i++) {
        IRReactiveState* state = &manifest->states[i];

        // Generate: local count = Reactive.state(0)
        fprintf(out, "local %s = Reactive.state(%s)\n",
                state->name,
                state->initial_value);
    }

    fprintf(out, "\n");
}

static void lua_generate_event_handlers(FILE* out, IRLogicManifest* logic) {
    fprintf(out, "-- Event Handlers\n");

    IRLogicBlock* block = logic->blocks;
    while (block) {
        if (block->type == LOGIC_EVENT_HANDLER) {
            if (strcmp(block->language, "lua") == 0) {
                fprintf(out, "%s\n\n", block->source_code);
            } else {
                char* lua_code = translate_logic_to_lua(block);
                fprintf(out, "%s\n\n", lua_code);
                free(lua_code);
            }
        }
        block = block->next;
    }

    fprintf(out, "\n");
}

static void lua_generate_component_tree(FILE* out, IRComponent* comp, int indent) {
    // Generate: UI.Container { properties, children }
    fprintf(out, "UI.%s {\n", ir_component_type_to_string(comp->type));

    // Properties
    lua_generate_properties(out, comp, indent + 1);

    // Event handlers
    if (comp->event_bindings) {
        lua_generate_event_bindings(out, comp->event_bindings, indent + 1);
    }

    // Children
    if (comp->children && comp->child_count > 0) {
        for (int i = 0; i < indent + 1; i++) fprintf(out, "  ");
        fprintf(out, "\n");

        for (size_t i = 0; i < comp->child_count; i++) {
            for (int j = 0; j < indent + 1; j++) fprintf(out, "  ");
            lua_generate_component_tree(out, comp->children[i], indent + 1);
            fprintf(out, ",\n");
        }
    }

    for (int i = 0; i < indent; i++) fprintf(out, "  ");
    fprintf(out, "}");
}

static void lua_generate_properties(FILE* out, IRComponent* comp, int indent) {
    IRStyle* style = comp->style;
    if (!style) return;

    if (style->background) {
        for (int i = 0; i < indent; i++) fprintf(out, "  ");
        fprintf(out, "background = \"%s\",\n", style->background);
    }

    if (style->color) {
        for (int i = 0; i < indent; i++) fprintf(out, "  ");
        fprintf(out, "color = \"%s\",\n", style->color);
    }

    // ... all properties
}

static void lua_generate_event_bindings(FILE* out, IREventBindings* bindings, int indent) {
    IREventBinding* binding = bindings->bindings;
    while (binding) {
        for (int i = 0; i < indent; i++) fprintf(out, "  ");

        // Generate: onClick = function() ... end
        fprintf(out, "%s = %s,\n",
                binding->event_name,
                binding->logic_block_id);

        binding = binding->next;
    }
}
```

**Expected Output:**

```lua
-- Generated from KIR by Kryon Code Generator
local Reactive = require("kryon.reactive")
local UI = require("kryon.dsl")

-- Reactive State
local count = Reactive.state(0)

-- Event Handlers
local function handleClick()
  count:set(count:get() + 1)
end

-- UI Component Tree
local root = UI.Container {
  UI.Column {
    gap = 16,

    UI.Text {
      text = "Count: " .. count:get(),
    },

    UI.Button {
      text = "Increment",
      onClick = handleClick,
    },
  },
}

return root
```

### Phase 3: Complete C Codegen

**File:** `/mnt/storage/Projects/kryon/codegens/c/ir_c_codegen.c`

```c
bool c_codegen_generate(const char* kir_path, const char* output_path) {
    char* kir_json = read_file(kir_path);
    KIRDocument* doc = parse_kir_v3(kir_json);

    FILE* out = fopen(output_path, "w");

    // 1. Includes
    fprintf(out, "// Generated from KIR\n");
    fprintf(out, "#include \"kryon.h\"\n");
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <stdlib.h>\n\n");

    // 2. Global state variables
    if (doc->reactive_manifest) {
        c_generate_state_variables(out, doc->reactive_manifest);
    }

    // 3. Event handler functions
    if (doc->logic_manifest) {
        c_generate_event_handlers(out, doc->logic_manifest);
    }

    // 4. UI creation function
    fprintf(out, "void create_ui(void) {\n");
    c_generate_component_tree(out, doc->root_component, 1);
    fprintf(out, "}\n\n");

    // 5. Main function
    fprintf(out, "int main(void) {\n");
    fprintf(out, "    kryon_init();\n");
    fprintf(out, "    create_ui();\n");
    fprintf(out, "    kryon_run();\n");
    fprintf(out, "    kryon_shutdown();\n");
    fprintf(out, "    return 0;\n");
    fprintf(out, "}\n");

    fclose(out);
    free(kir_json);
    return true;
}

static void c_generate_state_variables(FILE* out, IRReactiveManifest* manifest) {
    fprintf(out, "// Reactive State\n");

    for (size_t i = 0; i < manifest->state_count; i++) {
        IRReactiveState* state = &manifest->states[i];

        // Generate: static int count = 0;
        fprintf(out, "static %s %s = %s;\n",
                state->type,
                state->name,
                state->initial_value);
    }

    fprintf(out, "\n");
}

static void c_generate_event_handlers(FILE* out, IRLogicManifest* logic) {
    fprintf(out, "// Event Handlers\n");

    IRLogicBlock* block = logic->blocks;
    while (block) {
        if (block->type == LOGIC_EVENT_HANDLER) {
            // Generate function definition
            fprintf(out, "void %s_handler(kryon_event_t* evt) {\n",
                    sanitize_function_name(block->id));

            if (strcmp(block->language, "c") == 0) {
                // Source is C - emit as-is
                fprintf(out, "    %s\n", block->source_code);
            } else {
                // Translate to C
                char* c_code = translate_logic_to_c(block);
                fprintf(out, "    %s\n", c_code);
                free(c_code);
            }

            fprintf(out, "    kryon_invalidate();  // Trigger re-render\n");
            fprintf(out, "}\n\n");
        }
        block = block->next;
    }

    fprintf(out, "\n");
}

static void c_generate_component_tree(FILE* out, IRComponent* comp, int indent) {
    char comp_var[64];
    snprintf(comp_var, sizeof(comp_var), "comp_%d", comp->id);

    // Create component
    for (int i = 0; i < indent; i++) fprintf(out, "    ");
    fprintf(out, "kryon_component_t* %s = kryon_%s_new();\n",
            comp_var,
            ir_component_type_to_lowercase(comp->type));

    // Set properties
    c_generate_property_calls(out, comp, comp_var, indent);

    // Bind events
    if (comp->event_bindings) {
        c_generate_event_binding_calls(out, comp, comp_var, indent);
    }

    // Add children
    if (comp->children) {
        for (size_t i = 0; i < comp->child_count; i++) {
            c_generate_component_tree(out, comp->children[i], indent);

            char child_var[64];
            snprintf(child_var, sizeof(child_var), "comp_%d", comp->children[i]->id);

            for (int j = 0; j < indent; j++) fprintf(out, "    ");
            fprintf(out, "kryon_container_add_child(%s, %s);\n", comp_var, child_var);
        }
    }
}

static void c_generate_event_binding_calls(FILE* out, IRComponent* comp, const char* comp_var, int indent) {
    IREventBinding* binding = comp->event_bindings->bindings;
    while (binding) {
        for (int i = 0; i < indent; i++) fprintf(out, "    ");

        fprintf(out, "kryon_event_bind(%s, KRYON_EVT_%s, %s_handler);\n",
                comp_var,
                event_name_to_enum(binding->event_name),
                binding->logic_block_id);

        binding = binding->next;
    }
}
```

**Expected Output:**

```c
// Generated from KIR
#include "kryon.h"
#include <stdio.h>
#include <stdlib.h>

// Reactive State
static int count = 0;

// Event Handlers
void logic_1_handler(kryon_event_t* evt) {
    count++;
    kryon_invalidate();
}

void create_ui(void) {
    kryon_component_t* comp_1 = kryon_container_new();

    kryon_component_t* comp_2 = kryon_button_new();
    kryon_button_set_text(comp_2, "Increment");
    kryon_event_bind(comp_2, KRYON_EVT_CLICK, logic_1_handler);

    kryon_container_add_child(comp_1, comp_2);
    kryon_set_root(comp_1);
}

int main(void) {
    kryon_init();
    create_ui();
    kryon_run();
    kryon_shutdown();
    return 0;
}
```

### Phase 4: Complete TSX Codegen

**File:** `/mnt/storage/Projects/kryon/codegens/tsx/tsx_codegen.c`

**(Already partially implemented - needs event handlers and useState)**

```c
// Add to existing implementation:

static void tsx_generate_react_hooks(FILE* out, IRReactiveManifest* manifest) {
    if (!manifest || manifest->state_count == 0) return;

    fprintf(out, "  // State\n");

    for (size_t i = 0; i < manifest->state_count; i++) {
        IRReactiveState* state = &manifest->states[i];

        // Generate: const [count, setCount] = useState(0);
        fprintf(out, "  const [%s, set%s] = useState<%s>(%s);\n",
                state->name,
                capitalize(state->name),
                state->type,
                state->initial_value);
    }

    fprintf(out, "\n");
}

static void tsx_generate_event_handlers(FILE* out, IRLogicManifest* logic) {
    if (!logic) return;

    fprintf(out, "  // Event Handlers\n");

    IRLogicBlock* block = logic->blocks;
    while (block) {
        if (block->type == LOGIC_EVENT_HANDLER) {
            if (strcmp(block->language, "typescript") == 0) {
                // Already TypeScript
                fprintf(out, "  const %s = %s;\n",
                        sanitize_handler_name(block->id),
                        block->source_code);
            } else {
                // Translate to TypeScript
                char* ts_code = translate_logic_to_typescript(block);
                fprintf(out, "  const %s = %s;\n",
                        sanitize_handler_name(block->id),
                        ts_code);
                free(ts_code);
            }
        }
        block = block->next;
    }

    fprintf(out, "\n");
}
```

### Phase 5: HTML/CSS Event Handler Generation

**File:** `/mnt/storage/Projects/kryon/codegens/web/html_generator.c`

**Currently has TODO at line 748:**

```c
// TODO: Generate event attributes (onclick, etc.)
if (comp->event_bindings) {
    IREventBinding* binding = comp->event_bindings->bindings;
    while (binding) {
        // Generate: onclick="handleClick()"
        fprintf(out, " %s=\"%s()\"",
                event_name_to_html_attr(binding->event_name),
                binding->logic_block_id);

        binding = binding->next;
    }
}
```

**Also generate JavaScript:**

**File:** `/mnt/storage/Projects/kryon/codegens/web/js_generator.c` (NEW)

```c
void js_generate_script(FILE* out, IRLogicManifest* logic, IRReactiveManifest* manifest) {
    fprintf(out, "<script>\n");

    // State variables
    if (manifest) {
        for (size_t i = 0; i < manifest->state_count; i++) {
            fprintf(out, "let %s = %s;\n",
                    manifest->states[i].name,
                    manifest->states[i].initial_value);
        }
    }

    // Event handlers
    if (logic) {
        IRLogicBlock* block = logic->blocks;
        while (block) {
            if (block->type == LOGIC_EVENT_HANDLER) {
                fprintf(out, "function %s() {\n", block->id);
                fprintf(out, "  %s\n", translate_to_javascript(block));
                fprintf(out, "}\n\n");
            }
            block = block->next;
        }
    }

    fprintf(out, "</script>\n");
}
```

## Testing Strategy

### Codegen Round-Trip Tests

For each language:

```bash
# Test 1: KIR → Lang → Compile → Run
kryon codegen nim button.kir -o button.nim
nim c -r button.nim  # Should run without errors

# Test 2: Event handlers work
# Click button → state updates → UI re-renders

# Test 3: Complex example
kryon codegen tsx counter_app.kir -o app.tsx
npm run dev  # Should work with React
```

### Visual Equivalence Tests

```bash
# Same KIR should produce equivalent UIs in all languages
KRYON_SCREENSHOT=/tmp/nim.png kryon run button.nim
KRYON_SCREENSHOT=/tmp/lua.png kryon run button.lua
KRYON_SCREENSHOT=/tmp/tsx.png kryon run button.tsx
KRYON_SCREENSHOT=/tmp/html.png kryon run button.html

identify /tmp/*.png  # Should all be visually identical
```

## Files to Modify/Create

```
codegens/nim/nim_codegen.c ............ MODIFY: Complete implementation
codegens/lua/lua_codegen.c ............ MODIFY: Complete implementation
codegens/c/ir_c_codegen.c ............. MODIFY: Complete implementation
codegens/tsx/tsx_codegen.c ............ MODIFY: Add hooks & handlers
codegens/web/html_generator.c ......... MODIFY: Add event attributes
codegens/web/js_generator.c ........... CREATE: JavaScript generation
codegens/common/logic_translator.c .... CREATE: Cross-language logic translation
codegens/common/logic_translator.h .... CREATE: Translation API
```

## Dependencies

- KIR parsing (Phase 1 of KIR spec)
- Logic block structures
- Reactive manifest structures

## Estimated Effort

- Phase 1 (Nim): 5 days
- Phase 2 (Lua): 4 days
- Phase 3 (C): 5 days
- Phase 4 (TSX): 2 days
- Phase 5 (HTML/JS): 3 days
- Logic Translator: 4 days
- Testing: 3 days

**Total: ~26 days (5 weeks)**

## Success Criteria

✅ All languages generate complete, runnable code from KIR
✅ Event handlers correctly translated between languages
✅ Reactive state properly declared and bound
✅ Generated code is idiomatic for each language
✅ Round-trip conversion preserves semantics
✅ All 50+ examples work across all languages
