## Simple Markdown Test
##
## Basic test to verify the Markdown component works correctly

import kryon_dsl

# Markdown content with lots of text to test scrolling
let sampleMarkdown = """
# Scrollable Markdown Test

This is a **bold** text with *italic* and `code` formatting.

## Section 1: Lists

### Unordered List
- First item
- Second item
- Third item

### Ordered List
1. First step
2. Second step
3. Third step

## Section 2: Code Example

```nim
import kryon_dsl

let app = kryonApp:
  Body:
    Text:
      text = "Hello World"
```

## Section 3: Blockquote

> This is a blockquote.
> It can span multiple lines.

## Section 4: More Content

Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.

### Subsection A

More text here to make the content overflow the container height.

### Subsection B

Even more text to ensure scrolling is necessary.

## Section 5: Additional Lists

1. Item one
2. Item two
3. Item three
4. Item four
5. Item five

## Section 6: Links

Visit [Kryon](https://kryon.dev) for more information.

## Section 7: Final Content

This is the end of the markdown content. You should be able to scroll to see all of this text.
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

echo "Simple Markdown Test Running"
echo "Testing basic markdown rendering with:"
echo "- Headings (H1, H2, H3)"
echo "- Text formatting (bold, italic, code)"
echo "- Lists (ordered and unordered)"
echo "- Code blocks"
echo "- Blockquotes"
echo "- Links"
echo ""