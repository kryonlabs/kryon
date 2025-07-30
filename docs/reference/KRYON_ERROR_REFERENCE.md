# Kryon Error Reference & Debugging Guide

Comprehensive guide to all Kryon error codes, debugging techniques, and troubleshooting common issues.

> **🔗 Related Documentation**: 
> - [KRY_LANGUAGE_SPECIFICATION.md](KRY_LANGUAGE_SPECIFICATION.md) - Language syntax reference
> - [KRB_BINARY_FORMAT_SPECIFICATION.md](KRB_BINARY_FORMAT_SPECIFICATION.md) - Binary format details

## Table of Contents
- [Error Categories](#error-categories)
- [Compilation Errors](#compilation-errors)
- [Runtime Errors](#runtime-errors)
- [Script System Errors](#script-system-errors)
- [Renderer Errors](#renderer-errors)
- [Layout Engine Errors](#layout-engine-errors)
- [Debugging Techniques](#debugging-techniques)
- [Common Issues & Solutions](#common-issues--solutions)
- [Performance Troubleshooting](#performance-troubleshooting)

## Error Categories

Kryon errors are organized into distinct categories for easier identification and resolution.

### Error Code Ranges
| Range | Category | Description |
|-------|----------|-------------|
| `E001-E099` | Parse/Compilation | KRY source parsing and KRB compilation |
| `E100-E199` | Validation | Property validation and element constraints |
| `E200-E299` | Runtime | Application execution and state management |
| `E300-E399` | Script | Lua/JavaScript execution and DOM bridge |
| `E400-E499` | Render | Backend rendering and graphics |
| `E500-E599` | Layout | Layout computation and constraint solving |
| `E600-E699` | Resource | Asset loading and memory management |
| `E700-E799` | Platform | Platform-specific backend issues |

## Compilation Errors

Errors that occur during KRY-to-KRB compilation.

### Parse Errors (E001-E049)

#### E001: Unexpected Token
```
Error E001: Unexpected token 'invalid_token' at line 15, column 8
Expected: identifier, string, number, or '{'
```

**Cause**: Invalid KRY syntax - unknown token encountered  
**Solution**: Check syntax around the specified line/column

**Example**:
```kry
Container {
    @ invalid syntax here  # ❌ Invalid token
    text: "Hello"
}
```

#### E002: Unterminated String
```
Error E002: Unterminated string literal starting at line 10, column 12
```

**Cause**: String opened but not closed  
**Solution**: Add closing quote

#### E003: Missing Opening Brace
```
Error E003: Expected '{' after element name 'Container' at line 5
```

**Cause**: Element declaration without opening brace  
**Solution**: Add `{` after element name

#### E004: Missing Closing Brace
```
Error E004: Expected '}' to close element 'Container' opened at line 5
```

**Cause**: Element not properly closed  
**Solution**: Add matching `}`

#### E005: Invalid Property Name
```
Error E005: Invalid property name 'invalid-prop' at line 8, column 4
Valid properties for Container: backgroundColor, padding, margin, etc.
```

**Cause**: Property not valid for element type  
**Solution**: Check property spelling or element compatibility

### Semantic Errors (E050-E099)

#### E050: Property Type Mismatch
```
Error E050: Property 'fontSize' expects Float, got String "large" at line 12
```

**Cause**: Wrong value type for property  
**Solution**: Use correct type (e.g., `fontSize: 16` not `fontSize: "large"`)

#### E051: Invalid Color Format
```
Error E051: Invalid color format "#gggggg" at line 15
Expected: #RGB, #RRGGBB, #RRGGBBAA, or named color
```

**Cause**: Malformed hex color  
**Solution**: Use valid hex format like `#FF0000` or named color like `"red"`

#### E052: Unknown Element Type
```
Error E052: Unknown element type 'InvalidElement' at line 7
Available elements: App, Container, Text, Button, Input, etc.
```

**Cause**: Element type doesn't exist  
**Solution**: Check element spelling against available types

#### E053: Circular Reference
```
Error E053: Circular reference detected in component hierarchy
Components: MyCard -> MyButton -> MyCard
```

**Cause**: Component includes itself through inheritance chain  
**Solution**: Remove circular dependency

## Runtime Errors

Errors during application execution.

### Application Errors (E100-E149)

#### E100: KRB File Not Found
```
Error E100: KRB file not found: 'app.krb'
Current directory: /path/to/app
```

**Cause**: Compiled KRB file missing  
**Solution**: Compile KRY source or check file path

#### E101: Invalid KRB Format
```
Error E101: Invalid KRB file format
Expected magic number: KRBY (0x4B524259)
Found: 0x12345678
```

**Cause**: File is not a valid KRB binary  
**Solution**: Recompile from KRY source

#### E102: KRB Version Mismatch
```
Error E102: KRB version mismatch
File version: 2.1.0
Runtime version: 2.0.0
```

**Cause**: KRB file compiled with newer version  
**Solution**: Update runtime or recompile with matching version

#### E103: Missing Root Element
```
Error E103: No root element found in KRB file
Application requires exactly one root element
```

**Cause**: KRB file has no App element  
**Solution**: Add App element as root

### State Management Errors (E150-E199)

#### E150: Template Variable Not Found
```
Error E150: Template variable '$counter' not found
Available variables: $message, $isVisible, $userName
```

**Cause**: Undefined template variable referenced  
**Solution**: Initialize variable or check spelling

#### E151: Template Binding Error
```
Error E151: Failed to bind template variable '$counter' to element property 'text'
Cannot convert integer 42 to string context
```

**Cause**: Type conversion failed for template binding  
**Solution**: Use explicit conversion or compatible types

#### E152: State Update Conflict
```
Error E152: Conflicting state updates for element ID 'myButton'
Script attempted to set both 'visible' and 'hidden' properties
```

**Cause**: Script tried to set conflicting properties  
**Solution**: Review script logic for conflicts

#### E153: Invalid State Transition
```
Error E153: Invalid state transition for element ID 'checkbox1'
Cannot transition from Disabled to Checked directly
```

**Cause**: Attempted invalid UI state change  
**Solution**: Transition through valid intermediate states

## Script System Errors

Errors in Lua/JavaScript execution and DOM bridge.

### Script Execution Errors (E300-E349)

#### E300: Script Not Found
```
Error E300: Script function 'handleClick' not found
Available functions: init, handleSubmit, updateCounter
```

**Cause**: Event handler references non-existent function  
**Solution**: Define function or check spelling

#### E301: Lua Syntax Error
```
Error E301: Lua syntax error in script 'myScript.lua' at line 5:
unexpected symbol near ')'
```

**Cause**: Invalid Lua syntax  
**Solution**: Fix Lua code syntax

#### E302: Script Runtime Error
```
Error E302: Script runtime error in function 'handleClick':
attempt to perform arithmetic on a nil value
Stack trace: handleClick:12 -> updateCounter:4
```

**Cause**: Lua runtime error (nil access, etc.)  
**Solution**: Add null checks and proper error handling

#### E303: Function Call Error
```
Error E303: Error calling function 'processData' with args [42, "test"]
Function expects (number, number), got (number, string)
```

**Cause**: Wrong argument types passed to function  
**Solution**: Pass correct argument types

### DOM Bridge Errors (E350-E399)

#### E350: Element Not Found
```
Error E350: DOM element with ID 'nonExistentButton' not found
Available element IDs: submitBtn, cancelBtn, messageText
```

**Cause**: Script tried to access non-existent element  
**Solution**: Check element ID or create element

#### E351: Property Access Error
```
Error E351: Cannot access property 'invalidProp' on element 'myButton'
Valid properties: text, backgroundColor, visible, onClick
```

**Cause**: Script accessed invalid property  
**Solution**: Use valid property name

#### E352: Permission Denied
```
Error E352: Script permission denied: cannot modify system properties
Attempted to modify 'windowWidth' from user script
```

**Cause**: Script tried to modify restricted properties  
**Solution**: Use allowed properties only

#### E353: Type Conversion Error
```
Error E353: Cannot convert script value to property type
Script provided: table
Property 'backgroundColor' expects: Color
```

**Cause**: Script passed wrong value type  
**Solution**: Convert value to expected type

## Renderer Errors

Backend rendering and graphics errors.

### Graphics Errors (E400-E449)

#### E400: Renderer Initialization Failed
```
Error E400: Failed to initialize WGPU renderer
No compatible graphics adapter found
```

**Cause**: Graphics hardware incompatible or drivers missing  
**Solution**: Update graphics drivers or use different backend

#### E401: Texture Loading Failed
```
Error E401: Failed to load texture 'background.png'
File format not supported: PNG with alpha channel
```

**Cause**: Unsupported image format or corrupted file  
**Solution**: Convert to supported format (JPEG, PNG, etc.)

#### E402: Shader Compilation Error
```
Error E402: Shader compilation failed for 'rect.wgsl'
Line 15: unknown identifier 'invalidVar'
```

**Cause**: Invalid WGSL shader code  
**Solution**: Fix shader syntax

#### E403: Memory Allocation Failed
```
Error E403: GPU memory allocation failed
Requested: 256MB, Available: 128MB
```

**Cause**: Insufficient GPU memory  
**Solution**: Reduce texture sizes or use lower quality settings

### Backend-Specific Errors (E450-E499)

#### E450: Raylib Backend Error
```
Error E450: Raylib window creation failed
Invalid window dimensions: -100x600
```

**Cause**: Invalid window configuration  
**Solution**: Use valid positive dimensions

#### E451: HTML Renderer Error
```
Error E451: HTML CSS generation failed
Invalid CSS property value: backgroundColor = 'notacolor'
```

**Cause**: Property value cannot be converted to CSS  
**Solution**: Use valid CSS-compatible values

#### E452: Ratatui Terminal Error
```
Error E452: Terminal backend initialization failed
Terminal size too small: 10x5 (minimum: 80x24)
```

**Cause**: Terminal window too small  
**Solution**: Increase terminal size

## Layout Engine Errors

Layout computation and constraint solving errors.

### Layout Computation Errors (E500-E549)

#### E500: Layout Constraint Conflict
```
Error E500: Conflicting layout constraints for element 'container1'
Cannot satisfy: width = 100px AND width = 200px
```

**Cause**: Contradictory layout properties set  
**Solution**: Remove conflicting constraints

#### E501: Circular Layout Dependency
```
Error E501: Circular layout dependency detected
Elements: container1 -> container2 -> container1
```

**Cause**: Elements depend on each other's sizes  
**Solution**: Break dependency cycle

#### E502: Invalid Flex Properties
```
Error E502: Invalid flexbox configuration for element 'flexContainer'
flexDirection = "invalid", expected: row, column, row-reverse, column-reverse
```

**Cause**: Invalid flexbox property value  
**Solution**: Use valid flexbox values

#### E503: Grid Layout Error
```
Error E503: Grid layout error for element 'gridContainer'
Grid item at (5, 3) exceeds grid dimensions (3x3)
```

**Cause**: Grid item positioned outside grid bounds  
**Solution**: Adjust grid dimensions or item position

### Taffy Engine Errors (E550-E599)

#### E550: Taffy Node Creation Failed
```
Error E550: Failed to create Taffy layout node for element 'myElement'
Maximum node limit (1000) exceeded
```

**Cause**: Too many layout nodes created  
**Solution**: Optimize element hierarchy or increase limit

#### E551: Layout Style Conversion Error
```
Error E551: Failed to convert Kryon style to Taffy style
Unsupported dimension type: Auto for marginTop
```

**Cause**: Property value incompatible with Taffy engine  
**Solution**: Use compatible layout values

## Debugging Techniques

### Enable Debug Logging

Set environment variables for detailed logging:

```bash
# Enable all debug logging
export RUST_LOG=debug
export KRYON_DEBUG=1

# Enable specific subsystems
export RUST_LOG=kryon_runtime::script=debug,kryon_layout=info

# Enable layout debugging
export KRYON_LAYOUT_DEBUG=1
```

### KRY Source Debugging

Add debug annotations to KRY files:

```kry
Container {
    @debug("Layout container for main content")
    backgroundColor: "#f0f0f0"
    
    Text {
        @debug("Title text element")
        text: "Debug Example"
    }
}
```

### Script Debugging

Use Lua debugging functions:

```lua
function handleClick()
    kryon.debug("Button clicked!")
    kryon.log("Counter value: " .. counter)
    
    -- Debug element state
    kryon.debugElement("myButton")
    
    -- Trace execution
    kryon.trace("handleClick", "Processing click event")
end
```

### Runtime Inspection

Access runtime state programmatically:

```rust
// In Rust application
app.debug_dump_elements();
app.debug_layout_tree();
app.debug_script_state();

// Check for errors
if let Err(e) = app.update(delta_time) {
    eprintln!("Runtime error: {}", e);
    app.debug_last_error();
}
```

## Common Issues & Solutions

### Issue: Elements Not Rendering

**Symptoms**: Elements defined but not visible on screen

**Debugging Steps**:
1. Check element `visible` property
2. Verify parent element sizes
3. Check z-index ordering
4. Verify layout constraints

**Solutions**:
```kry
Container {
    @debug("Parent container must have explicit size")
    width: 400px
    height: 300px
    
    Text {
        visible: true  # Explicitly set visibility
        text: "Now visible"
    }
}
```

### Issue: Layout Not Updating

**Symptoms**: Changes to layout properties don't take effect

**Debugging Steps**:
1. Check if `mark_needs_layout()` was called
2. Verify layout engine is processing updates
3. Look for constraint conflicts

**Solutions**:
```lua
function updateLayout()
    -- Force layout recalculation
    kryon.markNeedsLayout()
    
    -- Update properties
    kryon.setProperty("container", "width", "500px")
end
```

### Issue: Scripts Not Executing

**Symptoms**: Event handlers or initialization scripts don't run

**Debugging Steps**:
1. Check script compilation errors
2. Verify function names match event handlers
3. Check script system initialization

**Solutions**:
```kry
Button {
    text: "Click Me"
    onClick: "handleButtonClick"  # Must match Lua function name
    @debug("Handler: handleButtonClick")
}
```

### Issue: Memory Leaks

**Symptoms**: Memory usage grows over time

**Debugging Steps**:
1. Check element creation/destruction balance
2. Monitor texture and resource loading
3. Verify script garbage collection

**Solutions**:
```rust
// Proper resource cleanup
impl Drop for MyRenderer {
    fn drop(&mut self) {
        self.cleanup_textures();
        self.cleanup_buffers();
    }
}
```

### Issue: Performance Degradation

**Symptoms**: Application becomes slow over time

**Debugging Steps**:
1. Profile layout computation frequency
2. Check render call optimization
3. Monitor script execution time

**Solutions**:
```kry
Container {
    @performance("Cache layout results")
    cacheLayout: true
    
    @performance("Limit render updates")
    renderOnDemand: true
}
```

## Performance Troubleshooting

### Layout Performance

**Problem**: Slow layout computation
**Solutions**:
- Use absolute positioning where possible
- Minimize nested flex containers
- Cache layout results for static elements
- Use optimized layout engine

```kry
Container {
    # Fast: Absolute positioning
    position: "absolute"
    left: 10px
    top: 20px
    
    # Avoid: Deep nesting
    # Container { Container { Container { ... } } }
}
```

### Rendering Performance

**Problem**: Low frame rates
**Solutions**:
- Batch render operations
- Use texture atlases
- Minimize state changes
- Optimize shaders

```rust
// Batch rendering
renderer.begin_batch();
for element in elements {
    renderer.queue_element(element);
}
renderer.end_batch(); // Single draw call
```

### Script Performance

**Problem**: Slow script execution
**Solutions**:
- Minimize DOM queries
- Cache element references
- Use efficient algorithms
- Avoid frequent string concatenation

```lua
-- Good: Cache element reference
local button = kryon.getElementById("myButton")
for i = 1, 100 do
    button:setProperty("text", "Count: " .. i)
end

-- Bad: Query every iteration
for i = 1, 100 do
    kryon.getElementById("myButton"):setProperty("text", "Count: " .. i)
end
```

### Memory Performance

**Problem**: High memory usage
**Solutions**:
- Cleanup unused resources
- Use object pooling
- Optimize texture sizes
- Implement proper asset streaming

```rust
// Object pooling for elements
struct ElementPool {
    available: Vec<Element>,
    in_use: Vec<Element>,
}

impl ElementPool {
    fn get_element(&mut self) -> Element {
        self.available.pop().unwrap_or_else(|| Element::new())
    }
    
    fn return_element(&mut self, element: Element) {
        element.reset();
        self.available.push(element);
    }
}
```

## Error Logging Configuration

### Production Logging
```toml
[logging]
level = "warn"
file = "kryon.log"
max_size = "10MB"
rotate = true
```

### Development Logging
```toml
[logging]
level = "debug"
console = true
file = "kryon-debug.log"
include_source = true
include_timestamps = true
```

### Custom Error Handlers
```rust
// Custom error handling
kryon_runtime::set_error_handler(|error| {
    match error {
        KryonError::Script(script_error) => {
            // Send to analytics
            analytics::report_script_error(&script_error);
        }
        KryonError::Render(render_error) => {
            // Fallback to safe renderer
            fallback_renderer::activate();
        }
        _ => {
            // Default error handling
            eprintln!("Kryon error: {}", error);
        }
    }
});
```

This comprehensive error reference covers all aspects of Kryon development, from compilation through runtime execution. Use the error codes to quickly identify issues and follow the debugging techniques to resolve problems efficiently.