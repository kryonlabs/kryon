# Complete Lua Implementation Plan

## Executive Summary

This plan covers the full implementation of Lua support in Kryon, including:
1. **Lua Parser** - Convert Lua DSL syntax to KIR
2. **Lua Codegen** - Generate idiomatic Lua DSL from KIR
3. **Full round-trip capability** - Lua → KIR → Lua (preserving semantics)

## Current Lua Ecosystem

### Existing Lua DSL (bindings/lua/kryon/dsl.lua)

**Metaprogramming Pattern:**
- Uses `SmartComponent()` factory that returns a function
- Functions accept tables where:
  - String keys = properties (e.g., `backgroundColor = "#1a1a1a"`)
  - Numeric keys (array part) = children (e.g., `Button { ... }`)
- Enables Nim-like nested syntax without special operators

**Example Syntax:**
```lua
local UI = require("kryon.dsl")
local Reactive = require("kryon.reactive")

local count = Reactive.state(0)

local root = UI.Column {
  backgroundColor = "#1a1a1a",
  padding = 20,
  gap = 10,

  UI.Text {
    text = "Count: " .. count:get(),
    fontSize = 24,
    color = "#ffffff"
  },

  UI.Button {
    text = "Increment",
    backgroundColor = "#404080",
    onClick = function()
      count:set(count:get() + 1)
    end
  }
}

return { root = root }
```

### Reactive System (bindings/lua/kryon/reactive.lua)

**API:**
```lua
-- Create reactive state
local count = Reactive.state(0)

-- Get/set values
local value = count:get()
count:set(10)
count:update(function(old) return old + 1 end)

-- Computed values (derived state)
local doubled = Reactive.computed({count}, function()
  return count:get() * 2
end)

-- Reactive arrays
local items = Reactive.array({"item1", "item2"})
items:push("item3")
items:remove(2)

-- Effects (run when dependencies change)
Reactive.effect({count}, function()
  print("Count changed:", count:get())
end)

-- Batch updates
Reactive.batch(function()
  count:set(5)
  count:set(10)
  -- Only one notification sent
end)
```

## Part 1: Lua Parser Implementation

### Overview

Parse Lua DSL syntax into complete KIR with:
- Component hierarchy
- Properties and styling
- Event handlers as source code
- Reactive state declarations
- Computed values

### Architecture

```
Lua Source Code
    ↓
┌──────────────────────────────────┐
│ Lua AST Parser                   │
│ - Use Lua's built-in load()     │
│ - Parse AST via debug.getinfo()  │
│ - OR: Use LuaJIT parser API      │
│ - Extract function bodies        │
└──────────────────────────────────┘
    ↓
┌──────────────────────────────────┐
│ DSL Call Interceptor             │
│ - Mock UI.Column, UI.Button, etc│
│ - Capture component tree         │
│ - Record property assignments    │
│ - Extract event handler code     │
└──────────────────────────────────┘
    ↓
┌──────────────────────────────────┐
│ KIR Generator                    │
│ - Build IRComponent tree         │
│ - Create IRLogicBlock for events │
│ - Create IRReactiveManifest      │
│ - Add source metadata            │
└──────────────────────────────────┘
    ↓
Complete KIR JSON
```

### Implementation Strategy

**Option A: Execution-Based Parser (Recommended)**

Parse by executing Lua code with mocked DSL:

