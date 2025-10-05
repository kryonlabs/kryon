# KRY Language Specification v0.1
## Smart Hybrid System

## Core Philosophy

**The Best of Both Worlds: CSS-Like Styling + Widget Layout**

KRY combines familiar CSS-like properties for styling with powerful widget elements for layout, giving developers the flexibility of CSS with the structure and performance of widgets.

## Syntax

### Basic Structure
```kry
# CSS-like styles for appearance
style "styleName" {
    background: "#ffffff"
    color: "#000000"
    fontSize: 16
    padding: 12
    borderRadius: 6
}

# Widget elements for layout and structure
Widget {
    property: value
    style: "styleName"
    
    child: ChildWidget { }
    
    # Or multiple children
    children: [
        ChildWidget1 { }
        ChildWidget2 { }
    ]
}
```

### Value Types
- **String**: `"text"` (always quoted)
- **Number**: `42`, `3.14`, `100px`, `50%`, `12rem`, `1.5em` (never quoted)
- **Boolean**: `true`, `false` (never quoted)
- **Color**: `"#RRGGBBAA"`, `"#RGB"`, `"red"`, `"rgb(255,0,0)"`, `"hsl(120,50%,50%)"` (always quoted)
- **Array**: `["item1", "item2", "item3"]`, `[1, 2, 3]`
- **Object**: `{ key: value, key2: value2 }`
- **Function**: `"functionName"` (quoted function reference)
- **Variable**: `variableName` (used directly)
- **String Interpolation**: `"text ${expression} more text"`

## Style System

### Style Definitions
```kry
style "button" {
    background: "#007AFF"
    color: "#ffffff"
    fontSize: 16
    fontWeight: 600
    borderRadius: 6
    padding: "12px 24px"
    border: "none"
    cursor: "pointer"
    transition: "all 0.2s ease"
}

style "primaryButton" extends "button" {
    background: "linear-gradient(135deg, #667eea 0%, #764ba2 100%)"
}

style "card" {
    background: "#ffffff"
    borderRadius: 8
    boxShadow: "0 2px 4px rgba(0,0,0,0.1)"
    padding: 16
    border: "1px solid #e5e5e7"
}
```

### CSS-Like Properties

#### Layout Properties
| Property | Type | Example | Description |
|----------|------|---------|-------------|
| width | Dimension | `200`, `"100%"`, `"50vw"` | Element width |
| height | Dimension | `100`, `"auto"`, `"50vh"` | Element height |
| minWidth | Dimension | `100`, `"200px"` | Minimum width |
| maxWidth | Dimension | `500`, `"100%"` | Maximum width |
| minHeight | Dimension | `50`, `"100px"` | Minimum height |
| maxHeight | Dimension | `300`, `"80vh"` | Maximum height |
| padding | Spacing | `16`, `"10px 20px"` | Inner spacing |
| margin | Spacing | `8`, `"auto"`, `"10px 0"` | Outer spacing |

#### Visual Properties
| Property | Type | Example | Description |
|----------|------|---------|-------------|
| background | Color/Gradient | `"#ffffff"`, `"linear-gradient(...)"` | Background color/gradient |
| color | Color | `"#000000"`, `"red"` | Text color |
| fontSize | Number | `16`, `14` | Font size in pixels |
| fontWeight | Number/String | `600`, `"bold"` | Font weight |
| fontFamily | String | `"Arial"`, `"system-ui"` | Font family |
| lineHeight | Number | `1.5`, `24` | Line height |
| textAlign | String | `"left"`, `"center"`, `"right"` | Text alignment |
| border | String | `"1px solid #ccc"` | Border definition |
| borderRadius | Number | `8`, `6` | Corner radius |
| boxShadow | String | `"0 2px 4px rgba(0,0,0,0.1)"` | Drop shadow |
| opacity | Number | `0.8`, `1` | Transparency |
| cursor | String | `"pointer"`, `"default"` | Mouse cursor |
| transition | String | `"all 0.2s ease"` | CSS transitions |

## Enhanced Theming System

### Theme Variables (Enhanced Variables)
```kry
# Theme as organized variable groups
theme colors {
    primary: "#007AFF"
    secondary: "#34C759"
    success: "#30D158"
    warning: "#FF9500"
    error: "#FF3B30"
    background: "#ffffff"
    surface: "#f2f2f7"
    text: "#000000"
    textSecondary: "#8E8E93"
    border: "#C6C6C8"
}

theme spacing {
    xs: 4
    sm: 8
    md: 16
    lg: 24
    xl: 32
    xxl: 48
}

theme typography {
    caption: 12
    body: 16
    subheading: 18
    h3: 24
    h2: 32
    h1: 48
}

theme radius {
    sm: 4
    md: 8
    lg: 16
    xl: 24
}
```

