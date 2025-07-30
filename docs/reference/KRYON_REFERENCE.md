# Complete Kryon UI Reference

This comprehensive reference shows all Kryon elements, their properties, inheritance relationships, and how they work together.

> **🔗 Related Documentation**: 
> - [KRB_TO_HTML_MAPPING.md](KRB_TO_HTML_MAPPING.md) - KRB-to-HTML conversion reference
> - [KRY_LANGUAGE_SPECIFICATION.md](KRY_LANGUAGE_SPECIFICATION.md) - Complete language specification

## Table of Contents
- [Element Hierarchy & Inheritance](#element-hierarchy--inheritance)
- [Universal Properties](#universal-properties)
- [Core Elements](#core-elements)
- [Interactive Elements](#interactive-elements)
- [Media Elements](#media-elements)
- [Form Controls](#form-controls)
- [Semantic Elements](#semantic-elements)
- [Table Elements](#table-elements)
- [Property Reference](#property-reference)
- [Aliases & Shortcuts](#aliases--shortcuts)

## Element Hierarchy & Inheritance

All Kryon elements inherit from a base element that provides universal properties. Here's how inheritance works:

```
BaseElement (Universal Properties)
├── App (0x00) - Window & application properties
├── Container (0x01) - Layout & flex properties  
├── Text (0x02) - Typography properties
├── EmbedView (0x03) - Platform-specific properties
├── Interactive Elements (0x10-0x1F)
│   ├── Button (0x10) - Text + interaction properties
│   ├── Input (0x11) - Text + input-specific properties
│   └── Link (0x12) - Text + navigation properties
├── Media Elements (0x20-0x2F)
│   ├── Image (0x20) - Media + sizing properties
│   ├── Video (0x21) - Media + playback properties
│   ├── Canvas (0x22) - Drawing context properties
│   └── Svg (0x23) - Vector graphics properties
├── Form Controls (0x30-0x3F)
│   ├── Select (0x30) - Input + options properties
│   ├── Slider (0x31) - Input + range properties
│   ├── Checkbox (0x33) - Input + boolean properties
│   └── ... (other form controls)
├── Semantic Elements (0x40-0x4F)
│   ├── Form (0x40) - Container + form properties
│   ├── List (0x41) - Container + list properties
│   └── ListItem (0x42) - Text + list item properties
└── Table Elements (0x50-0x5F)
    ├── Table (0x50) - Container + table properties
    ├── TableRow (0x51) - Container + row properties
    ├── TableCell (0x52) - Text + cell properties
    └── TableHeader (0x53) - Text + header properties
```

### Inheritance Rules
- ✅ **Inherited Properties**: Pass down to child elements automatically
- ❌ **Non-Inherited Properties**: Must be explicitly set on each element
- 🔄 **Override**: Child elements can override inherited values

## Universal Properties

These properties are available on **ALL** elements and form the foundation of the Kryon UI system:

### Visual Properties (0x01-0x0F)
| Property | Hex | Type | Inherited | Description | Example |
|----------|-----|------|-----------|-------------|---------|
| `backgroundColor` | `0x01` | Color | ❌ | Element background | `backgroundColor: "#FF0000"` |
| `textColor` | `0x02` | Color | ✅ | Text color | `textColor: "white"` |
| `borderColor` | `0x03` | Color | ❌ | Border color | `borderColor: "black"` |
| `borderWidth` | `0x04` | Float | ❌ | Border thickness | `borderWidth: 2` |
| `borderRadius` | `0x05` | Float | ❌ | Corner radius | `borderRadius: 8` |
| `opacity` | `0x06` | Float | ✅ | Transparency | `opacity: 0.8` |
| `visibility` | `0x07` | Bool | ✅ | Show/hide element | `visibility: false` |
| `cursor` | `0x08` | String | ✅ | Mouse cursor | `cursor: "pointer"` |

### Layout Properties (0x20-0x2F)
| Property | Hex | Type | Inherited | Description | Example |
|----------|-----|------|-----------|-------------|---------|
| `width` | `0x20` | Dimension | ❌ | Element width | `width: 100px` |
| `height` | `0x21` | Dimension | ❌ | Element height | `height: 200px` |
| `display` | `0x25` | String | ❌ | Display mode | `display: "flex"` |
| `position` | `0x26` | String | ❌ | Position type | `position: "absolute"` |
| `zIndex` | `0x24` | Int | ❌ | Stacking order | `zIndex: 10` |

### Spacing Properties (0x70-0x7F)
| Property | Hex | Type | Inherited | Description | Example |
|----------|-----|------|-----------|-------------|---------|
| `padding` | `0x70` | Dimension | ❌ | All padding | `padding: 10px` |
| `margin` | `0x75` | Dimension | ❌ | All margins | `margin: 20px` |

## Core Elements

These elements form the foundation - all UI patterns can theoretically be built using just these:

### App (0x00) - Application Root

**Purpose**: Root container for the entire application
**Inherits**: Universal properties
**Additional Properties**:

| Property | Hex | Type | Description | Example |
|----------|-----|------|-------------|---------|
| `windowWidth` | `0xA0` | Int | Window width | `windowWidth: 1024` |
| `windowHeight` | `0xA1` | Int | Window height | `windowHeight: 768` |
| `windowTitle` | `0xA2` | String | Window title | `windowTitle: "My App"` |
| `windowResizable` | `0xA3` | Bool | Allow resize | `windowResizable: true` |

**Example**:
```kry
App {
    windowTitle: "My Application" 
    windowWidth: 1200
    windowHeight: 800
    backgroundColor: "#f5f5f5"
    
    Container {
        // App contents...
    }
}
```

### Container (0x01) - Layout Container

**Purpose**: Groups and layouts child elements
**Inherits**: Universal properties
**Additional Properties**:

| Property | Hex | Type | Description | Example |
|----------|-----|------|-------------|---------|
| `flexDirection` | `0x40` | String | Layout direction | `flexDirection: "column"` |
| `alignItems` | `0x45` | String | Cross-axis alignment | `alignItems: "center"` |
| `justifyContent` | `0x48` | String | Main-axis alignment | `justifyContent: "space-between"` |
| `gap` | `0x23` | Float | Space between children | `gap: 16` |

**Example**:
```kry
Container {
    display: "flex"
    flexDirection: "row"
    alignItems: "center"
    gap: 20
    padding: 16
    
    Text { text: "Label" }
    Button { text: "Action" }
}
```

### Text (0x02) - Text Display

**Purpose**: Displays text content with typography
**Inherits**: Universal properties + text color inheritance
**Additional Properties**:

| Property | Hex | Type | Inherited | Description | Example |
|----------|-----|------|-----------|-------------|---------|
| `textContent` | `0x10` | String | ❌ | Text content | `textContent: "Hello"` |
| `fontSize` | `0x11` | Float | ✅ | Font size | `fontSize: 16` |
| `fontWeight` | `0x12` | Int | ✅ | Font weight | `fontWeight: 700` |
| `fontFamily` | `0x14` | String | ✅ | Font family | `fontFamily: "Arial"` |
| `textAlignment` | `0x13` | String | ✅ | Text alignment | `textAlignment: "center"` |

**Example**:
```kry
Text {
    text: "Welcome to Kryon"     # Short alias
    fontSize: 24
    fontWeight: 700
    textColor: "#333"
    textAlignment: "center"
}
```

## Interactive Elements

Elements that respond to user interaction, inheriting base properties plus interaction capabilities:

### Button (0x10) - Interactive Button

**Purpose**: Clickable interface element
**Inherits**: Universal properties + Text properties
**Additional Properties**:

| Property | Hex | Type | Description | Example |
|----------|-----|------|-------------|---------|
| `onClick` | Event | String | Click handler | `onClick: "handleClick()"` |
| `disabled` | State | Bool | Disabled state | `disabled: false` |
| `type` | `0xC5` | String | Button type | `type: "submit"` |

**Example**:
```kry
Button {
    text: "Submit Form"
    backgroundColor: "#007bff"
    textColor: "white"
    padding: 12
    borderRadius: 6
    onClick: "submitForm()"
    
    # Hover state
    @hover {
        backgroundColor: "#0056b3"
    }
}
```

### Input (0x11) - Text Input Field

**Purpose**: User text input
**Inherits**: Universal properties + Text properties
**Additional Properties**:

| Property | Hex | Type | Description | Example |
|----------|-----|------|-------------|---------|
| `placeholder` | String | String | Placeholder text | `placeholder: "Enter email..."` |
| `value` | String | String | Current value | `value: $email` |
| `type` | `0xC8` | String | Input type | `type: "email"` |
| `onChange` | Event | String | Change handler | `onChange: "handleChange()"` |

**Example**:
```kry
Input {
    type: "email"
    placeholder: "Enter your email"
    value: $userEmail
    borderWidth: 1
    borderColor: "#ddd"
    padding: 8
    onChange: "updateEmail()"
}
```

## Media Elements

Elements for rich media content:

### Image (0x20) - Image Display

**Purpose**: Display images
**Inherits**: Universal properties
**Additional Properties**:

| Property | Hex | Type | Description | Example |
|----------|-----|------|-------------|---------|
| `imageSource` | `0x0A` | String | Image URL/path | `imageSource: "logo.png"` |
| `alt` | String | String | Alt text | `alt: "Company logo"` |
| `objectFit` | `0xEC` | String | How image fits | `objectFit: "cover"` |

**Example**:
```kry
Image {
    src: "hero-image.jpg"        # Short alias
    alt: "Product showcase"
    width: 400
    height: 300
    objectFit: "cover"
    borderRadius: 8
}
```

### Link (0x11) - Navigation and KRB Linking

**Purpose**: Navigation links with intelligent KRB file linking and bundling
**Inherits**: Universal properties + Text properties
**Additional Properties**:

| Property | Hex | Type | Description | Example |
|----------|-----|------|-------------|---------|
| href | 0x50 | String | Target URL or KRB file path | `"page2.krb"` |
| target | 0x51 | String | Link target (`_self`, `_blank`, `_parent`) | `"_blank"` |
| bundleHint | 0x52 | String | Bundling strategy (`static`, `dynamic`, `lazy`) | `"static"` |

**KRB File Linking**:
Links can reference other KRB files, enabling multi-page applications with intelligent bundling:

```kry
// Static link - analyzed for bundling
Link {
    href: "dashboard.krb"
    text: "Go to Dashboard"
    bundleHint: "static"  // Include in bundle
}

// Dynamic link - requires explicit app metadata
Link {
    href: $userRole == "admin" ? "admin.krb" : "user.krb"
    text: "Control Panel"
    bundleHint: "dynamic"  // Warn if not in metadata
}

// External link
Link {
    href: "https://example.com"
    target: "_blank"
    text: "External Site"
}
```

**Bundle Analysis**:
- **Static links**: Automatically included in bundle analysis
- **Dynamic links**: Require explicit declaration in app metadata
- **Lazy links**: Loaded on-demand, not bundled

**Example**:
```kry
Link {
    href: "profile.krb"
    text: "View Profile"
    color: "#0066cc"
    textDecoration: "underline"
    bundleHint: "static"
}
```

## Form Controls

Specialized input elements:

### Select (0x30) - Dropdown Selection

**Purpose**: Choose from multiple options
**Inherits**: Universal properties + Input-like properties
**Additional Properties**:

| Property | Type | Description | Example |
|----------|------|-------------|---------|
| `options` | Array | Available options | `options: ["Red", "Green", "Blue"]` |
| `selected` | String | Selected value | `selected: $color` |
| `multiple` | Bool | Multi-select | `multiple: false` |

**Example**:
```kry
Select {
    options: ["Small", "Medium", "Large"]
    selected: $selectedSize
    placeholder: "Choose size..."
    onChange: "updateSize()"
    padding: 8
    borderWidth: 1
}
```

### Checkbox (0x33) - Boolean Input

**Purpose**: True/false selection
**Inherits**: Universal properties
**Additional Properties**:

| Property | Type | Description | Example |
|----------|------|-------------|---------|
| `checked` | Bool | Checked state | `checked: $isEnabled` |
| `label` | String | Associated label | `label: "Enable notifications"` |

**Example**:
```kry
Checkbox {
    checked: $notificationsEnabled
    label: "Enable push notifications"
    onChange: "toggleNotifications()"
}
```

## Property Inheritance Examples

Understanding how properties flow through the element tree:

### Example 1: Text Color Inheritance
```kry
Container {
    textColor: "blue"                    # Set on container
    
    Text {
        text: "I am blue"                # ✅ Inherits blue color
    }
    
    Text {
        text: "I am red"                 # 🔄 Overrides inheritance
        textColor: "red"
    }
    
    Button {
        text: "I am also blue"           # ✅ Inherits blue color
        backgroundColor: "lightgray"
    }
}
```

### Example 2: Font Inheritance
```kry
App {
    fontFamily: "Arial"                  # Set at app level
    fontSize: 16
    
    Container {
        Text {
            text: "Arial, 16px"          # ✅ Inherits both
        }
        
        Text {
            text: "Arial, 20px"          # ✅ Inherits font, overrides size
            fontSize: 20
        }
        
        Text {
            text: "Georgia, 16px"        # 🔄 Overrides font, keeps size
            fontFamily: "Georgia"
        }
    }
}
```

### Example 3: Non-Inherited Properties
```kry
Container {
    backgroundColor: "lightblue"         # ❌ Does NOT inherit
    padding: 20                          # ❌ Does NOT inherit
    
    Container {
        # This container has NO background or padding
        # Must be explicitly set:
        backgroundColor: "white"
        padding: 10
        
        Text {
            text: "Explicit styling needed"
        }
    }
}
```

## Property Aliases & Shortcuts

Make development faster with short aliases:

### Universal Aliases
| Short | Full Property | Example |
|-------|---------------|---------|
| `bg` | `backgroundColor` | `bg: "#f0f0f0"` |
| `color` | `textColor` | `color: "black"` |
| `size` | `fontSize` | `size: 18` |
| `weight` | `fontWeight` | `weight: 700` |
| `align` | `textAlignment` | `align: "center"` |
| `w` | `width` | `w: 200` |
| `h` | `height` | `h: 100` |
| `x` | `left` | `x: 10` |
| `y` | `top` | `y: 20` |

### Element-Specific Aliases
| Element | Alias | Resolves To | Example |
|---------|-------|-------------|---------|
| App | `title` | `windowTitle` | `title: "My App"` |
| Image | `src` | `imageSource` | `src: "photo.jpg"` |
| Input | `hint` | `placeholder` | `hint: "Enter text..."` |
| Container | `direction` | `flexDirection` | `direction: "column"` |

## Complete Example

Here's a complex example showing inheritance and relationships:

```kry
App {
    title: "Task Manager"
    fontFamily: "Inter"                  # ✅ Inherited by all text
    fontSize: 14                         # ✅ Inherited by all text
    textColor: "#333"                    # ✅ Inherited by all text
    
    Container {
        bg: "#f8f9fa"                    # ❌ Not inherited
        padding: 20                      # ❌ Not inherited
        gap: 16                          # ❌ Not inherited
        
        Text {
            text: "My Tasks"             # Uses inherited font & color
            size: 24                     # 🔄 Overrides inherited fontSize
            weight: 700                  # New property
        }
        
        Container {
            direction: "row"             # Flexbox layout
            alignItems: "center"
            gap: 12
            bg: "white"                  # ❌ Not inherited, must set
            padding: 16
            borderRadius: 8
            
            Checkbox {
                checked: false
                # Uses inherited text color for label
            }
            
            Text {
                text: "Complete documentation"
                # Uses all inherited typography
            }
            
            Button {
                text: "Edit"
                bg: "#007bff"
                color: "white"           # 🔄 Overrides inherited textColor
                padding: 8
                borderRadius: 4
                onClick: "editTask()"
            }
        }
    }
}
```

This example shows:
- ✅ **Font properties** flow down from App to all text elements
- ❌ **Layout properties** (padding, background) must be set on each container
- 🔄 **Override behavior** where elements can customize inherited values
- **Aliases** (`bg`, `color`, `size`) make code more concise
- **Element relationships** showing how Button inherits from text elements

## Summary

- **Universal Properties**: Work on all elements, some inherit down the tree
- **Element-Specific Properties**: Only available on certain element types
- **Inheritance**: Typography and color properties inherit; layout properties don't
- **Aliases**: Short names for common properties increase productivity
- **Override**: Child elements can always override inherited values

This unified system makes Kryon UI both powerful and predictable!