```lua
-- ir/parsers/lua/lua_to_kir.lua

local Parser = {}

-- Mock DSL that captures calls instead of creating real components
local function createMockDSL()
  local components = {}
  local nextId = 1
  local reactiveStates = {}
  local logicBlocks = {}

  local MockUI = {}
  local MockReactive = {}

  -- Mock Reactive.state()
  function MockReactive.state(initialValue)
    local stateId = "state_" .. nextId
    nextId = nextId + 1

    table.insert(reactiveStates, {
      id = stateId,
      name = debug.getlocal(2, 1), -- Get variable name from caller
      type = type(initialValue),
      initial_value = tostring(initialValue),
      source_code = string.format("Reactive.state(%s)", tostring(initialValue))
    })

    -- Return mock state object
    return {
      _id = stateId,
      _value = initialValue,
      get = function(self) return self._value end,
      set = function(self, val) self._value = val end
    }
  end

  -- Mock Reactive.computed()
  function MockReactive.computed(dependencies, computeFn)
    local computedId = "computed_" .. nextId
    nextId = nextId + 1

    local depIds = {}
    for _, dep in ipairs(dependencies) do
      table.insert(depIds, dep._id)
    end

    -- Extract function source code
    local info = debug.getinfo(computeFn, "S")
    local source = extractFunctionSource(info)

    table.insert(reactiveStates, {
      id = computedId,
      name = debug.getlocal(2, 1),
      type = "computed",
      dependencies = depIds,
      expression = source
    })

    return { _id = computedId, get = function() end }
  end

  -- Mock component factory
  local function mockComponent(componentType)
    return function(props)
      local comp = {
        id = nextId,
        type = componentType,
        properties = {},
        children = {},
        events = {}
      }
      nextId = nextId + 1

      if not props then return comp end

      -- Separate properties from children
      for key, value in pairs(props) do
        if type(key) == "number" then
          -- Numeric key = child component
          table.insert(comp.children, value)
        elseif type(key) == "string" and key:match("^on[A-Z]") then
          -- Event handler (onClick, onHover, etc.)
          local logicId = "logic_" .. nextId
          nextId = nextId + 1

          -- Extract event handler source code
          local info = debug.getinfo(value, "S")
          local source = extractFunctionSource(info)

          table.insert(logicBlocks, {
            id = logicId,
            type = "event_handler",
            event = key,
            component_id = comp.id,
            language = "lua",
            source_code = source
          })

          comp.events[key] = logicId
        else
          -- Regular property
          comp.properties[key] = value
        end
      end

      table.insert(components, comp)
      return comp
    end
  end

  -- Create mock constructors for all component types
  MockUI.Container = mockComponent("Container")
  MockUI.Column = mockComponent("Column")
  MockUI.Row = mockComponent("Row")
  MockUI.Text = mockComponent("Text")
  MockUI.Button = mockComponent("Button")
  MockUI.Input = mockComponent("Input")
  MockUI.Checkbox = mockComponent("Checkbox")
  MockUI.Center = mockComponent("Center")
  MockUI.Canvas = mockComponent("Canvas")
  MockUI.Image = mockComponent("Image")
  MockUI.Markdown = mockComponent("Markdown")
  MockUI.Dropdown = mockComponent("Dropdown")

  return {
    UI = MockUI,
    Reactive = MockReactive,
    components = components,
    reactiveStates = reactiveStates,
    logicBlocks = logicBlocks
  }
end

-- Extract function source code from debug info
local function extractFunctionSource(debugInfo)
  if not debugInfo.source:match("^@") then
    -- Inline function, source is in the string
    return debugInfo.source
  end

  -- Function defined in file, read the file
  local filepath = debugInfo.source:sub(2) -- Remove '@'
  local file = io.open(filepath, "r")
  if not file then return "function() end" end

  local lines = {}
  for line in file:lines() do
    table.insert(lines, line)
  end
  file:close()

  -- Extract lines from linedefined to lastlinedefined
  local source = table.concat(lines, "\n", debugInfo.linedefined, debugInfo.lastlinedefined)
  return source
end

function Parser.parse(luaCode)
  local mockDSL = createMockDSL()

  -- Create environment with mocked modules
  local env = {
    require = function(module)
      if module == "kryon.dsl" then return mockDSL.UI end
      if module == "kryon.reactive" then return mockDSL.Reactive end
      return require(module)
    end,
    print = function() end, -- Suppress output
    -- Include standard library
    pairs = pairs,
    ipairs = ipairs,
    type = type,
    tostring = tostring,
    tonumber = tonumber,
    table = table,
    string = string,
    math = math
  }

  -- Execute Lua code in mocked environment
  local chunk, err = load(luaCode, "lua_source", "t", env)
  if not chunk then
    error("Failed to load Lua code: " .. err)
  end

  local success, result = pcall(chunk)
  if not success then
    error("Failed to execute Lua code: " .. result)
  end

  -- Build KIR from captured data
  local kir = {
    format = "kir",
    metadata = {
      source_language = "lua",
      source_file = "stdin",
      compiler_version = "kryon-1.0.0"
    },
    reactive_manifest = {
      states = mockDSL.reactiveStates
    },
    logic_blocks = mockDSL.logicBlocks,
    components = mockDSL.components
  }

  return cjson.encode(kir)
end

return Parser
```

