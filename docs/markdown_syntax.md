# Kryon Markdown Syntax Reference

**Version**: 1.0
**Status**: Draft
**Last Updated**: 2026-02-01

## Overview

Kryon Markdown extends CommonMark with custom block syntax for UI components. All Kryon-specific syntax uses the `:::` delimiter to avoid conflicts with standard markdown.

## Syntax Specification

### Component Block Syntax

```
:::component_type attribute1="value1" attribute2="value2"
  Body content (children or text)
:::
```

**Rules:**
- Opening: `:::` followed immediately by component type (no space)
- Attributes: space-separated `name="value"` pairs on opening line
- Body: indented or nested content on following lines
- Closing: `:::` on its own line

### Component Types

#### 1. Button
```markdown
:::button text="Click Me" onclick="count = count + 1"
:::
```

**Attributes:**
- `text` (string) - Button label
- `onclick` (string) - Click event handler
- `id` (number) - Component ID (auto-generated)

#### 2. Input (Text Field)
```markdown
:::input placeholder="Enter name" value="" onchange="validate($value)"
:::
```

**Attributes:**
- `placeholder` (string) - Placeholder text
- `value` (string) - Current value
- `type` (string) - Input type: "text", "password", "email" (default: "text")
- `onchange` (string) - Change event handler
- `oninput` (string) - Input event handler

#### 3. Checkbox
```markdown
:::checkbox checked=false text="Enable feature" onchange="toggleFeature($checked)"
:::
```

**Attributes:**
- `checked` (boolean) - Checked state
- `text` (string) - Label text
- `onchange` (string) - Change event handler (receives `$checked`)

#### 4. Text
```markdown
:::text color="#ff0000" bold=true::Hello World::
```

**Inline syntax** (for text within other components):
```
::text attrs::content::
```

**Attributes:**
- `color` (color) - Text color (hex or rgba)
- `backgroundColor` (color) - Background color
- `bold` (boolean) - Bold text
- `italic` (boolean) - Italic text
- `fontSize` (number) - Font size in pixels
- `fontFamily` (string) - Font name

#### 5. Image
```markdown
:::image src="logo.png" width="200" height="100" onclick="handleImageClick()"
:::
```

**Attributes:**
- `src` (string) - Image path/URL
- `width` (number) - Width in pixels
- `height` (number) - Height in pixels
- `alt` (string) - Alt text
- `onclick` (string) - Click event handler

#### 6. Row (Horizontal Layout)
```markdown
:::row gap="10" padding="5" alignment="center"
  ::text Left::
  ::text Center::
  ::text Right::
:::
```

**Attributes:**
- `gap` (number) - Spacing between children (pixels)
- `padding` (number or "top right bottom left") - Padding
- `margin` (number or "top right bottom left") - Margin
- `alignment` (string) - Vertical alignment: "start", "center", "end"
- `width` (string) - Width: "auto", number, or "flex"

#### 7. Column (Vertical Layout)
```markdown
:::column gap="15" padding="20" alignment="start"
  ::text Title::
  ::input::
  ::button Submit::
:::
```

**Attributes:**
- `gap` (number) - Spacing between children (pixels)
- `padding` (number or "top right bottom left") - Padding
- `margin` (number or "top right bottom left") - Margin
- `alignment` (string) - Horizontal alignment: "start", "center", "end"
- `flex` (number) - Flex grow factor (0-1)

#### 8. Canvas
```markdown
:::canvas width="400" height="300" ondraw="renderCanvas($ctx)"
:::
```

**Attributes:**
- `width` (number) - Canvas width
- `height` (number) - Canvas height
- `ondraw` (string) - Draw event handler (receives `$ctx`)

### Standard Markdown (Mixed Content)

You can mix standard markdown with Kryon components:

```markdown
# Welcome to My App

This is a paragraph with **bold** and *italic* text.

:::column gap="10"
  :::button Click Me::
  :::input placeholder="Type here"::
:::

## Features

- Feature 1
- Feature 2

More text here...
```

## Attribute Value Types

### Strings
```markdown
text="Hello World"
placeholder="Enter text"
fontFamily="Arial"
```

### Numbers
```markdown
fontSize=16
width=200
flex=1
```

### Booleans
```markdown
checked=true
bold=false
visible=true
```

### Colors
```markdown
color="#ff0000"
backgroundColor="rgba(255, 0, 0, 0.5)"
borderColor="#00ff00"
```

### Arrays (Space-separated)
```markdown
padding="10 20 10 20"  <!-- top right bottom left -->
```

## Event Handlers

### Handler Syntax
Events are attributes starting with `on`:

```markdown
:::button onclick="myFunction()":::
```

### Available Events
- `onclick` - Mouse click / tap
- `onchange` - Value changed (input, checkbox)
- `oninput` - Value input (text field)
- `onsubmit` - Form submitted
- `ondraw` - Canvas render (canvas only)

### Special Variables
Event handlers can access special variables:

| Variable | Type | Description |
|----------|------|-------------|
| `$event` | Event | The event object |
| `$value` | any | The current value (input) |
| `$checked` | boolean | Checkbox state (checkbox) |
| `$ctx` | Context | Canvas context (canvas) |

