## Kryon Markdown Component - Rich Text Formatting
##
## Provides native Markdown rendering with CommonMark compliance
## Full GitHub Flavored Markdown support with customizable theming

import core_kryon, core_kryon_markdown, runtime

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

type
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

# Theme access
var mdThemeLight*: CMarkdownTheme {.importc: "md_theme_light", header: "core/markdown.c".}
var mdThemeDark*: CMarkdownTheme {.importc: "md_theme_dark", header: "core/markdown.c".}
var mdThemeGithub*: CMarkdownTheme {.importc: "md_theme_github", header: "core/markdown.c".}

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

  # Parse the markdown
  let ast = parseMarkdown(content)
  if ast == nil:
    return newKryonContainer()

  # Create container component that will render markdown
  result = newKryonContainer()

  # Store parsing data in component state for rendering
  # In a full implementation, we'd store this properly
  discard

# ============================================================================
# DSL Macro for Markdown Component
# ============================================================================

macro Markdown*(props: untyped): untyped =
  ## Creates a Markdown component using DSL syntax
  ##
  ## Usage:
  ## Markdown:
  ##   source = "# Title\nThis is **bold** text."
  ##   theme = darkTheme
  ##   width = 800

  result = quote do:
    createMarkdownComponent(`props`)

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

proc renderMarkdownToCanvas*(markdown: string, canvas: Canvas, theme: MarkdownTheme) =
  ## Render markdown directly to canvas using Love2D-style API
  if markdown.len == 0 or canvas == nil: return

  # Parse the markdown
  let ast = parseMarkdown(markdown)
  if ast == nil: return

  # Create renderer with canvas command buffer
  let renderer = createMarkdownRenderer(getCanvasCommandBuffer(), canvas.getWidth, canvas.getHeight)
  if renderer == nil:
    freeMarkdown(ast)
    return

  # Convert theme to C theme (simplified)
  var cTheme = mdThemeLight

  if theme != nil:
    # Convert Nim theme to C theme structure
    cTheme.textColor = theme.textColor
    cTheme.linkColor = theme.linkColor
    # ... copy other fields

  mdRendererSetTheme(renderer, cTheme)

  # Render the document
  mdRenderDocument(nil, renderer)  # Convert AST back to C

  # Cleanup
  mdRendererDestroy(renderer)
  freeMarkdown(ast)

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
    elif not inWord and (c.isLetterAscii() or c.isDigit()):
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
  let props = MarkdownProps(source: source, theme: if theme != nil: theme else newMarkdownTheme())
  createMarkdownComponent(props)

proc newMarkdownFromFile*(filename: string, theme: MarkdownTheme = nil): KryonComponent =
  ## Create a new Markdown component from file
  let props = MarkdownProps(file: filename, theme: if theme != nil: theme else newMarkdownTheme())
  createMarkdownComponent(props)