**Option B: Static AST Parser**

Parse Lua AST without execution (harder but more precise):

```c
// ir/parsers/lua/ir_lua_parser.c

typedef struct LuaParser {
    const char* source;
    size_t length;
    size_t pos;
    LuaToken* tokens;
    size_t token_count;
} LuaParser;

char* ir_lua_file_to_kir(const char* filepath) {
    // 1. Tokenize Lua source
    LuaLexer lexer;
    lua_lexer_init(&lexer, source, length);
    LuaToken* tokens = lua_lexer_tokenize(&lexer);

    // 2. Parse top-level statements
    LuaParser parser = {0};
    parser.tokens = tokens;

    // Find reactive state declarations
    IRReactiveManifest manifest = parse_reactive_declarations(&parser);

    // Find UI component tree
    IRComponent* root = parse_ui_tree(&parser);

    // Find event handlers
    IRLogicManifest logic = parse_event_handlers(&parser);

    // 3. Build KIR
    IRSourceMetadata metadata = {
        .language = "lua",
        .file_path = filepath
    };

    char* kir = ir_serialize_json_complete(root, &manifest, &logic, &metadata);

    return kir;
}
```

### Files to Create

```
ir/parsers/lua/
├── lua_to_kir.lua ............. Execution-based parser (Lua)
├── ir_lua_parser.h ............ C header
├── ir_lua_parser.c ............ C wrapper that calls lua_to_kir.lua
├── lua_lexer.h ................ (Optional) Lexer for static parsing
├── lua_lexer.c ................ (Optional) Lexer implementation
└── Makefile ................... Build system
```

### Testing Strategy

```lua
-- Test 1: Simple component
local input = [[
local UI = require("kryon.dsl")
return UI.Button { text = "Click" }
]]

local kir = parse_lua(input)
assert(kir.components[1].type == "Button")
assert(kir.components[1].properties.text == "Click")

-- Test 2: Reactive state
local input = [[
local Reactive = require("kryon.reactive")
local UI = require("kryon.dsl")

local count = Reactive.state(0)

return UI.Text { text = "Count: " .. count:get() }
]]

local kir = parse_lua(input)
assert(#kir.reactive_manifest.states == 1)
assert(kir.reactive_manifest.states[1].name == "count")

-- Test 3: Event handlers
local input = [[
local count = Reactive.state(0)

return UI.Button {
  text = "Click",
  onClick = function()
    count:set(count:get() + 1)
  end
}
]]

local kir = parse_lua(input)
assert(#kir.logic_blocks == 1)
assert(kir.logic_blocks[1].event == "onClick")
assert(kir.logic_blocks[1].source_code:match("count:set"))
```

## Part 2: Lua Codegen Implementation

### Overview

Generate idiomatic Lua DSL code from KIR, matching the existing metaprogramming style.

### Output Format

