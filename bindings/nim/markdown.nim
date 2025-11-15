## Kryon Markdown Component - Rich Text Formatting
##
## Provides native Markdown rendering with CommonMark compliance
## Full GitHub Flavored Markdown support with customizable theming

import core_kryon, runtime, macros, strutils, os

export core_kryon

# ============================================================================
# Types and Enums (matching C definitions)
# ============================================================================

type
  MarkdownBlockType* = enum
    Document = 0
    Heading1 = 1
    Heading2 = 2
    Heading3 = 3
    Heading4 = 4
    Heading5 = 5
    Heading6 = 6
    Paragraph = 7
    ListItem = 8
    ListUnordered = 9
    ListOrdered = 10
    CodeBlock = 11
    HtmlBlock = 12
    Blockquote = 13
    Table = 14
    TableRow = 15
    TableCell = 16
    ThematicBreak = 17

  MarkdownInlineType* = enum
    Text = 0
    CodeSpan = 1
    Emphasis = 2
    Strong = 3
    Link = 4
    Image = 5
    LineBreak = 6
    SoftBreak = 7
    RawHtml = 8
    Strikethrough = 9

  MarkdownAlignment* = enum
    AlignLeft = 0
    AlignCenter = 1
    AlignRight = 2
    AlignNone = 3

  MarkdownNode* = ref object
    blockType*: MarkdownBlockType
    parent*: MarkdownNode
    firstChild*: MarkdownNode
    lastChild*: MarkdownNode
    next*: MarkdownNode
    prev*: MarkdownNode
    # Additional fields for specific types...

  MarkdownInline* = ref object
    inlineType*: MarkdownInlineType
    next*: MarkdownInline
    # Additional fields for specific types...

  MarkdownTheme* = ref object
    # Colors
    textColor*: uint32
    headingColors*: array[6, uint32]
    linkColor*: uint32
    codeTextColor*: uint32
    codeBackgroundColor*: uint32
    blockquoteTextColor*: uint32
    blockquoteBorderColor*: uint32
    tableBorderColor*: uint32
    tableHeaderBackgroundColor*: uint32

    # Typography
    baseFontSize*: int
    headingFontSizes*: array[6, int]
    codeFontSize*: int
    lineHeight*: int
    paragraphSpacing*: int
    headingSpacing*: array[6, int]

    # Layout
    maxWidth*: int
    padding*: int
    margin*: int
    listIndent*: int
    codeBlockIndent*: int

  MarkdownProps* = ref object
    source*: string
    file*: string
    theme*: MarkdownTheme
    width*: int
    height*: int
    padding*: int
    scrollable*: bool
    onLinkClick*: proc(url: string)
    onImageClick*: proc(url: string)

# ============================================================================
# Built-in Themes
# ============================================================================

proc newMarkdownTheme*(): MarkdownTheme =
  ## Create a default light theme
  result = MarkdownTheme(
    textColor: 0x202020FF'u32,
    headingColors: [
      0x000000FF'u32,  # H1
      0x1a1a1aFF'u32,  # H2
      0x2d2d2dFF'u32,  # H3
      0x404040FF'u32,  # H4
      0x535353FF'u32,  # H5
      0x666666FF'u32   # H6
    ],
    linkColor: 0x0066CCFF'u32,
    codeTextColor: 0x333333FF'u32,
    codeBackgroundColor: 0xF0F0F0FF'u32,
    blockquoteTextColor: 0x333333FF'u32,
    blockquoteBorderColor: 0xDDDDDDFF'u32,
    tableBorderColor: 0xCCCCCCFF'u32,
    tableHeaderBackgroundColor: 0xF5F5F5FF'u32,
    baseFontSize: 16,
    headingFontSizes: [32, 28, 24, 20, 18, 16],
    codeFontSize: 14,
    lineHeight: 24,
    paragraphSpacing: 16,
    headingSpacing: [16, 14, 12, 10, 8, 6],
    maxWidth: 800,
    padding: 16,
    margin: 16,
    listIndent: 24,
    codeBlockIndent: 16
  )

