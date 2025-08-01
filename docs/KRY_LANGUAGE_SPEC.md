# KRY Language Specification v1.0

## Syntax

### Basic Structure
```
Element {
    property: value
    property: value
    
    ChildElement {
        property: value
    }
}
```

### Value Types
- **String**: `"text"` (always quoted)
- **Number**: `42`, `3.14`, `100px`, `50%`, `12rem`, `1.5em` (never quoted)
- **Boolean**: `true`, `false` (never quoted)
- **Color**: `"#RRGGBBAA"`, `"#RGB"`, `"red"`, `"rgb(255,0,0)"` (always quoted)
- **Array**: `["item1", "item2", "item3"]`
- **Object**: `{ key: value, key2: value2 }`
- **Reference**: `$variableName`
- **Expression**: `${expression}`

### Elements

#### Core Elements
- `App` - Application root container
- `Container` - Layout container  
- `Text` - Text display
- `Button` - Interactive button
- `Input` - Text input field
- `Image` - Image display
- `Link` - Navigation link
- `EmbedView` - Platform-specific embed

#### Form Elements
- `Form` - Form container
- `Select` - Dropdown selection
- `Checkbox` - Boolean checkbox
- `Radio` - Radio button
- `Slider` - Range slider
- `TextArea` - Multi-line input
- `FileInput` - File upload
- `DatePicker` - Date selection
- `ColorPicker` - Color selection

#### Media Elements
- `Video` - Video player
- `Audio` - Audio player
- `Canvas` - Drawing surface
- `Svg` - SVG graphics
- `WebView` - Web content embed

#### Layout Elements
- `ScrollView` - Scrollable container
- `Grid` - Grid layout
- `Stack` - Stacked/layered layout
- `Spacer` - Flexible space

#### Semantic Elements
- `Header` - Header section
- `Footer` - Footer section
- `Nav` - Navigation section
- `Section` - Content section
- `Article` - Article content
- `Aside` - Sidebar content
- `List` - List container
- `ListItem` - List item
- `Table` - Table container
- `TableRow` - Table row
- `TableCell` - Table cell
- `TableHeader` - Table header

### Properties

#### Universal Properties
- `id` - Element identifier
- `class` - Style classes
- `style` - Inline style reference
- `visible` / `visibility` - Show/hide element
- `enabled` / `disabled` - Enable/disable element
- `tooltip` - Tooltip text

#### Layout Properties
- `width`, `height` - Dimensions (px, %, auto, min-content, max-content)
- `minWidth`, `maxWidth`, `minHeight`, `maxHeight` - Size constraints
- `display` - "flex", "block", "inline", "inline-block", "grid", "none"
- `position` - "relative", "absolute", "fixed", "sticky"
- `x`, `y` / `left`, `top`, `right`, `bottom` - Position
- `zIndex` - Stacking order

#### Flexbox Properties
- `flexDirection` - "row", "column", "row-reverse", "column-reverse"
- `flexWrap` - "nowrap", "wrap", "wrap-reverse"
- `alignItems` - "start", "center", "end", "stretch", "baseline"
- `justifyContent` - "start", "center", "end", "space-between", "space-around", "space-evenly"
- `alignContent` - "start", "center", "end", "stretch", "space-between", "space-around"
- `alignSelf` - "auto", "start", "center", "end", "stretch", "baseline"
- `flexGrow` - Growth factor
- `flexShrink` - Shrink factor
- `flexBasis` - Base size
- `gap` / `rowGap` / `columnGap` - Spacing between items

#### Grid Properties
- `gridTemplateColumns` - Column definition
- `gridTemplateRows` - Row definition
- `gridColumn` - Column placement
- `gridRow` - Row placement
- `gridArea` - Area placement
- `gridGap` - Grid spacing

#### Box Model Properties
- `padding` / `paddingTop`, `paddingRight`, `paddingBottom`, `paddingLeft`
- `margin` / `marginTop`, `marginRight`, `marginBottom`, `marginLeft`
- `border` / `borderTop`, `borderRight`, `borderBottom`, `borderLeft`
- `borderWidth` / `borderTopWidth`, etc.
- `borderColor` / `borderTopColor`, etc.
- `borderStyle` - "solid", "dashed", "dotted", "none"
- `borderRadius` / `borderTopLeftRadius`, etc.
- `boxShadow` - Shadow definition
- `overflow` - "visible", "hidden", "scroll", "auto"
- `overflowX`, `overflowY` - Axis-specific overflow