**Target Syntax:**
```lua
-- Generated from KIR
local Reactive = require("kryon.reactive")
local UI = require("kryon.dsl")

-- Reactive State
local count = Reactive.state(0)
local todos = Reactive.array({"Buy milk", "Write code"})

-- Computed Values
local doubleCount = Reactive.computed({count}, function()
  return count:get() * 2
end)

-- Event Handlers
local function handleClick()
  count:set(count:get() + 1)
end

local function handleAddTodo()
  todos:push("New todo")
end

-- UI Tree
local root = UI.Column {
  backgroundColor = "#1a1a1a",
  padding = 20,
  gap = 10,

  UI.Text {
    text = "Count: " .. count:get(),
    fontSize = 24,
    color = "#ffffff"
  },

  UI.Button {
    text = "Increment",
    onClick = handleClick
  },

  UI.Column {
    gap = 5,

    UI.mapArray(todos:getItems(), function(item, i)
      return UI.Text { text = item }
    end)
  }
}

-- Return app object
return {
  root = root,
  window = {
    width = 800,
    height = 600,
    title = "My App"
  }
}
```

### Implementation

```c
// codegens/lua/lua_codegen.c

bool lua_codegen_generate(const char* kir_path, const char* output_path) {
    // Parse KIR
    char* kir_json = read_file(kir_path);
    KIRDocument* doc = parse_kir(kir_json);

    FILE* out = fopen(output_path, "w");

    // 1. Header comment
    fprintf(out, "-- Generated from KIR\n");
    fprintf(out, "local Reactive = require(\"kryon.reactive\")\n");
    fprintf(out, "local UI = require(\"kryon.dsl\")\n\n");

    // 2. Reactive state declarations
    if (doc->reactive_manifest) {
        lua_generate_reactive_state(out, doc->reactive_manifest);
    }

    // 3. Event handler functions
    if (doc->logic_manifest) {
        lua_generate_event_handlers(out, doc->logic_manifest);
    }

    // 4. UI component tree
    fprintf(out, "-- UI Tree\n");
    fprintf(out, "local root = ");
    lua_generate_component_tree(out, doc->root_component, 0);

    // 5. Return statement
    fprintf(out, "\n\n-- Return app object\n");
    fprintf(out, "return {\n");
    fprintf(out, "  root = root");

    // Window config if present
    if (doc->metadata && doc->metadata->window_title) {
        fprintf(out, ",\n  window = {\n");
        fprintf(out, "    width = %d,\n", doc->metadata->window_width);
        fprintf(out, "    height = %d,\n", doc->metadata->window_height);
        fprintf(out, "    title = \"%s\"\n", doc->metadata->window_title);
        fprintf(out, "  }\n");
    } else {
        fprintf(out, "\n");
    }

    fprintf(out, "}\n");

    fclose(out);
    free(kir_json);
    return true;
}

static void lua_generate_reactive_state(FILE* out, IRReactiveManifest* manifest) {
    fprintf(out, "-- Reactive State\n");

    for (size_t i = 0; i < manifest->state_count; i++) {
        IRReactiveState* state = &manifest->states[i];

        // Generate based on type
        if (strcmp(state->type, "array") == 0) {
            fprintf(out, "local %s = Reactive.array({%s})\n",
                    state->name, state->initial_value);
        } else if (strcmp(state->type, "computed") == 0) {
            // Computed values come later, after dependencies
            continue;
        } else {
            // Regular state
            fprintf(out, "local %s = Reactive.state(%s)\n",
                    state->name, state->initial_value);
        }
    }

    // Now generate computed values
    for (size_t i = 0; i < manifest->computed_count; i++) {
        IRReactiveComputed* computed = &manifest->computed[i];

        fprintf(out, "local %s = Reactive.computed({", computed->name);

        // Dependencies
        for (size_t j = 0; j < computed->dependency_count; j++) {
            fprintf(out, "%s", computed->dependencies[j]);
            if (j < computed->dependency_count - 1) fprintf(out, ", ");
        }

        fprintf(out, "}, function()\n");
        fprintf(out, "  return %s\n", computed->expression);
        fprintf(out, "end)\n");
    }

    fprintf(out, "\n");
}

static void lua_generate_event_handlers(FILE* out, IRLogicManifest* logic) {
    fprintf(out, "-- Event Handlers\n");

    IRLogicBlock* block = logic->blocks;
    while (block) {
        if (block->type == LOGIC_EVENT_HANDLER) {
            // Generate function name from event
            char funcName[128];
            snprintf(funcName, sizeof(funcName), "handle%s",
                     capitalize(block->event_name + 2)); // Skip "on"

            fprintf(out, "local function %s()\n", funcName);

            if (strcmp(block->language, "lua") == 0) {
                // Source is already Lua
                fprintf(out, "  %s\n", block->source_code);
            } else {
                // Translate from another language
                char* lua_code = translate_logic_to_lua(block);
                fprintf(out, "  %s\n", lua_code);
                free(lua_code);
            }

            fprintf(out, "end\n\n");
        }
        block = block->next;
    }

    fprintf(out, "\n");
}

static void lua_generate_component_tree(FILE* out, IRComponent* comp, int indent) {
    // Generate: UI.ComponentType {
    fprintf(out, "UI.%s {\n", ir_component_type_to_string(comp->type));

    // Properties
    lua_generate_properties(out, comp, indent + 1);

    // Event handlers
    if (comp->event_bindings) {
        lua_generate_event_bindings(out, comp->event_bindings, indent + 1);
    }

    // Children (in array part of table)
    if (comp->children && comp->child_count > 0) {
        for (size_t i = 0; i < comp->child_count; i++) {
            for (int j = 0; j < indent + 1; j++) fprintf(out, "  ");
            lua_generate_component_tree(out, comp->children[i], indent + 1);
            if (i < comp->child_count - 1) fprintf(out, ",\n");
        }
    }

    // Close table
    for (int i = 0; i < indent; i++) fprintf(out, "  ");
    fprintf(out, "}");
}

static void lua_generate_properties(FILE* out, IRComponent* comp, int indent) {
    IRStyle* style = comp->style;
    if (!style) return;

    // Background color
    if (style->background) {
        for (int i = 0; i < indent; i++) fprintf(out, "  ");
        fprintf(out, "backgroundColor = \"%s\",\n", style->background);
    }

    // Text color
    if (style->color) {
        for (int i = 0; i < indent; i++) fprintf(out, "  ");
        fprintf(out, "color = \"%s\",\n", style->color);
    }

    // Dimensions
    if (style->width.type != IR_DIMENSION_AUTO) {
        for (int i = 0; i < indent; i++) fprintf(out, "  ");
        fprintf(out, "width = ");
        lua_emit_dimension(out, &style->width);
        fprintf(out, ",\n");
    }

    if (style->height.type != IR_DIMENSION_AUTO) {
        for (int i = 0; i < indent; i++) fprintf(out, "  ");
        fprintf(out, "height = ");
        lua_emit_dimension(out, &style->height);
        fprintf(out, ",\n");
    }

    // Padding
    if (style->padding.top > 0 || style->padding.left > 0) {
        for (int i = 0; i < indent; i++) fprintf(out, "  ");
        if (style->padding.top == style->padding.right &&
            style->padding.top == style->padding.bottom &&
            style->padding.top == style->padding.left) {
            fprintf(out, "padding = %d,\n", (int)style->padding.top);
        } else {
            fprintf(out, "padding = \"%dpx %dpx %dpx %dpx\",\n",
                    (int)style->padding.top, (int)style->padding.right,
                    (int)style->padding.bottom, (int)style->padding.left);
        }
    }

    // Gap (for Column/Row)
    if (comp->layout && comp->layout->gap > 0) {
        for (int i = 0; i < indent; i++) fprintf(out, "  ");
        fprintf(out, "gap = %d,\n", (int)comp->layout->gap);
    }

    // Text content
    if (comp->text_content) {
        for (int i = 0; i < indent; i++) fprintf(out, "  ");
        fprintf(out, "text = \"%s\",\n", escape_string(comp->text_content));
    }

    // Font size
    if (style->font && style->font->size > 0) {
        for (int i = 0; i < indent; i++) fprintf(out, "  ");
        fprintf(out, "fontSize = %d,\n", style->font->size);
    }

    // ... all other properties
}

static void lua_emit_dimension(FILE* out, IRDimension* dim) {
    switch (dim->type) {
        case IR_DIMENSION_PX:
            if (dim->value == (int)dim->value) {
                fprintf(out, "%d", (int)dim->value);
            } else {
                fprintf(out, "\"%fpx\"", dim->value);
            }
            break;
        case IR_DIMENSION_PERCENT:
            fprintf(out, "\"%d%%\"", (int)dim->value);
            break;
        case IR_DIMENSION_AUTO:
            fprintf(out, "\"auto\"");
            break;
        case IR_DIMENSION_FLEX:
            fprintf(out, "\"%dfr\"", (int)dim->value);
            break;
        case IR_DIMENSION_VW:
            fprintf(out, "\"%dvw\"", (int)dim->value);
            break;
        case IR_DIMENSION_VH:
            fprintf(out, "\"%dvh\"", (int)dim->value);
            break;
    }
}

static void lua_generate_event_bindings(FILE* out, IREventBindings* bindings, int indent) {
    IREventBinding* binding = bindings->bindings;
    while (binding) {
        for (int i = 0; i < indent; i++) fprintf(out, "  ");

        // Generate: onClick = handleClick,
        char funcName[128];
        snprintf(funcName, sizeof(funcName), "handle%s",
                 capitalize(binding->event_name + 2));

        fprintf(out, "%s = %s,\n", binding->event_name, funcName);

        binding = binding->next;
    }
}
```