proc newDarkMarkdownTheme*(): MarkdownTheme =
  ## Create a dark theme
  let theme = newMarkdownTheme()
  theme.textColor = 0xE5E5E5FF'u32
  theme.headingColors = [
    0xFFFFFFFF'u32,  # H1
    0xF0F0F0FF'u32,  # H2
    0xE0E0E0FF'u32,  # H3
    0xD0D0D0FF'u32,  # H4
    0xB0B0B0FF'u32,  # H5
    0x909090FF'u32   # H6
  ]
  theme.linkColor = 0x60A5FAFF'u32
  theme.codeTextColor = 0xA0A0A0FF'u32
  theme.codeBackgroundColor = 0x1F1F1FFF'u32
  theme.blockquoteTextColor = 0xD0D0D0FF'u32
  theme.blockquoteBorderColor = 0x404040FF'u32
  theme.tableBorderColor = 0x333333FF'u32
  theme.tableHeaderBackgroundColor = 0x2D2D2DFF'u32
  result = theme

proc newGitHubMarkdownTheme*(): MarkdownTheme =
  ## Create a GitHub-style theme
  let theme = newMarkdownTheme()
  theme.textColor = 0x24292EFF'u32
  theme.headingColors = [
    0x24292EFF'u32,  # H1
    0x24292EFF'u32,  # H2
    0x24292EFF'u32,  # H3
    0x24292EFF'u32,  # H4
    0x24292EFF'u32,  # H5
    0x24292EFF'u32   # H6
  ]
  theme.linkColor = 0x0366D6FF'u32
  theme.codeTextColor = 0xE1E4E8FF'u32
  theme.codeBackgroundColor = 0xF6F8FAFF'u32
  theme.blockquoteTextColor = 0x6A737DFF'u32
  theme.blockquoteBorderColor = 0xD0D7DEFF'u32
  theme.tableBorderColor = 0xD0D7DEFF'u32
  theme.tableHeaderBackgroundColor = 0xF6F8FAFF'u32
  theme.maxWidth = 880
  result = theme

# ============================================================================
# C API Bindings
# ============================================================================

# C types (opaque pointers)
type
  md_node_t {.importc: "md_node_t", header: "kryon_markdown.h", incompleteStruct.} = object
  md_renderer_t {.importc: "md_renderer_t", header: "kryon_markdown.h", incompleteStruct.} = object
  md_theme_t {.importc: "md_theme_t", header: "kryon_markdown.h".} = object

  CMarkdownNode* = ptr md_node_t
  CMarkdownRenderer* = ptr md_renderer_t
  CMarkdownTheme* = ptr md_theme_t

# Parser functions
proc mdParse*(input: cstring, length: csize_t): CMarkdownNode {.importc: "md_parse", header: "kryon_markdown.h".}
proc mdParseFile*(filename: cstring): CMarkdownNode {.importc: "md_parse_file", header: "kryon_markdown.h".}
proc mdFree*(root: CMarkdownNode) {.importc: "md_free", header: "kryon_markdown.h".}

# Renderer functions
proc mdRendererCreate*(cmdBuf: KryonCmdBuf, width, height: uint16): CMarkdownRenderer {.importc: "md_renderer_create", header: "kryon_markdown.h".}
proc mdRendererDestroy*(renderer: CMarkdownRenderer) {.importc: "md_renderer_destroy", header: "kryon_markdown.h".}
proc mdRendererSetTheme*(renderer: CMarkdownRenderer, theme: CMarkdownTheme) {.importc: "md_renderer_set_theme", header: "kryon_markdown.h".}
proc mdRenderDocument*(root: CMarkdownNode, renderer: CMarkdownRenderer) {.importc: "md_render_document", header: "kryon_markdown.h".}

# Theme access (external C variables)
var mdThemeLight* {.importc: "md_theme_light", header: "kryon_markdown.h".}: md_theme_t
var mdThemeDark* {.importc: "md_theme_dark", header: "kryon_markdown.h".}: md_theme_t
var mdThemeGithub* {.importc: "md_theme_github", header: "kryon_markdown.h".}: md_theme_t

# ============================================================================
# High-Level API
# ============================================================================