#### Visual Properties
- `backgroundColor` / `bg` - Background color
- `backgroundImage` - Background image URL
- `backgroundSize` - "cover", "contain", "auto", dimensions
- `backgroundPosition` - Position of background
- `backgroundRepeat` - "repeat", "repeat-x", "repeat-y", "no-repeat"
- `gradient` - Gradient definition
- `opacity` - Transparency (0.0-1.0)
- `filter` - CSS filters
- `backdropFilter` - Backdrop filters
- `transform` - Transform string
- `transformOrigin` - Transform origin
- `transition` - Transition definition
- `animation` - Animation name

#### Text Properties
- `text` / `textContent` - Text content
- `textColor` / `color` - Text color
- `fontSize` / `size` - Font size
- `fontWeight` / `weight` - Font weight (100-900)
- `fontFamily` / `font` - Font family
- `fontStyle` - "normal", "italic", "oblique"
- `textAlign` / `align` - "left", "center", "right", "justify"
- `textDecoration` - "none", "underline", "overline", "line-through"
- `textTransform` - "none", "uppercase", "lowercase", "capitalize"
- `lineHeight` - Line height
- `letterSpacing` - Letter spacing
- `wordSpacing` - Word spacing
- `whiteSpace` - "normal", "nowrap", "pre", "pre-wrap"
- `textOverflow` - "clip", "ellipsis"
- `wordBreak` - "normal", "break-all", "keep-all"

#### Interactive Properties
- `onClick` - Click event handler
- `onDoubleClick` - Double click handler
- `onMouseEnter` / `onHover` - Mouse enter
- `onMouseLeave` - Mouse leave
- `onMouseMove` - Mouse move
- `onMouseDown` - Mouse button down
- `onMouseUp` - Mouse button up
- `onFocus` - Focus event
- `onBlur` - Blur event
- `onChange` - Value change
- `onInput` - Input event
- `onKeyDown` - Key down
- `onKeyUp` - Key up
- `onKeyPress` - Key press
- `onScroll` - Scroll event
- `onLoad` - Load complete
- `onError` - Error event
- `cursor` - Cursor type
- `pointerEvents` - "auto", "none"
- `userSelect` - "auto", "none", "text", "all"

#### Element-Specific Properties

**App**
- `windowTitle` / `title` - Window title
- `windowWidth` / `windowHeight` - Window dimensions
- `windowResizable` / `resizable` - Allow resize
- `windowFullscreen` / `fullscreen` - Fullscreen mode
- `windowMinWidth` / `windowMinHeight` - Minimum size
- `windowMaxWidth` / `windowMaxHeight` - Maximum size
- `windowIcon` - Icon path
- `theme` - Theme name

**Input/TextArea**
- `value` - Current value
- `placeholder` / `hint` - Placeholder text
- `type` - "text", "password", "email", "number", "tel", "url", "search", "date", "time"
- `required` - Required field
- `readonly` - Read-only
- `maxLength` - Maximum length
- `minLength` - Minimum length
- `pattern` - Validation pattern
- `autocomplete` - Autocomplete hint
- `spellcheck` - Enable spellcheck

**Select**
- `options` - Array of options
- `selected` / `value` - Selected value
- `multiple` - Multi-select
- `size` - Visible options

**Image**
- `src` / `imageSource` - Image URL
- `alt` - Alternative text
- `objectFit` - "fill", "contain", "cover", "none", "scale-down"
- `objectPosition` - Position within container
- `loading` - "eager", "lazy"

**Link**
- `href` - Target URL
- `target` - "_self", "_blank", "_parent", "_top"
- `download` - Download filename
- `rel` - Relationship

**Video/Audio**
- `src` - Media URL
- `autoplay` - Auto play
- `controls` - Show controls
- `loop` - Loop playback
- `muted` - Start muted
- `poster` - Poster image (video)
- `preload` - "none", "metadata", "auto"

**List**
- `listStyle` - "disc", "circle", "square", "decimal", "none"
- `listStylePosition` - "inside", "outside"

**Table**
- `borderCollapse` - "collapse", "separate"
- `borderSpacing` - Cell spacing

### Directives

#### Style Definition
```
style "styleName" {
    property: value
    property: value
    
    :hover {
        property: value
    }
    
    :active {
        property: value
    }
    
    :focus {
        property: value
    }
    
    :disabled {
        property: value
    }
    
    :checked {
        property: value
    }
}
```

#### Variables
```
@variables {
    varName: value
    typedVar: Type = value
    count: Int = 0
    theme: String = "dark"
    enabled: Bool = true
    colors: Array<String> = ["red", "green", "blue"]
    config: Object = {
        timeout: 5000,
        retries: 3
    }
}

# Or shorthand
@var count = 0
@var theme = "dark"
```

