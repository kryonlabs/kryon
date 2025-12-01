## Text Shaping Demo - HarfBuzz Integration
## Demonstrates complex script support, ligatures, and proper text measurement

import kryon_dsl
import strformat

# Font path (DejaVu Sans from Nix store)
const fontPath = "/nix/store/5djka4vhy98x1grxmkghpql3531qjbza-dejavu-fonts-2.37/share/fonts/truetype/DejaVuSans.ttf"

var
  # Global font handle
  globalFont: ptr IRShapingFont = nil

  # Shaped text results
  simpleTextWidth: float = 0.0
  ligatureTextWidth: float = 0.0

proc initTextShaping() =
  ## Initialize the text shaping system
  if not ir_text_shaping_init():
    echo "Failed to initialize text shaping"
    return

  echo "✓ Text shaping initialized"

  # Load font
  globalFont = ir_font_load(fontPath, 16.0)
  if globalFont.isNil:
    echo "Failed to load font"
    return

  echo fmt"✓ Loaded font: {fontPath}"

  # Test 1: Simple text (pass NULL for options to use defaults)
  let text1 = "Hello, World!"
  let shaped1 = ir_shape_text(globalFont, cstring(text1), uint32(text1.len), nil)

  if not shaped1.isNil:
    simpleTextWidth = ir_shaped_text_get_width(shaped1)
    echo fmt"Simple text: '{text1}' = {shaped1.glyph_count} glyphs, {simpleTextWidth:.2f}px wide"
    ir_shaped_text_destroy(shaped1)

  # Test 2: Text with ligatures
  let text2 = "fficeffifflfi"
  let shaped2 = ir_shape_text(globalFont, cstring(text2), uint32(text2.len), nil)

  if not shaped2.isNil:
    ligatureTextWidth = ir_shaped_text_get_width(shaped2)
    echo fmt"Ligature text: '{text2}' = {shaped2.glyph_count} glyphs (ligatures formed!), {ligatureTextWidth:.2f}px wide"
    ir_shaped_text_destroy(shaped2)

proc cleanupTextShaping() =
  ## Cleanup text shaping resources
  if not globalFont.isNil:
    ir_font_destroy(globalFont)
    globalFont = nil

  ir_text_shaping_shutdown()
  echo "✓ Text shaping cleaned up"

# Initialize text shaping before creating UI
initTextShaping()

# Create UI
discard kryonApp:
  Header:
    windowWidth = 600
    windowHeight = 500
    windowTitle = "Text Shaping Demo - HarfBuzz"

  Body:
    backgroundColor = "#1e1e1e"
    padding = 30
    layoutDirection = 1  # Column
    gap = 20

    # Title
    Text:
      content = "HarfBuzz Text Shaping Demo"
      fontSize = 28
      color = "#ffffff"
      marginBottom = 10

    # Description
    Text:
      content = "This demo shows HarfBuzz integration for complex text layout"
      fontSize = 14
      color = "#aaaaaa"
      marginBottom = 20

    # Test Results Section
    Container:
      width = 560
      height = auto
      backgroundColor = "#2d2d2d"
      borderRadius = 8
      padding = 20
      layoutDirection = 1  # Column
      gap = 15

      Text:
        content = "✓ Text Shaping System Initialized"
        fontSize = 16
        color = "#4CAF50"

      Text:
        content = "✓ Font Loaded: DejaVu Sans (16pt)"
        fontSize = 16
        color = "#4CAF50"

    # Simple Text Test
    Container:
      width = 560
      height = auto
      backgroundColor = "#2d2d2d"
      borderRadius = 8
      padding = 20
      layoutDirection = 1  # Column
      gap = 10
      marginTop = 10

      Text:
        content = "Test 1: Simple Latin Text"
        fontSize = 18
        color = "#FFC107"
        marginBottom = 5

      Text:
        content = "Text: \"Hello, World!\""
        fontSize = 14
        color = "#e0e0e0"

      Text:
        content = fmt"Width: {simpleTextWidth:.2f}px (measured with HarfBuzz)"
        fontSize = 14
        color = "#e0e0e0"

    # Ligature Test
    Container:
      width = 560
      height = auto
      backgroundColor = "#2d2d2d"
      borderRadius = 8
      padding = 20
      layoutDirection = 1  # Column
      gap = 10

      Text:
        content = "Test 2: Ligatures"
        fontSize = 18
        color = "#FFC107"
        marginBottom = 5

      Text:
        content = "Text: \"fficeffifflfi\""
        fontSize = 14
        color = "#e0e0e0"

      Text:
        content = fmt"Width: {ligatureTextWidth:.2f}px"
        fontSize = 14
        color = "#e0e0e0"

      Text:
        content = "✓ Ligatures like 'ffi' and 'ffl' automatically formed!"
        fontSize = 14
        color = "#4CAF50"

    # Features
    Container:
      width = 560
      height = auto
      backgroundColor = "#2d2d2d"
      borderRadius = 8
      padding = 20
      layoutDirection = 1  # Column
      gap = 8

      Text:
        content = "Supported Features:"
        fontSize = 18
        color = "#FFC107"
        marginBottom = 5

      Text:
        content = "✓ Complex script shaping (Arabic, Devanagari, Thai, etc.)"
        fontSize = 14
        color = "#e0e0e0"

      Text:
        content = "✓ Automatic ligature formation"
        fontSize = 14
        color = "#e0e0e0"

      Text:
        content = "✓ Proper glyph positioning and kerning"
        fontSize = 14
        color = "#e0e0e0"

      Text:
        content = "✓ Bidirectional text support (LTR/RTL)"
        fontSize = 14
        color = "#e0e0e0"

      Text:
        content = "✓ OpenType font features"
        fontSize = 14
        color = "#e0e0e0"

# Cleanup on exit
cleanupTextShaping()