### Using Theme Variables
```kry
style "primaryButton" {
    background: colors.primary
    color: colors.background
    fontSize: typography.body
    padding: spacing.sm spacing.md
    borderRadius: radius.md
}

Button {
    style: "primaryButton"
    text: "Click Me"
}
```

### Theme Switching
```kry
# Define multiple themes
theme light {
    background: "#ffffff"
    surface: "#f2f2f7"
    text: "#000000"
    textSecondary: "#8E8E93"
    primary: "#007AFF"
}

theme dark {
    background: "#000000"
    surface: "#1C1C1E"
    text: "#ffffff"
    textSecondary: "#8E8E93"
    primary: "#0A84FF"
}

# App can switch themes
App {
    theme: "light"  # or "dark"
    
    Container {
        background: background
        color: text
        
        Button {
            background: primary
            color: background
        }
    }
}
```

### Regular Variables (Existing)
```kry
variables {
    userName: String = "John Doe"
    count: Int = 0
    isLoggedIn: Boolean = false
}

# Shorthand syntax
var apiUrl = "https://api.example.com"
var maxItems = 10
```

## Layout Widgets

### Column - Vertical Layout
```kry
Column {
    spacing: 16                    # Space between children
    mainAxis: "start"              # start, center, end, spaceBetween, spaceAround, spaceEvenly
    crossAxis: "center"            # start, center, end, stretch
    
    children: [
        Text { text: "Item 1" }
        Text { text: "Item 2" }
        Text { text: "Item 3" }
    ]
}
```

### Row - Horizontal Layout  
```kry
Row {
    spacing: 8
    mainAxis: "spaceBetween"
    crossAxis: "center"
    
    children: [
        Button { text: "Cancel" }
        Button { text: "Save", style: "primaryButton" }
    ]
}
```

### Container - Basic Container
```kry
Container {
    width: 200
    height: 100
    background: colors.surface
    borderRadius: radius.md
    padding: spacing.md
    
    child: Text { 
        text: "Hello, World!"
        color: colors.text
    }
}
```

### Center - Center Child
```kry
Center {
    child: Button {
        text: "Centered Button"
        style: "primaryButton"
    }
}
```

### Spacer - Flexible Space
```kry
Row {
    children: [
        Text { text: "Left" }
        Spacer {}  # Pushes content apart
        Text { text: "Right" }
    ]
}
```

### Flex - Flexible Layout
```kry
Flex {
    direction: "row"      # row, column
    wrap: "nowrap"        # nowrap, wrap
    align: "center"       # start, center, end, stretch
    justify: "spaceBetween" # start, center, end, spaceBetween, spaceAround, spaceEvenly
    gap: 12
    
    children: [
        Container { flex: 1, background: colors.primary }
        Container { flex: 2, background: colors.secondary }  
        Container { flex: 1, background: colors.success }
    ]
}
```

## Content Widgets

### Text - Text Display
```kry
Text {
    text: "Hello, World!"
    fontSize: typography.body
    fontWeight: 600
    color: colors.text
    textAlign: "center"
}
```

### Button - Interactive Button
```kry
Button {
    text: "Click Me"
    style: "primaryButton"
    onClick: "handleClick"
    disabled: false
}
```

### Image - Image Display
```kry
Image {
    src: "avatar.jpg"
    width: 100
    height: 100
    borderRadius: radius.xl
    objectFit: "cover"  # cover, contain, fill
}
```

### Input - Text Input
```kry
Input {
    placeholder: "Enter your name"
    value: userName
    onChange: "handleNameChange"
    style: "inputField"
}
```

## Practical Example

```kry
# Theme variables
theme colors {
    primary: "#007AFF"
    background: "#ffffff"
    surface: "#f2f2f7"
    text: "#000000"
    border: "#e5e5e7"
}

theme spacing {
    sm: 8
    md: 16
    lg: 24
}

# Styles
style "card" {
    background: colors.surface
    borderRadius: 12
    padding: spacing.lg
    border: "1px solid ${colors.border}"
    boxShadow: "0 2px 8px rgba(0,0,0,0.1)"
}

style "primaryButton" {
    background: colors.primary
    color: colors.background
    fontSize: 16
    fontWeight: 600
    borderRadius: 8
    padding: spacing.sm spacing.md
    border: "none"
    cursor: "pointer"
}

# Layout with widgets + styles
App {
    windowWidth: 800
    windowHeight: 600
    background: colors.background
    
    Center {
        child: Container {
            style: "card"
            width: 400
            
            Column {
                spacing: spacing.md
                
                Text {
                    text: "User Profile"
                    fontSize: 24
                    fontWeight: 700
                    color: colors.text
                }
                
                Row {
                    spacing: spacing.sm
                    crossAxis: "center"
                    
                    Image {
                        src: "avatar.jpg"
                        width: 60
                        height: 60
                        borderRadius: 30
                    }
                    
                    Column {
                        spacing: 4
                        
                        Text { text: "John Doe", fontWeight: 600 }
                        Text { text: "Designer", color: colors.textSecondary }
                    }
                    
                    Spacer {}
                    
                    Button {
                        text: "Edit"
                        style: "primaryButton"
                        onClick: "handleEdit"
                    }
                }
            }
        }
    }
}
```

