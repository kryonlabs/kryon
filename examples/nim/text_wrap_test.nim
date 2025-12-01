## Test example for multi-line text wrapping
## Demonstrates Phase 1 of text layout implementation

import kryon_dsl

# Long text that should wrap
const longText = """
This is a long paragraph that demonstrates multi-line text wrapping in Kryon. The text layout engine will automatically break this text into multiple lines based on the maximum width constraint. This makes it possible to create text-heavy UIs like settings panels, chat messages, and documentation.

The greedy line-breaking algorithm ensures that words are not split in the middle, and trailing whitespace is trimmed from each line. You can also include explicit line breaks like this:

New paragraph!

The text layout system caches the computed layout for performance, and automatically invalidates the cache when the text content changes.
"""

# Define the UI with kryonApp structure
discard kryonApp:
  Header:
    windowWidth = 500
    windowHeight = 600
    windowTitle = "Multi-line Text Wrapping Demo"

  Body:
    backgroundColor = "#f0f0f0"
    padding = 20
    layoutDirection = 1  # Column

    # Title (single line)
    Text:
      content = "Multi-line Text Wrapping Demo"
      fontSize = 24
      color = "#333333"
      marginBottom = 20

    # Long text with wrapping enabled
    Text:
      content = longText
      fontSize = 16
      color = "#444444"
      # Set max_width to enable wrapping - this is the key!
      maxTextWidth = 420

    # Another wrapped text example
    Text:
      content = "This is a shorter paragraph that also wraps. Notice how the text breaks at word boundaries and maintains proper spacing between lines."
      fontSize = 14
      color = "#666666"
      marginTop = 20
      maxTextWidth = 420