### Logic Translation Engine

```c
// codegens/common/logic_translator.c

char* translate_logic_to_lua(IRLogicBlock* block) {
    if (strcmp(block->language, "lua") == 0) {
        return strdup(block->source_code);
    }

    if (strcmp(block->language, "typescript") == 0) {
        return translate_typescript_to_lua(block->source_code);
    }

    if (strcmp(block->language, "c") == 0) {
        return translate_c_to_lua(block->source_code);
    }

    // Fallback
    return strdup("-- TODO: Translate from " + block->language);
}

static char* translate_typescript_to_lua(const char* ts_code) {
    // Example: "() => setCount(count + 1)"
    // Becomes: "count:set(count:get() + 1)"

    char* lua_code = malloc(strlen(ts_code) * 2);

    // Simple pattern matching for common cases
    if (strstr(ts_code, "() =>")) {
        // Arrow function
        const char* body = strchr(ts_code, '>') + 1;
        while (*body == ' ') body++;

        // Replace setter calls: setX(...) -> x:set(...)
        char* result = replace_setState_calls(body);

        // Replace getter calls: count -> count:get()
        result = replace_state_access(result);

        strcpy(lua_code, result);
        free(result);
    } else {
        strcpy(lua_code, ts_code);
    }

    return lua_code;
}
```

