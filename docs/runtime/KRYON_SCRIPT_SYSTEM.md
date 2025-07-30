# Kryon Script System & Lua Integration

Complete documentation of Kryon's script execution system, including Lua integration, DOM bridge, and reactive programming capabilities.

> **🔗 Related Documentation**: 
> - [KRYON_RUNTIME_SYSTEM.md](KRYON_RUNTIME_SYSTEM.md) - Runtime system architecture
> - [KRYON_ERROR_REFERENCE.md](KRYON_ERROR_REFERENCE.md) - Script error codes and debugging

## Table of Contents
- [Script System Overview](#script-system-overview)
- [Lua Engine Integration](#lua-engine-integration)
- [DOM Bridge API](#dom-bridge-api)
- [Reactive Variables](#reactive-variables)
- [Event Handling](#event-handling)
- [Canvas Scripting](#canvas-scripting)
- [Native Renderer Integration](#native-renderer-integration)
- [Performance & Memory Management](#performance--memory-management)
- [Advanced Features](#advanced-features)

## Script System Overview

Kryon's script system provides dynamic behavior through multiple scripting languages, with Lua as the primary engine for performance and embedded systems compatibility.

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    ScriptSystem                             │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐│
│  │ Script Registry │ │ Template Vars   │ │ Bridge Data     ││
│  │ (Multi-Engine)  │ │ (Reactive)      │ │ (DOM Access)    ││
│  └─────────────────┘ └─────────────────┘ └─────────────────┘│
└─────────────────────────────────────────────────────────────┘
            │                    │                    │
    ┌───────▼───────┐    ┌───────▼───────┐    ┌───────▼───────┐
    │  Lua Engine   │    │   JS Engine   │    │ Python Engine │
    │  (Primary)    │    │  (Browser)    │    │ (Extensions)  │
    └───────────────┘    └───────────────┘    └───────────────┘
```

### Core Components

```rust
pub struct ScriptSystem {
    /// Registry of available script engines
    registry: ScriptRegistry,
    /// Template variables for reactive updates
    template_variables: HashMap<String, String>,
    /// Element data for DOM API
    elements_data: HashMap<ElementId, Element>,
    /// Style mappings from KRB file
    style_mappings: HashMap<u8, Style>,
    /// Bridge data for DOM API setup
    bridge_data: Option<BridgeData>,
}
```

### Supported Languages

| Language | Status | Use Cases | Performance |
|----------|--------|-----------|-------------|
| **Lua** | ✅ Primary | UI logic, events, canvas | Excellent |
| **JavaScript** | 🚧 Planned | Web compatibility | Good |
| **Python** | 🚧 Planned | Data processing, ML | Fair |
| **Wren** | 🚧 Planned | Game scripting | Excellent |

## Lua Engine Integration

### Engine Architecture

```rust
pub struct LuaEngine {
    /// The Lua virtual machine
    lua: Rc<Lua>,
    /// Bridge for DOM API
    bridge: LuaBridge,
    /// Reactive variable system
    reactive: LuaReactiveSystem,
    /// Bytecode executor
    bytecode_executor: LuaBytecodeExecutor,
    /// Function registry
    functions: HashMap<String, String>,
    /// Memory statistics
    memory_stats: EngineMemoryStats,
    /// Native renderer contexts
    native_contexts: HashMap<String, NativeRendererContext>,
}
```

### Script Compilation & Loading

**Source Code Loading**:
```lua
-- myScript.lua
function handleButtonClick()
    kryon.debug("Button clicked!")
    local counter = kryon.getVariable("counter") or 0
    counter = counter + 1
    kryon.setVariable("counter", tostring(counter))
end

function init()
    kryon.debug("Script initialized")
    kryon.setVariable("counter", "0")
end
```

**Bytecode Execution**:
```rust
// Compile KRY with scripts to KRB
let compiler = Compiler::new();
let krb_bytes = compiler.compile_to_bytes(&kry_source)?;

// Scripts are pre-compiled to bytecode in KRB format
// Runtime loads and executes bytecode directly
script_system.load_compiled_scripts(&krb_file.scripts)?;
```

### Function Management

```rust
impl LuaEngine {
    pub fn load_script(&mut self, name: &str, code: &str) -> Result<()> {
        // Execute script to load functions
        self.lua.load(code).exec()?;
        
        // Extract function names using regex
        let function_regex = regex::Regex::new(r"function\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(")?;
        for captures in function_regex.captures_iter(code) {
            if let Some(func_name) = captures.get(1) {
                self.functions.insert(func_name.as_str().to_string(), name.to_string());
            }
        }
        
        Ok(())
    }
    
    pub fn call_function(&mut self, name: &str, args: Vec<ScriptValue>) -> Result<ScriptValue> {
        let function: LuaFunction = self.lua.globals().get(name)?;
        
        // Convert args to Lua values
        let lua_args: Vec<LuaValue> = args.into_iter()
            .map(|arg| self.script_value_to_lua_value(arg))
            .collect::<Result<Vec<_>>>()?;
        
        // Execute function
        let result: LuaValue = function.call(lua_args)?;
        
        Ok(self.lua_value_to_script_value(result))
    }
}
```

## DOM Bridge API

The DOM Bridge provides Lua scripts with full access to UI elements and their properties.

### Core DOM Functions

```lua
-- Element Access
local button = kryon.getElementById("myButton")
local elements = kryon.getElementsByType("Text")

-- Property Manipulation
kryon.setProperty("myButton", "text", "New Text")
local text = kryon.getProperty("myButton", "text")

-- Style Changes
kryon.setStyle("myButton", "button_primary")
kryon.setProperty("myButton", "backgroundColor", "#FF0000")

-- Visibility Control
kryon.setVisible("myButton", false)
kryon.show("myButton")
kryon.hide("myButton")

-- State Management
kryon.setState("myCheckbox", true)  -- Check checkbox
local isChecked = kryon.getState("myCheckbox")
```

### Bridge Implementation

```rust
pub struct LuaBridge {
    lua: Rc<Lua>,
    element_ids: HashMap<String, ElementId>,
    style_ids: HashMap<String, u8>,
    pending_changes: HashMap<String, ChangeSet>,
    canvas_commands: Vec<serde_json::Value>,
}

impl LuaBridge {
    pub fn setup(&mut self, bridge_data: &BridgeData) -> Result<()> {
        // Setup DOM API functions in Lua global scope
        let lua = &self.lua;
        
        // Element access functions
        lua.globals().set("getElementById", self.create_get_element_by_id_function()?)?;
        lua.globals().set("setProperty", self.create_set_property_function()?)?;
        lua.globals().set("getProperty", self.create_get_property_function()?)?;
        
        // Style functions
        lua.globals().set("setStyle", self.create_set_style_function()?)?;
        
        // Visibility functions
        lua.globals().set("setVisible", self.create_set_visible_function()?)?;
        lua.globals().set("show", self.create_show_function()?)?;
        lua.globals().set("hide", self.create_hide_function()?)?;
        
        // State management
        lua.globals().set("setState", self.create_set_state_function()?)?;
        lua.globals().set("getState", self.create_get_state_function()?)?;
        
        // Utility functions
        lua.globals().set("debug", self.create_debug_function()?)?;
        lua.globals().set("log", self.create_log_function()?)?;
        
        Ok(())
    }
}
```

### Advanced DOM Operations

```lua
-- Element Creation (Dynamic UI)
local newButton = kryon.createElement("Button", {
    text = "Dynamic Button",
    backgroundColor = "#00FF00",
    onClick = "handleDynamicClick"
})
kryon.appendChild("container", newButton)

-- Element Removal
kryon.removeElement("oldButton")

-- Batch Operations
kryon.batchUpdate(function()
    kryon.setProperty("button1", "text", "Updated")
    kryon.setProperty("button2", "text", "Updated")
    kryon.setProperty("button3", "text", "Updated")
end)

-- Animation Support
kryon.animate("myButton", {
    property = "opacity",
    from = 0.0,
    to = 1.0,
    duration = 1000,  -- milliseconds
    easing = "ease-in-out"
})
```

## Reactive Variables

Kryon's reactive variable system automatically updates UI elements when template variables change.

### Template Variable Binding

**KRY Source**:
```kry
App {
    Container {
        Text {
            text: "Count: " + $counter
            visible: $showCounter
        }
        
        Button {
            text: "Increment"
            onClick: "incrementCounter()"
        }
    }
}
```

**Lua Script**:
```lua
function incrementCounter()
    local counter = tonumber(kryon.getVariable("counter")) or 0
    counter = counter + 1
    
    -- This automatically updates all elements using $counter
    kryon.setVariable("counter", tostring(counter))
    
    -- Show/hide based on counter value
    local showCounter = counter > 0
    kryon.setVariable("showCounter", tostring(showCounter))
end

function init()
    kryon.setVariable("counter", "0")
    kryon.setVariable("showCounter", "false")
end
```

### Reactive System Implementation

```rust
pub struct LuaReactiveSystem {
    lua: Rc<Lua>,
    variables: HashMap<String, String>,
    bindings: Vec<VariableBinding>,
    pending_changes: HashMap<String, ChangeSet>,
}

pub struct VariableBinding {
    variable_name: String,
    element_id: ElementId,
    property: String,
    expression: String,  // For computed bindings like "Count: " + $counter
}

impl LuaReactiveSystem {
    pub fn setup(&mut self, variables: &HashMap<String, String>) -> Result<()> {
        self.variables = variables.clone();
        
        // Setup reactive variable functions in Lua
        let set_variable_fn = self.lua.create_function(move |_, (name, value): (String, String)| {
            // This triggers reactive updates
            Ok(())
        })?;
        
        self.lua.globals().set("setVariable", set_variable_fn)?;
        
        Ok(())
    }
    
    pub fn get_pending_changes(&mut self) -> Result<HashMap<String, ChangeSet>> {
        let changes = self.pending_changes.clone();
        Ok(changes)
    }
}
```

### Computed Properties

```lua
-- Computed properties that depend on multiple variables
function updateComputedProperties()
    local firstName = kryon.getVariable("firstName") or ""
    local lastName = kryon.getVariable("lastName") or ""
    local fullName = firstName .. " " .. lastName
    
    -- Update computed property
    kryon.setVariable("fullName", fullName)
    
    -- Complex computation
    local counter = tonumber(kryon.getVariable("counter")) or 0
    local isEven = (counter % 2) == 0
    kryon.setVariable("counterIsEven", tostring(isEven))
    
    -- Conditional computed values
    local status = counter > 10 and "High" or counter > 5 and "Medium" or "Low"
    kryon.setVariable("counterStatus", status)
end
```

## Event Handling

### UI Event Integration

```lua
-- Mouse Events
function handleClick()
    kryon.debug("Click event received")
end

function handleHover()
    kryon.setProperty("hoverTarget", "backgroundColor", "#FFFFCC")
end

function handleMouseLeave()
    kryon.setProperty("hoverTarget", "backgroundColor", "#FFFFFF")
end

-- Keyboard Events
function handleKeyPress(key, modifiers)
    if key == "Enter" then
        handleSubmit()
    elseif key == "Escape" then
        handleCancel()
    end
end

-- Input Events
function handleTextChange(newText)
    kryon.setVariable("inputValue", newText)
    validateInput(newText)
end

function handleCheckboxChange(checked)
    kryon.setVariable("isChecked", tostring(checked))
    updateFormState()
end
```

### Custom Event System

```lua
-- Custom event emission
function emitCustomEvent(eventName, data)
    kryon.emit(eventName, data)
end

-- Custom event handling
function onCustomEvent(eventName, data)
    if eventName == "dataUpdated" then
        refreshUI(data)
    elseif eventName == "userLogin" then
        showUserInterface(data.username)
    end
end

-- Register custom event listeners
kryon.addEventListener("dataUpdated", onCustomEvent)
kryon.addEventListener("userLogin", onCustomEvent)
```

## Canvas Scripting

Kryon supports powerful 2D and 3D canvas scripting for custom graphics and games.

### 2D Canvas API

```lua
-- Canvas drawing functions
function drawCanvas()
    -- Clear canvas
    canvas.clear()
    
    -- Basic shapes
    canvas.drawRect(10, 10, 100, 50, {r = 1.0, g = 0.0, b = 0.0, a = 1.0})
    canvas.drawCircle(200, 100, 30, {r = 0.0, g = 1.0, b = 0.0, a = 1.0})
    canvas.drawLine(0, 0, 300, 200, {r = 0.0, g = 0.0, b = 1.0, a = 1.0}, 2.0)
    
    -- Text rendering
    canvas.drawText("Hello Canvas!", 50, 150, 16, {r = 0.0, g = 0.0, b = 0.0, a = 1.0})
    
    -- Image drawing
    canvas.drawImage("logo.png", 250, 50, 100, 100, 1.0)
    
    -- Complex shapes
    local points = {{x = 100, y = 200}, {x = 150, y = 250}, {x = 200, y = 200}}
    canvas.drawPolygon(points, {r = 1.0, g = 1.0, b = 0.0, a = 0.5})
end

-- Canvas lifecycle hooks
function onCanvasLoad()
    kryon.debug("Canvas loaded and ready")
end

function onCanvasUpdate(deltaTime)
    -- Update animation state
    local time = kryon.getTime()
    local x = 150 + math.sin(time) * 50
    
    -- Clear and redraw
    canvas.clear()
    canvas.drawCircle(x, 100, 20, {r = 1.0, g = 0.0, b = 0.0, a = 1.0})
end
```

### 3D Canvas API

```lua
function draw3DCanvas()
    -- Setup 3D camera
    local camera = {
        position = {x = 0, y = 0, z = 10},
        target = {x = 0, y = 0, z = 0},
        up = {x = 0, y = 1, z = 0},
        fov = 45,
        near_plane = 0.1,
        far_plane = 100.0
    }
    
    canvas3d.beginMode(camera)
    
    -- 3D primitives
    canvas3d.drawCube({x = 0, y = 0, z = 0}, {x = 2, y = 2, z = 2}, 
                     {r = 1.0, g = 0.0, b = 0.0, a = 1.0}, false)
    
    canvas3d.drawSphere({x = 3, y = 0, z = 0}, 1.0, 
                       {r = 0.0, g = 1.0, b = 0.0, a = 1.0}, false)
    
    canvas3d.drawPlane({x = -3, y = 0, z = 0}, {x = 2, y = 2}, {x = 0, y = 1, z = 0},
                      {r = 0.0, g = 0.0, b = 1.0, a = 1.0})
    
    -- Lighting
    canvas3d.setLighting({
        ambient_color = {r = 0.2, g = 0.2, b = 0.2, a = 1.0},
        directional_light = {
            direction = {x = -1, y = -1, z = -1},
            color = {r = 1.0, g = 1.0, b = 1.0, a = 1.0},
            intensity = 1.0
        }
    })
    
    canvas3d.endMode()
end
```

## Native Renderer Integration

For platform-specific optimizations, Kryon supports native renderer contexts.

### Native Context Setup

```lua
-- Create native renderer context
function initNativeRenderer()
    local contextId = "gameView"
    local backend = "raylib"  -- or "wgpu", "opengl"
    local elementId = kryon.getElementById("gameCanvas")
    local position = {x = 0, y = 0}
    local size = {x = 800, y = 600}
    
    kryon.createNativeContext(contextId, backend, elementId, position, size)
end

-- Native rendering script
function renderNativeFrame()
    native.beginDrawing()
    native.clearBackground({r = 0.1, g = 0.1, b = 0.1, a = 1.0})
    
    -- High-performance native operations
    native.drawTexture("spritesheet.png", 100, 100)
    native.drawText("Native Rendered", 10, 10, 20, {r = 1, g = 1, b = 1, a = 1})
    
    native.endDrawing()
end
```

## Performance & Memory Management

### Memory Statistics

```rust
pub struct EngineMemoryStats {
    pub current_usage: usize,
    pub peak_usage: usize,
    pub object_count: usize,
    pub memory_limit: Option<usize>,
}

impl LuaEngine {
    fn update_memory_stats(&mut self) {
        self.memory_stats.object_count = self.functions.len();
        self.memory_stats.current_usage = self.memory_stats.object_count * 1024;
        self.memory_stats.peak_usage = self.memory_stats.peak_usage.max(self.memory_stats.current_usage);
    }
}
```

### Performance Best Practices

**Efficient Lua Code**:
```lua
-- Good: Cache element references
local button = kryon.getElementById("myButton")
for i = 1, 100 do
    button.text = "Count: " .. i
end

-- Bad: Query every iteration
for i = 1, 100 do
    kryon.getElementById("myButton").text = "Count: " .. i
end

-- Good: Batch DOM operations
kryon.batchUpdate(function()
    for i = 1, 10 do
        kryon.setProperty("item" .. i, "visible", false)
    end
end)

-- Good: Use local variables
local function updateCounters()
    local count = tonumber(kryon.getVariable("count")) or 0
    local doubled = count * 2
    local status = count > 10 and "High" or "Low"
    
    kryon.setVariable("doubled", tostring(doubled))
    kryon.setVariable("status", status)
end
```

### Memory Optimization

```lua
-- Cleanup resources
function cleanup()
    -- Clear large tables
    myLargeTable = nil
    
    -- Remove event listeners
    kryon.removeEventListener("dataUpdated", onDataUpdated)
    
    -- Force garbage collection (use sparingly)
    collectgarbage("collect")
end

-- Pool objects for frequent allocation/deallocation
local objectPool = {}

function getPooledObject()
    if #objectPool > 0 then
        return table.remove(objectPool)
    else
        return createNewObject()
    end
end

function returnToPool(obj)
    resetObject(obj)
    table.insert(objectPool, obj)
end
```

## Advanced Features

### Script Debugging

```lua
-- Debug utilities
function debugElementTree()
    local elements = kryon.getAllElements()
    for id, element in pairs(elements) do
        kryon.debug("Element " .. id .. ": " .. element.type)
    end
end

function debugVariables()
    local vars = kryon.getAllVariables()
    for name, value in pairs(vars) do
        kryon.debug("Variable " .. name .. " = " .. value)
    end
end

-- Performance profiling
function profileFunction(name, func)
    local startTime = kryon.getTime()
    local result = func()
    local endTime = kryon.getTime()
    kryon.debug("Function " .. name .. " took " .. (endTime - startTime) .. "ms")
    return result
end
```

### Error Handling

```lua
-- Robust error handling
function safeExecute(func, errorHandler)
    local success, result = pcall(func)
    if success then
        return result
    else
        if errorHandler then
            errorHandler(result)
        else
            kryon.error("Script error: " .. tostring(result))
        end
        return nil
    end
end

-- Usage example
safeExecute(function()
    local risky_value = tonumber(kryon.getVariable("maybeNumber"))
    return risky_value * 2
end, function(err)
    kryon.debug("Failed to process number: " .. err)
    kryon.setVariable("maybeNumber", "0")
end)
```

### Async Operations

```lua
-- Asynchronous operations using coroutines
function asyncDataLoad()
    local co = coroutine.create(function()
        kryon.debug("Starting data load...")
        
        -- Simulate async operation
        kryon.setTimeout(1000, function()
            kryon.setVariable("dataLoaded", "true")
            kryon.debug("Data loaded!")
        end)
    end)
    
    coroutine.resume(co)
end

-- Promise-like pattern
function loadDataAsync()
    return {
        then = function(self, callback)
            kryon.setTimeout(500, function()
                local data = {status = "success", value = "loaded data"}
                callback(data)
            end)
            return self
        end,
        catch = function(self, errorCallback)
            -- Error handling would go here
            return self
        end
    }
end

-- Usage
loadDataAsync()
    :then(function(data)
        kryon.setVariable("asyncData", data.value)
    end)
    :catch(function(error)
        kryon.error("Failed to load data: " .. error)
    end)
```

This comprehensive script system provides powerful dynamic behavior capabilities while maintaining excellent performance and developer experience.