## Advanced Features

### Style Inheritance
```kry
style "button" {
    fontSize: 16
    padding: "8px 16px"
    borderRadius: 6
    border: "none"
    cursor: "pointer"
}

style "primaryButton" extends "button" {
    background: colors.primary
    color: colors.background
}

style "secondaryButton" extends "button" {
    background: "transparent"
    color: colors.primary
    border: "1px solid ${colors.primary}"
}
```

### Responsive Properties
```kry
Container {
    width: {
        mobile: "100%"
        tablet: 600
        desktop: 800
    }
    padding: {
        mobile: spacing.sm
        tablet: spacing.md
        desktop: spacing.lg
    }
}
```

### Conditional Styling
```kry
Button {
    text: "Submit"
    style: isValid ? "primaryButton" : "disabledButton"
    disabled: !isValid
}
```

## Functions and Event Handlers

### Function Definitions
```kry
function handleClick() {
    console.log("Button clicked!");
}

function calculateTotal(items) {
    local total = 0
    for i, item in ipairs(items) do
        total = total + item.price
    end
    return total
}
```

### Event Handling

KRY supports two forms of event handler syntax:

#### Short Form (Simple Function Reference)
For simple cases, use a string reference to the function name:

```kry
Button {
    text: "Click Me"
    onClick: "handleClick"
    onMouseEnter: "handleHover"
    onMouseLeave: "handleUnhover"
}

Input {
    value: inputValue
    onChange: "handleInputChange"
    onFocus: "handleFocus"
    onBlur: "handleBlur"
}
```

#### Long Form (Advanced Event Configuration)
For advanced scenarios with parameters, async handling, or event options:

```kry
Button {
    text: "Submit Form"
    onClick: {
        handler: "submitForm"
        args: ["userId", "formData"]
        async: true
        preventDefault: true
        debounce: 300
    }
}

Button {
    text: "Delete Item"
    onClick: {
        handler: "deleteItem"
        args: [itemId]
        confirm: "Are you sure you want to delete this item?"
        async: true
    }
}
```

This hybrid system gives you the familiar styling power of CSS with the structural benefits of widgets, plus an enhanced theming system that builds naturally on your existing variables.

## Component System

### component Syntax
```kry
component UserCard {
    prop name
    prop avatar
    prop role

    Container {
        style = "card"

        Row {
            spacing = spacing.sm
            crossAxis = "center"

            Image {
                src = avatar
                width = 60
                height = 60
                borderRadius = 30
            }

            Column {
                spacing = 4

                Text { text = name, fontWeight = 600 }
                Text { text = role, color = colors.textSecondary }
            }
        }
    }
}

# Usage
UserCard {
    name = "John Doe"
    avatar = "avatar.jpg"
    role = "Designer"
}
```

## Reactive System

### Reactive Variables & Functions
```kry
variables {
    count = 0           # Reactive - changing triggers re-render
    currentTheme = "light"  # Reactive theme switcher
}

function increment() {
    count = count + 1  # Direct assignment updates reactive variable
}

function toggleTheme() {
    currentTheme = currentTheme == "light" ? "dark" : "light"
}

Button {
    text = "Count: count"  # Automatically updates when count changes
    onClick = "increment"   # Function directly modifies reactive variable
}
```

### Advanced Theme Switching
```kry
# Define themes
theme light {
    background: "#ffffff"
    text: "#000000" 
    primary: "#007AFF"
}

theme dark {
    background: "#000000"
    text: "#ffffff"
    primary: "#0A84FF" 
}

# App-level theme switching
App {
    theme: currentTheme  # reactive variable controls active theme
    
    Container {
        background: background  # automatically uses active theme
        color: text
    }
}

# Theme switching function
function setTheme(themeName) {
    currentTheme = themeName  # Updates reactive variable, triggers re-render
}
```

## Property Precedence Rules

When the same property is defined in multiple places, KRY follows this precedence order (highest to lowest):

1. **Widget Properties** (highest precedence)
2. **Style Properties** 
3. **Theme Variables** (lowest precedence)

```kry
style "button" { fontSize: 16, color: "blue" }

Button {
    style: "button"
    fontSize: 20      # Widget property OVERRIDES style property
    text: "Click"     # Final: fontSize=20, color="blue"
}
```

### Key Points:
- **variables are reactive by default** - any change triggers re-render
- **Functions directly modify variables** - no setState needed
- **Theme switching via reactive theme property** on App
- **Widget properties override style properties**
