# .kryon Layout Format

## Purpose

.kryon files are **simple layout descriptors**, not programming languages. They declare widget hierarchies and assign IDs that controllers can reference. All application logic and event handling lives in controller code (Lua, Scheme, C, or any language).

**Key Principles:**
- `.kryon` = Layout declaration + Widget IDs
- Controllers (Lua/Scheme/C) = All logic and event handling
- Clean separation: Structure vs. Behavior

## Syntax

### Line Format

For simple widgets without children:

```
widget_type widget_id "text content" prop=value prop2=value
```

**Examples:**
```kryon
button btn_submit "Submit" at (10, 10)
label lbl_status "Ready" at (10, 50)
textinput txt_name placeholder="Enter name" at (10, 100)
```

### Block Format (Nested Widgets)

For widgets with children, use curly braces:

```kryon
widget_type widget_id prop=value {
    child_widget_type child_id "text"
    child_widget_type child_id2 "text"
}
```

**Example:**
```kryon
window "My App" 800 600 {
    column padding=20 {
        label "Username:"
        textinput txt_user placeholder="Enter username"
        button btn_login "Login"
    }
}
```

### Window Syntax

Windows are the root containers:

```kryon
window "Title" width height
```

Or with nested children:

```kryon
window "Title" width height {
    // widgets here
}
```

**Examples:**
```kryon
# Simple window (no children declared here)
window "Hello" 400 300
label lbl_greeting "Hello, World!" at (150, 140)

# Window with nested layout
window "App" 800 600 {
    column {
        button btn_click "Click Me"
        label lbl_status "Status: Ready"
    }
}
```

## Widget IDs

Widget IDs are **identifiers** that controllers use to reference widgets:

**Rules:**
- Must start with a letter or underscore
- Can contain letters, numbers, and underscores
- Case-sensitive
- Must be unique within a window

**Examples:**
```kryon
button btn_submit      # Good
label lbl_status_1    # Good
textinput txtUserName # Good
button 123_btn        # Bad - starts with number
button btn-submit     # Bad - contains hyphen
```

## Property Syntax

Properties are key-value pairs:

```
prop=value
prop="string value"
prop=(x, y)           # Position/size
```

**Examples:**
```kryon
button btn_click "Click" at=(10, 10) enabled=true
textinput txt_name placeholder="Name" visible=true
label lbl_title "Header" font="Arial 20 bold" color="#FF0000"
```

## Layout Containers

### Column (Vertical Layout)

```kryon
column gap=10 padding=20 {
    label "First"
    label "Second"
    label "Third"
}
```

### Row (Horizontal Layout)

```kryon
row gap=10 {
    button "Left"
    button "Middle"
    button "Right"
}
```

### Container (Generic)

```kryon
container padding=10 {
    label "Content"
}
```

## Positioning

### Absolute Positioning

```kryon
button btn_click "Click" at (100, 50)
label lbl_title "Title" at (50, 10)
```

### Layout-Based Positioning

```kryon
column {
    label "Auto-positioned"
    label "Auto-positioned too"
}
```

## Comments

```kryon
# Single-line comment
// Also single-line comment

button btn_click "Click" # Comment after syntax
```

## Controller Integration

Controllers reference widgets by ID to:
- Read/write properties
- Attach event handlers
- Query state

### Lua Example

**layout.kryon:**
```kryon
window "Counter" 300 200 {
    column gap=10 {
        label lbl_count "Count: 0"
        button btn_inc "+"
        button btn_dec "-"
    }
}
```

**controller.lua:**
```lua
local kryon = require("kryon")

-- Look up widgets by ID
local lbl = kryon.widget("lbl_count")
local btn_inc = kryon.widget("btn_inc")
local btn_dec = kryon.widget("btn_dec")

local count = 0

-- Attach event handlers
btn_inc.on_click(function()
    count = count + 1
    lbl.text = "Count: " .. count
end)

btn_dec.on_click(function()
    count = count - 1
    lbl.text = "Count: " .. count
end)

-- Event loop
kryon.run()
```

### Scheme Example

**controller.scm:**
```scheme
(define lbl (kryon-widget "lbl_count"))
(define btn-inc (kryon-widget "btn_inc"))
(define count 0)

(kryon-on-click btn-inc
  (lambda ()
    (set! count (+ count 1))
    (kryon-set-property! lbl "text" (string-append "Count: " (number->string count)))))
```

## Comparison

### .kryon vs HTML

| .kryon | HTML |
|--------|------|
| Layout + IDs | Structure + Content + Scripts |
| No event handlers | Inline event handlers (`onclick`) |
| Controllers are separate | JavaScript embedded or linked |
| IDs for controllers | IDs for CSS/scripts |

### .kryon vs JSON

| .kryon | JSON |
|--------|------|
| Human-readable syntax | Machine-oriented syntax |
| Comments allowed | No comments |
| Concise widget syntax | Verbose nesting |
| Designed for hand-editing | Designed for tool generation |

## Complete Example

**app.kryon:**
```kryon
# Login form layout
window "Login" 400 300 {
    column padding=20 gap=15 {
        label lbl_title "Welcome Back!" font="Arial 20 bold"

        formgroup "Username" {
            textinput txt_user placeholder="Enter username"
        }

        formgroup "Password" {
            passwordinput txt_pass placeholder="Enter password"
        }

        checkbox chk_remember "Remember me"

        row gap=10 {
            button btn_login "Login" style="primary"
            button btn_cancel "Cancel" style="secondary"
        }
    }
}
```

**app.lua:**
```lua
local kryon = require("kryon")

local txt_user = kryon.widget("txt_user")
local txt_pass = kryon.widget("txt_pass")
local chk_remember = kryon.widget("chk_remember")

kryon.widget("btn_login").on_click(function()
    local username = txt_user.value
    local password = txt_pass.value
    local remember = chk_remember.checked

    -- Validate and login
    if login(username, password, remember) then
        print("Login successful!")
    else
        kryon.widget("lbl_title").text = "Login Failed!"
    end
end)

kryon.widget("btn_cancel").on_click(function()
    kryon.quit()
end)

kryon.run()
```

## What .kryon Does NOT Include

- ❌ No variables or expressions
- ❌ No functions or methods
- ❌ No conditionals or loops
- ❌ No event handlers (`on_click`, etc.)
- ❌ No import/include statements
- ❌ No type annotations
- ❌ No calculations or transformations

**All logic lives in controller code.**

## Summary

.kryon files declare **what** the UI looks like and assign IDs. Controllers declare **how** it behaves.

```kryon
# layout.kryon - Structure
window "App" 800 600 {
    button btn_click "Click Me"
    label lbl_status "Ready"
}
```

```lua
-- controller.lua - Behavior
local btn = kryon.widget("btn_click")
local lbl = kryon.widget("lbl_status")

btn.on_click(function()
    lbl.text = "Clicked!"
end)
```

**Clean separation enables:**
- Any programming language as controller
- Simple tooling (no compiler needed)
- Clear separation of concerns
- Easy automation and code generation
