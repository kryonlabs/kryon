## Simple Markdown Test
##
## Basic test to verify the Markdown component works correctly

import kryon_dsl
import markdown

# Simple markdown content
let sampleMarkdown = """
# Simple Markdown Test

This is a **bold** text with *italic* and `code` formatting.

## Lists

### Unordered List
- First item
- Second item
- Third item

### Ordered List
1. First step
2. Second step
3. Third step

## Code Example

```nim
import kryon_dsl

let app = kryonApp:
  Body:
    Text:
      text = "Hello World"
```

## Blockquote

> This is a blockquote.
> It can span multiple lines.

## Links

Visit [Kryon](https://kryon.dev) for more information.
"""

let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Simple Markdown Test"

  Body:
    backgroundColor = "#ffffff"

    Container:
      width = 800
      height = 600
      padding = 20

      # Title
      Text:
        text = "Markdown Component Test"
        color = "#333333"
        fontSize = 24
        marginBottom = 20

      # Markdown content with default theme
      Markdown:
        source = sampleMarkdown
        width = 760
        height = 500
        onLinkClick = proc(url: string) =
          echo "Link clicked: ", url

echo "Simple Markdown Test Running"
echo "Testing basic markdown rendering with:"
echo "- Headings (H1, H2, H3)"
echo "- Text formatting (bold, italic, code)"
echo "- Lists (ordered and unordered)"
echo "- Code blocks"
echo "- Blockquotes"
echo "- Links"
echo ""