### Files to Modify/Create

```
codegens/lua/
├── lua_codegen.h .............. Header
├── lua_codegen.c .............. Main implementation
├── lua_property_emitter.c ..... Property generation
└── Makefile ................... Build system

codegens/common/
├── logic_translator.h ......... Cross-language translation API
└── logic_translator.c ......... Translation engine
```

## Part 3: Integration & Testing

### CLI Integration

```c
// cli/src/commands/cmd_compile.c

// Add Lua support to compile command
if (has_extension(input_file, ".lua")) {
    kir_json = ir_lua_file_to_kir(input_file);
}

// cli/src/commands/cmd_codegen.c

// Add Lua support to codegen command
if (strcmp(target_lang, "lua") == 0) {
    success = lua_codegen_generate(kir_file, output_file);
}
```

### Round-Trip Tests

```bash
#!/bin/bash
# Test 1: Lua → KIR → Lua

# Original Lua code
cat > original.lua << 'EOF'
local Reactive = require("kryon.reactive")
local UI = require("kryon.dsl")

local count = Reactive.state(0)

local root = UI.Column {
  UI.Text { text = "Count: " .. count:get() },
  UI.Button {
    text = "Increment",
    onClick = function()
      count:set(count:get() + 1)
    end
  }
}

return { root = root }
EOF

# Step 1: Lua → KIR
kryon compile original.lua -o app.kir

# Verify KIR contains state and logic
jq '.reactive_manifest.states[0].name' app.kir  # Should be "count"
jq '.logic_blocks[0].event' app.kir             # Should be "onClick"

# Step 2: KIR → Lua
kryon codegen lua app.kir -o generated.lua

# Step 3: Generated Lua → KIR
kryon compile generated.lua -o app2.kir

# Step 4: Verify semantic equivalence
diff app.kir app2.kir  # Should be identical or semantically equivalent
```