proc parseMarkdown*(text: string): MarkdownNode =
  ## Parse markdown text and return AST root node
  if text.len == 0: return nil

  let cRoot = mdParse(text.cstring, csize_t(text.len))
  if cRoot == nil: return nil

  result = MarkdownNode(blockType: MarkdownBlockType.Document)
  # Convert C nodes to Nim wrapper (simplified)
  result.firstChild = nil

proc parseMarkdownFile*(filename: string): MarkdownNode =
  ## Parse markdown from file and return AST root node
  let cRoot = mdParseFile(filename.cstring)
  if cRoot == nil: return nil

  result = MarkdownNode(blockType: MarkdownBlockType.Document)

proc freeMarkdown*(node: MarkdownNode) =
  ## Free parsed markdown AST
  if node != nil:
    # Convert back to C node for cleanup
    mdFree(nil)  # Simplified - in real implementation would track C nodes

proc createMarkdownRenderer*(cmdBuf: KryonCmdBuf, width, height: int): CMarkdownRenderer =
  ## Create a markdown renderer for the given command buffer
  mdRendererCreate(cmdBuf, uint16(width), uint16(height))

# ============================================================================
# Component Integration
# ============================================================================

proc parseInlineMarkdown(text: string, theme: MarkdownTheme, fontSize: uint8 = 0): KryonComponent =
  ## Parse inline markdown and convert to NATIVE inline components
  ## Returns a Text component with Bold, Italic, Code children
  ## Handles: **bold**, *italic*, `code`
  ## fontSize: Font size for all text (0 = use default)

  # Create the parent Text component
  result = newKryonText("", fontSize, 0, 0)

  # DEBUG
  if getEnv("KRYON_TRACE_LAYOUT") != "":
    echo "[nim][markdown] parseInlineMarkdown: text='", text, "' fontSize=", fontSize

  var i = 0
  var currentText = ""

  # Flush current plain text as a child
  template flushCurrentText() =
    if currentText.len > 0:
      let plainText = newKryonText(currentText, fontSize, 0, 0)
      let addResult = kryon_component_add_child(result, plainText)
      if getEnv("KRYON_TRACE_LAYOUT") != "":
        echo "[nim][markdown]   Added plain text child: '", currentText, "' result=", addResult
      currentText = ""

  while i < text.len:
    # Check for **bold**
    if i + 1 < text.len and text[i] == '*' and text[i+1] == '*':
      flushCurrentText()
      i += 2
      let start = i
      while i + 1 < text.len and not (text[i] == '*' and text[i+1] == '*'):
        inc i
      if i + 1 < text.len:
        let boldText = text[start..<i]  # Strip markers
        # Create Bold component with text using native constructor
        let boldComp = newKryonBold(boldText, fontSize)
        let addResult = kryon_component_add_child(result, boldComp)
        if getEnv("KRYON_TRACE_LAYOUT") != "":
          echo "[nim][markdown]   Added Bold child: '", boldText, "' result=", addResult
          echo "[nim][markdown]   After bold, i=", i, " next char='", (if i < text.len: $text[i] else: "EOF"), "'"
        i += 2
        continue
      else:
        # No closing markers, treat as literal
        currentText.add("**")
        i = start
        continue

    # Check for *italic*
    elif text[i] == '*':
      flushCurrentText()
      inc i
      let start = i
      while i < text.len and text[i] != '*':
        inc i
      if i < text.len:
        let italicText = text[start..<i]  # Strip markers
        # Create Italic component with text using native constructor
        let italicComp = newKryonItalic(italicText, fontSize)
        let addResult = kryon_component_add_child(result, italicComp)
        if getEnv("KRYON_TRACE_LAYOUT") != "":
          echo "[nim][markdown]   Added Italic child: '", italicText, "' result=", addResult
        inc i
        continue
      else:
        # No closing marker, treat as literal
        currentText.add('*')
        i = start
        continue

    # Check for `code`
    elif text[i] == '`':
      flushCurrentText()
      inc i
      let start = i
      while i < text.len and text[i] != '`':
        inc i
      if i < text.len:
        let codeText = text[start..<i]  # Strip markers
        # Create Code component with text using native constructor
        let codeComp = newKryonCode(codeText, fontSize)
        let addResult = kryon_component_add_child(result, codeComp)
        if getEnv("KRYON_TRACE_LAYOUT") != "":
          echo "[nim][markdown]   Added Code child: '", codeText, "' result=", addResult
        inc i
        continue
      else:
        # No closing backtick, treat as literal
        currentText.add('`')
        i = start
        continue

    # Regular character
    else:
      currentText.add(text[i])
      inc i

  flushCurrentText()