### Example Handlers
```markdown
:::button onclick="count = count + 1"::Increment:::

:::input onchange="name = $value; validate()":::

:::checkbox onchange="if ($checked) enableFeature()":::
```

## Styling Properties

### Text Styling
```markdown
:::text
  color="#333333"
  fontSize=16
  fontFamily="Arial"
  bold=true
  italic=false
::Styled Text::
```

### Background Styling
```markdown
:::column
  backgroundColor="#f0f0f0"
  padding=20
::Content::
```

### Border Styling
```markdown
:::button
  borderWidth=2
  borderColor="#000000"
  borderRadius=5
::Button::
```

### Layout Styling
```markdown
:::row
  gap=10
  padding="10 20"
  margin=5
  alignment="center"
::Content::
```

## Complex Examples

### Login Form
```markdown
:::column padding="30" gap="15"
  :::text fontSize=24 bold=true::Login:::

  :::input placeholder="Username" type="text":::
  :::input placeholder="Password" type="password":::

  :::checkbox text="Remember me" checked=false:::

  :::row gap="10"
    :::button text="Cancel":::
    :::button text="Login" onclick="doLogin()":::
  :::
:::
```

### Navigation Bar
```markdown
:::row
  padding="10 20"
  backgroundColor="#007bff"
  gap="20"
  alignment="center"
  :::text color="#ffffff" bold=true::MyApp:::
  :::text color="#ffffff"::Home:::
  :::text color="#ffffff"::About:::
  :::text color="#ffffff"::Contact:::
:::
```

### Card Layout
```markdown
:::column
  padding=20
  backgroundColor="#ffffff"
  border=1
  borderColor="#e0e0e0"
  borderRadius=8
  :::text fontSize=18 bold=true::Card Title:::
  :::text color="#666666"::Card description goes here...:::
  :::row gap="10"
    :::button text="Learn More":::
    :::button text="Action" onclick="handleAction()":::
  :::
:::
```

### Data Table
```markdown
:::column gap="5"
  :::row gap="10" padding="10" backgroundColor="#f0f0f0"
    :::text bold=true::Name:::
    :::text bold=true::Email:::
    :::text bold=true::Actions:::
  :::

  :::row gap="10" padding="10"
    :::text::John Doe:::
    :::text::john@example.com:::
    :::button text="Edit" onclick="editUser(1)":::
  :::

  :::row gap="10" padding="10"
    :::text::Jane Smith:::
    :::text::jane@example.com:::
    :::button text="Edit" onclick="editUser(2)":::
  :::
:::
```

## Migration from HTML Syntax

### Old HTML-based Output
```markdown
<button onclick="count = count + 1">Increment</button>
<input type="text" placeholder="Enter text" onchange="handleChange(event)" />
```

### New Custom Syntax
```markdown
:::button text="Increment" onclick="count = count + 1"
:::

:::input placeholder="Enter text" onchange="handleChange($value)"
:::
```

**Advantages:**
- ✅ Pure markdown (no mixed HTML)
- ✅ All attributes preserved in round-trip
- ✅ More concise and readable
- ✅ Better nesting support
- ✅ Event handlers preserved

## Best Practices

1. **Indentation**: Use consistent indentation (2 or 4 spaces) for nested components
2. **Attribute Order**: Put `text`/`value` first, then event handlers, then styling
3. **Quotes**: Always use double quotes for attribute values
4. **Spacing**: Put space between attributes for readability
5. **Closing**: Always close blocks with `:::` on its own line

### Good Example
```markdown
:::column gap="10" padding="20"
  :::text text="Welcome" fontSize=18 bold=true:::
  :::input placeholder="Name":::
  :::button text="Submit" onclick="submit()":::
:::
```

### Bad Example
```markdown
:::column gap=10 padding=20
:::text fontSize=18 bold=true::Welcome:::
:::input placeholder="Name":::
:::button text="Submit" onclick="submit()":::
:::
```

## Compatibility

### Works With
- ✅ CommonMark parsers (custom blocks ignored gracefully)
- ✅ GitHub Flavored Markdown
- ✅ Most markdown preview tools
- ✅ Standard markdown renderers

### Requires
- Kryon markdown parser for full functionality
- Kryon runtime for event handlers

## Limitations

1. **Deep Nesting**: Limit nesting to 10 levels deep
2. **Attribute Length**: Attribute values max 1024 characters
3. **Event Handlers**: Max 4096 characters per handler
4. **Special Characters**: Must escape quotes in attribute values: `text="Say \"Hello\""`

## Grammar (EBNF)

```
component_block ::= ":::" component_type attributes LF body ":::" LF
component_type ::= "button" | "input" | "checkbox" | "text" | "image" | "row" | "column" | "canvas"
attributes ::= (attribute)*
attribute ::= name "=" (string | number | boolean | color)
name ::= alphanumeric+
string ::= quoted_string
number ::= digits ["." digits]
boolean ::= "true" | "false"
color ::= "#" hex_hex_hex | "rgba" "(" digits "," digits "," digits "," digits ")"
body ::= (component_block | standard_markdown)*
```

## Examples Repository

See `tests/round_trip/markdown/` for complete examples:
- `button.md` - Button components
- `form.md` - Form layouts
- `nested.md` - Nested components
- `styling.md` - Styling examples
- `events.md` - Event handlers
- `mixed.md` - Mixed markdown and components