### Visual Equivalence Tests

```bash
# Same KIR should produce working Lua apps
kryon compile button.kry -o button.kir
kryon codegen lua button.kir -o button.lua

# Run and verify it works
kryon run button.lua  # Should display functional button
```

## Part 4: Advanced Features

### Dynamic Children (mapArray)

**Input KIR:**
```json
{
  "components": [{
    "id": 1,
    "type": "Column",
    "children": [/* dynamic list */],
    "dynamic_children": {
      "source": "state_items",
      "template": { /* child template */ }
    }
  }]
}
```

**Generated Lua:**
```lua
local items = Reactive.array({"A", "B", "C"})

UI.Column {
  unpack(UI.mapArray(items:getItems(), function(item, i)
    return UI.Text { text = item }
  end))
}
```

### Reactive Bindings in Text

**Input KIR:**
```json
{
  "type": "Text",
  "text": "Count: {{count}}",
  "bindings": {
    "text": "state_count"
  }
}
```

**Generated Lua:**
```lua
UI.Text {
  text = "Count: " .. count:get()
}
```

### Component Functions

**Input KIR:**
```json
{
  "custom_components": [{
    "name": "Counter",
    "parameters": ["initialValue"],
    "template": { /* component tree */ }
  }]
}
```

**Generated Lua:**
```lua
local function Counter(initialValue)
  local value = Reactive.state(initialValue or 0)

  return UI.Row {
    UI.Button {
      text = "-",
      onClick = function() value:update(function(v) return v - 1 end) end
    },
    UI.Text { text = value:get() },
    UI.Button {
      text = "+",
      onClick = function() value:update(function(v) return v + 1 end) end
    }
  }
end

-- Usage
UI.Column {
  Counter(0),
  Counter(10),
  Counter(5)
}
```

## Estimated Effort

### Parser Implementation
- Mock DSL setup: 2 days
- Component capture: 2 days
- Reactive state extraction: 1 day
- Event handler extraction: 2 days
- Testing: 1 day
**Subtotal: 8 days**

### Codegen Implementation
- Component tree generation: 2 days
- Property emission: 2 days
- Reactive state generation: 1 day
- Event handler generation: 2 days
- Logic translation: 2 days
- Testing: 1 day
**Subtotal: 10 days**

### Integration & Polish
- CLI integration: 1 day
- Round-trip tests: 2 days
- Documentation: 1 day
**Subtotal: 4 days**

**Total: ~22 days (4.5 weeks)**

## Success Criteria

✅ **Parser:**
- Parses Lua DSL with metaprogramming syntax
- Preserves all event handlers as source code
- Extracts reactive state declarations
- Handles Reactive.state(), Reactive.computed(), Reactive.array()
- Generates complete KIR

✅ **Codegen:**
- Generates idiomatic Lua DSL code
- Uses proper metaprogramming pattern (SmartComponent)
- Correctly emits reactive state declarations
- Event handlers as named functions
- Handles all component types and properties

✅ **Round-Trip:**
- Lua → KIR → Lua preserves semantics
- Generated Lua code runs correctly
- Visual output matches original

✅ **Examples:**
- All 15 Lua examples work
- Counter app, Todo app, Tabs work
- Complex apps with dynamic children work

## Next Steps

1. Implement Lua parser (execution-based approach)
2. Test parser with existing examples
3. Implement Lua codegen
4. Test codegen output
5. Implement logic translator
6. End-to-end round-trip tests
7. Update CLI commands
8. Documentation