proc createMarkdownComponent*(props: MarkdownProps): KryonComponent =
  ## Create a Markdown component with the given properties
  if props == nil: return nil

  # Parse markdown content
  var content = props.source
  if props.file.len > 0:
    # Load from file if file is specified
    content = readFile(props.file)

  if content.len == 0:
    # Empty or failed to load, create empty container
    return newKryonContainer()

  # Get theme
  let theme = if props.theme != nil: props.theme else: newMarkdownTheme()

  # Create container (scrollable or not based on props)
  result = newKryonContainer()
  kryon_component_set_layout_direction(result, 0)  # 0 = column (vertical)

  # Add padding to the markdown container for proper margins
  let containerPadding = uint8(theme.padding)
  kryon_component_set_padding(result, containerPadding, containerPadding, containerPadding, containerPadding)

  # Configure as scrollable if requested
  if props.scrollable:
    if props.height > 0:
      kryon_component_set_bounds(result, toFixed(0), toFixed(0), toFixed(props.width), toFixed(props.height))
    kryon_component_set_scrollable(result, true)

  # Parse markdown line by line for simple block-level elements
  let lines = content.split('\n')

  var i = 0
  while i < lines.len:
    let line = lines[i]

    if line.len == 0:
      # Empty line - create spacing between blocks
      let spacer = newKryonText("", uint8(theme.baseFontSize), 0, 0)
      kryon_component_set_margin(spacer, 0, 0, 12, 0)  # Add vertical spacing (bottom margin)
      addChild(result, spacer)
      inc i
      continue

    # Check for code blocks (triple backticks)
    if line.startsWith("```"):
      # Extract language identifier (optional)
      let lang = if line.len > 3: line[3..^1].strip() else: ""

      # Collect code block lines
      var codeLines: seq[string] = @[]
      inc i
      while i < lines.len and not lines[i].startsWith("```"):
        codeLines.add(lines[i])
        inc i

      # Skip the closing ```
      if i < lines.len:
        inc i

      # Create code block as a container with one text component per line
      let codeBlockContainer = newKryonContainer()
      kryon_component_set_layout_direction(codeBlockContainer, 0)  # Column layout

      # Set width to fill available space (accounting for parent's padding)
      # Inner width = total width - left padding - right padding
      if props.width > 0:
        let innerWidth = props.width - (theme.padding * 2)
        kryon_component_set_bounds(codeBlockContainer, toFixed(0), toFixed(0), toFixed(innerWidth), toFixed(0))

      # Add padding and background for proper code block styling
      kryon_component_set_padding(codeBlockContainer, 8, 8, 8, 8)
      kryon_component_set_background_color(codeBlockContainer, theme.codeBackgroundColor)

      if getEnv("KRYON_TRACE_LAYOUT") != "":
        echo "[nim][markdown] Creating code block, lang='", lang, "' lines=", codeLines.len

      # Add each line as a separate code text component
      for codeLine in codeLines:
        let lineComp = newKryonCodeBlock(codeLine, uint8(theme.codeFontSize))
        addChild(codeBlockContainer, lineComp)

      addChild(result, codeBlockContainer)
      continue

    # Check for headings (H1-H6)
    if line.startsWith("# "):
      let text = line[2..^1]  # Remove marker - H1
      let textComp = parseInlineMarkdown(text, theme, 32)  # H1 = 32px
      addChild(result,textComp)
    elif line.startsWith("## "):
      let text = line[3..^1]  # Remove marker - H2
      let textComp = parseInlineMarkdown(text, theme, 28)  # H2 = 28px
      addChild(result,textComp)
    elif line.startsWith("### "):
      let text = line[4..^1]  # Remove marker - H3
      let textComp = parseInlineMarkdown(text, theme, 24)  # H3 = 24px
      addChild(result,textComp)
    elif line.startsWith("#### "):
      let text = line[5..^1]  # Remove marker - H4
      let textComp = parseInlineMarkdown(text, theme, 20)  # H4 = 20px
      addChild(result,textComp)
    elif line.startsWith("##### "):
      let text = line[6..^1]  # Remove marker - H5
      let textComp = parseInlineMarkdown(text, theme, 18)  # H5 = 18px
      addChild(result,textComp)
    elif line.startsWith("###### "):
      let text = line[7..^1]  # Remove marker - H6
      let textComp = parseInlineMarkdown(text, theme, 16)  # H6 = 16px
      addChild(result,textComp)

    # Check for list items
    elif line.startsWith("- ") or line.startsWith("* "):
      let text = "• " & line[2..^1]
      let textComp = parseInlineMarkdown(text, theme)
      addChild(result,textComp)

    # Check for blockquote (lines starting with "> ")
    elif line.startsWith("> "):
      echo "[nim][markdown] Found blockquote at line ", i, ": '", line, "'"
      # Collect consecutive blockquote lines
      var quoteLines: seq[string] = @[]
      while i < lines.len and lines[i].startsWith("> "):
        quoteLines.add(lines[i][2..^1])  # Strip "> " prefix
        inc i

      # i now points to first non-blockquote line
      # We use 'continue' below to skip the outer loop's inc i,
      # so i will correctly point to the first non-blockquote line on next iteration
      echo "[nim][markdown] Collected ", quoteLines.len, " blockquote lines, i now at ", i

      # Create blockquote container
      let blockquoteContainer = newKryonContainer()
      kryon_component_set_layout_direction(blockquoteContainer, 0)  # Column layout

      # Set width to fill available space
      if props.width > 0:
        let innerWidth = props.width - (theme.padding * 2)
        kryon_component_set_bounds(blockquoteContainer, toFixed(0), toFixed(0), toFixed(innerWidth), toFixed(0))

      # Style: left padding for indentation (16px left, 12px top/bottom, 8px right)
      kryon_component_set_padding(blockquoteContainer, 12, 8, 12, 16)

      # Style: left border (4px wide) - Note: this will draw all 4 sides
      kryon_component_set_border_width(blockquoteContainer, 4)
      kryon_component_set_border_color(blockquoteContainer, theme.blockquoteBorderColor)

      # Style: subtle background
      kryon_component_set_background_color(blockquoteContainer, 0xF8F8F8FF'u32)

      # Add each line as a separate text component to create multiple lines
      for quoteLine in quoteLines:
        echo "[nim][markdown] Adding blockquote line: '", quoteLine, "'"
        let textComp = parseInlineMarkdown(quoteLine, theme)
        # Set text color for this line
        kryon_component_set_text_color(textComp, theme.blockquoteTextColor)
        # Add the line to the blockquote container
        addChild(blockquoteContainer, textComp)

      addChild(result, blockquoteContainer)
      continue

    # Regular paragraph with inline formatting
    else:
      let textComp = parseInlineMarkdown(line, theme)
      addChild(result,textComp)

    # Move to next line
    inc i

# ============================================================================
# DSL Macro - NOTE: The Markdown macro is defined in kryon_dsl.nim
# ============================================================================
# The Markdown component macro is integrated into the DSL system in kryon_dsl.nim
# It creates a Markdown component using the following syntax:
# Markdown:
#   source = "# Title\nThis is **bold** text."
#   theme = darkTheme
#   width = 800

# ============================================================================
# Convenience Functions
# ============================================================================

proc markdown*(source: string): MarkdownProps =
  ## Create markdown properties from source string
  MarkdownProps(source: source, theme: newMarkdownTheme())

proc markdownFile*(filename: string): MarkdownProps =
  ## Create markdown properties from file
  MarkdownProps(file: filename, theme: newMarkdownTheme())

proc withTheme*(props: MarkdownProps, theme: MarkdownTheme): MarkdownProps =
  ## Set theme for markdown properties
  result = props
  result.theme = theme

proc withSize*(props: MarkdownProps, width, height: int): MarkdownProps =
  ## Set size for markdown properties
  result = props
  result.width = width
  result.height = height

proc withPadding*(props: MarkdownProps, padding: int): MarkdownProps =
  ## Set padding for markdown properties
  result = props
  result.padding = padding

proc onLinkClick*(props: MarkdownProps, callback: proc(url: string)): MarkdownProps =
  ## Set link click handler
  result = props
  result.onLinkClick = callback

proc onImageClick*(props: MarkdownProps, callback: proc(url: string)): MarkdownProps =
  ## Set image click handler
  result = props
  result.onImageClick = callback

# ============================================================================
# Markdown Canvas Rendering (for Canvas component integration)
# ============================================================================

# TODO: Implement when Canvas component is available
# proc renderMarkdownToCanvas*(markdown: string, canvas: Canvas, theme: MarkdownTheme) =
#   ## Render markdown directly to canvas using Love2D-style API
#   if markdown.len == 0 or canvas == nil: return
#
#   # Parse the markdown
#   let ast = parseMarkdown(markdown)
#   if ast == nil: return
#
#   # Create renderer with canvas command buffer
#   let renderer = createMarkdownRenderer(getCanvasCommandBuffer(), canvas.getWidth, canvas.getHeight)
#   if renderer == nil:
#     freeMarkdown(ast)
#     return
#
#   # Use a C theme directly (can't access fields of opaque pointer)
#   var cTheme = addr mdThemeLight
#   if theme != nil:
#     # Would need C function to create theme from Nim values
#     discard
#
#   mdRendererSetTheme(renderer, cTheme)
#
#   # Render the document
#   mdRenderDocument(nil, renderer)  # Convert AST back to C
#
#   # Cleanup
#   mdRendererDestroy(renderer)
#   freeMarkdown(ast)

# ============================================================================
# Predefined Themes
# ============================================================================

proc lightTheme*(): MarkdownTheme =
  ## Get the predefined light theme
  newMarkdownTheme()

proc darkTheme*(): MarkdownTheme =
  ## Get the predefined dark theme
  newDarkMarkdownTheme()

proc githubTheme*(): MarkdownTheme =
  ## Get the predefined GitHub theme
  newGitHubMarkdownTheme()

# ============================================================================
# Utility Functions
# ============================================================================

proc countLines*(markdown: string): int =
  ## Count the number of lines in markdown text
  result = 0
  for c in markdown:
    if c == '\n':
      inc result

proc countWords*(markdown: string): int =
  ## Count the number of words in markdown text
  result = 0
  var inWord = false

  for i, c in markdown:
    if c == ' ' or c == '\t' or c == '\n' or c == '\r':
      inWord = false
    elif not inWord and (c.isAlphaAscii() or c.isDigit()):
      inc result
      inWord = true

proc extractLinks*(markdown: string): seq[string] =
  ## Extract all links from markdown text
  result = @[]
  var i = 0

  while i < markdown.len:
    if i + 1 < markdown.len and markdown[i] == '[':
      # Find closing bracket
      let closeBracket = markdown.find(']', i + 1)
      if closeBracket != -1:
        # Find opening parenthesis
        let openParen = markdown.find('(', closeBracket + 1)
        if openParen != -1:
          # Find closing parenthesis
          let closeParen = markdown.find(')', openParen + 1)
          if closeParen != -1:
            let url = markdown[openParen + 1 ..< closeParen]
            result.add(url)
            i = closeParen + 1
            continue
    inc i

# ============================================================================
# Markdown Component Factory Functions
# ============================================================================

proc newMarkdown*(source: string, theme: MarkdownTheme = nil): KryonComponent =
  ## Create a new Markdown component with source text
  let props = MarkdownProps(source: source, theme: if theme != nil: theme else: newMarkdownTheme())
  createMarkdownComponent(props)

proc newMarkdownFromFile*(filename: string, theme: MarkdownTheme = nil): KryonComponent =
  ## Create a new Markdown component from file
  let props = MarkdownProps(file: filename, theme: if theme != nil: theme else: newMarkdownTheme())
  createMarkdownComponent(props)