#### Metadata
```
@metadata {
    version: "1.0.0"
    author: "Developer Name"
    description: "App description"
    icon: "icon.png"
    bundle: ["page1.krb", "page2.krb"]
    scripts: ["utils.js", "helpers.lua"]
    permissions: ["camera", "microphone"]
}
```

#### Conditionals
```
@if condition {
    Element { }
}
@elif condition {
    Element { }  
}
@else {
    Element { }
}

# Compile-time conditionals
@const_if PLATFORM == "web" {
    WebView { }
}
```

#### Loops
```
@for item in collection {
    Text { text: $item }
}

@for i in 0..10 {
    Text { text: "Item ${i}" }
}

@while condition {
    Element { }
}
```

#### Functions
```
@function "lua" functionName(param1, param2) {
    -- Lua code
}

@func "javascript" handleClick() {
    // JavaScript code
}

@function "python" processData(data) {
    # Python code
}

@func "wren" calculate(x, y) {
    // Wren code
}
```

#### Script Blocks
```
@script "lua" {
    -- Global Lua code
    function setup()
        print("Initialized")
    end
}
```

#### Components
```
@component Button {
    @props {
        text: String = "Click me"
        variant: String = "primary"
        size: String = "medium"
        disabled: Bool = false
    }
    
    Container {
        class: "button button-${props.variant} button-${props.size}"
        onClick: props.onClick
        opacity: props.disabled ? 0.5 : 1.0
        
        Text {
            text: props.text
        }
    }
}

# Using component
Button {
    text: "Submit"
    variant: "success"
    onClick: "handleSubmit"
}
```

#### Slots
```
@component Card {
    @props {
        title: String
    }
    
    @slots {
        header
        default
        footer
    }
    
    Container {
        @slot header {
            Text { text: props.title }
        }
        
        Container {
            @slot default
        }
        
        @slot footer
    }
}

# Using slots
Card {
    title: "My Card"
    
    @slot default {
        Text { text: "Card content" }
    }
    
    @slot footer {
        Button { text: "Action" }
    }
}
```

#### Imports
```
@include "./components/button.kry"
@include "./styles/theme.kry"
@include "./shared/utils.kry"
```

#### Lifecycle Hooks
```
@onMount {
    // Called when element mounts
}

@onUnmount {
    // Called when element unmounts
}

@onUpdate {
    // Called when element updates
}
```

#### Debug Directives
```
@debug {
    // Debug-only code
}

@trace_renders {
    // Track render cycles
}

@performance_monitor {
    // Performance tracking
}
```

### Expressions

#### Variable References
```
text: $variableName
width: $containerWidth
color: $theme.primaryColor
```

#### String Interpolation
```
text: "Hello, ${userName}!"
class: "button button-${size} button-${variant}"
```

#### Arithmetic
```
width: ${baseWidth + padding * 2}
height: ${screenHeight - headerHeight - footerHeight}
x: ${index * itemWidth + spacing}
```

#### Comparison
```
visible: $count > 0
disabled: $age < 18
opacity: $loading ? 0.5 : 1.0
```

#### Logical
```
enabled: $isLoggedIn && $hasPermission
visible: $showMenu || $debugMode
valid: !$errors
```

#### Ternary
```
text: $isLoggedIn ? "Logout" : "Login"
color: $isDarkMode ? "#FFFFFF" : "#000000"
```

#### Array/Object Access
```
text: $items[0]
color: $theme.colors.primary
value: $user?.email ?? "No email"
```

#### Method Calls
```
text: $name.toUpperCase()
items: $data.filter(item => item.active)
```

### Property Aliases

#### Universal Aliases
| Alias | Full Property |
|-------|---------------|
| `bg` | `backgroundColor` |
| `color` | `textColor` |
| `size` | `fontSize` |
| `weight` | `fontWeight` |
| `align` | `textAlign` |
| `font` | `fontFamily` |
| `w` | `width` |
| `h` | `height` |
| `x` | `left` |
| `y` | `top` |
| `p` | `padding` |
| `m` | `margin` |
| `pt`, `pr`, `pb`, `pl` | padding sides |
| `mt`, `mr`, `mb`, `ml` | margin sides |

#### Element-Specific Aliases
| Element | Alias | Property |
|---------|-------|----------|
| App | `title` | `windowTitle` |
| Image | `src` | `imageSource` |
| Input | `hint` | `placeholder` |
| Container | `direction` | `flexDirection` |
| Container | `justify` | `justifyContent` |
| Container | `items` | `alignItems` |

### Comments
```
# Single line comment
// Also single line comment
```

### Reserved Keywords
- All element names
- All directive names (@if, @for, etc.)
- Boolean values (true, false)
- null, undefined
- All event handler names (onClick, etc.)