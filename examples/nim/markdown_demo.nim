## Markdown Component Demo
##
## Demonstrates the new Markdown rendering capabilities with various themes and content types
## Shows real-time markdown parsing and rendering with interactive features

import std/[strutils, tables, os]
import kryon_dsl
import markdown

# Demo state
var
  currentFile = "README.md"
  currentTheme = "light"
  showSource = false
  wordCount = 0
  lineCount = 0

# Sample markdown content for demo
let sampleMarkdown = """
# Kryon Markdown Component Demo

Welcome to the **Kryon Markdown Component**! This demonstrates rich text formatting capabilities.

## Features Demonstrated

### Text Formatting
You can use *italic*, **bold**, ***bold italic***, and `code` formatting.

### Lists

#### Unordered Lists
- First item
- Second item
  - Nested item
  - Another nested item
- Third item

#### Ordered Lists
1. First step
2. Second step
3. Third step

### Code Blocks

#### Regular Code Block
```nim
import kryon_dsl, markdown

let app = kryonApp:
  Body:
    Markdown:
      source = "# Hello **World**!"
```

#### Syntax Highlighted Code
```javascript
function renderMarkdown(markdown) {
    const ast = parse(markdown);
    return render(ast);
}
```

### Blockquotes

> This is a blockquote. It can span multiple lines and is useful for highlighting quotes or important information.
>
> You can also have nested blockquotes like this.

### Links and Images

Visit the [Kryon documentation](https://kryon.dev) for more information.

### Tables

| Feature | Status | Priority |
|---------|--------|----------|
| Parsing | ✅ Complete | High |
| Rendering | ✅ Complete | High |
| Themes | ✅ Complete | Medium |
| Images | 🔄 In Progress | Medium |
| Links | ✅ Complete | High |

### Horizontal Rules

---

### Raw HTML

<mark>This is highlighted text</mark>

### Subscript and Superscript

H₂O is water. X² is math.

### Strikethrough

~~This text is crossed out~~

### Emojis

🚀 Rocket ship 🎨 Art palette 📚 Books

## Interactive Features

The Markdown component supports:
- **Real-time parsing** - Content is parsed immediately
- **Theme switching** - Light, dark, and GitHub themes
- **File loading** - Load markdown from files
- **Link handling** - Click handlers for links
- **Statistics** - Word count, line count, reading time

## Performance

The parser is optimized for:
- **Memory efficiency** - Minimal allocations
- **Speed** - Fast parsing of large documents
- **Cross-platform** - Works from MCUs to desktop

Try the controls below to explore different features!
"""

proc updateStatistics(markdown: string) =
  wordCount = markdown.countWords()
  lineCount = markdown.countLines()

proc createMarkdownFromContent(content: string, theme: string): KryonComponent =
  let themeObj = case theme.toLowerAscii():
    of "dark": darkTheme()
    of "github": githubTheme()
    else: lightTheme()

  Markdown:
    source = content
    theme = themeObj
    width = 700
    height = 600
    onLinkClick = proc(url: string) =
      echo "Link clicked: ", url

proc loadMarkdownFile(filename: string): string =
  if fileExists(filename):
    try:
      result = readFile(filename)
      updateStatistics(result)
    except:
      result = "# Error loading file: " & filename & "\n\nFile may not exist or may not be readable."
  else:
    result = "# File Not Found\n\nThe file '" & filename & "' does not exist."

let app = kryonApp:
  Header:
    windowWidth = 1000
    windowHeight = 800
    windowTitle = "Markdown Component Demo"

  Body:
    backgroundColor = "#f8f9fa"

    # Top control panel
    Container:
      width = 1000
      height = 80
      backgroundColor = "#ffffff"
      padding = 20

      Row:
        # File selection
        Container:
          width = 200
          height = 40

          Button:
            text = "README.md"
            onClick = proc() =
              currentFile = "README.md"
              updateStatistics(loadMarkdownFile(currentFile))

        Button:
          text = "Sample.md"
          onClick = proc() =
              currentFile = "sample.md"
              updateStatistics(sampleMarkdown)

        # Theme selection
        Container:
          width = 200
          height = 40
          marginLeft = 20

          Button:
            text = "Light Theme"
            onClick = proc() = currentTheme = "light"

          Button:
            text = "Dark Theme"
            onClick = proc() = currentTheme = "dark"

          Button:
            text = "GitHub Theme"
            onClick = proc() = currentTheme = "github"

        # Statistics
        Container:
          flexGrow = 1
          height = 40
          backgroundColor = "#e9ecef"
          padding = 10

          Text:
            content = "Words: " & $wordCount & " | Lines: " & $lineCount & " | Reading time: " & $(wordCount div 200) & " min"
            color = "#495057"

        # Toggle source view
        Container:
          width = 120
          height = 40

          Button:
            text = if showSource: "Hide Source" else: "Show Source"
            onClick = proc() = showSource = not showSource

    # Main content area
    Row:
      # Markdown content
      Container:
        width = if showSource: 500 else: 1000
        flexGrow = 1
        backgroundColor = "#ffffff"
        padding = 20

        if showSource:
          # Show source code
          Text:
            content = sampleMarkdown
            color = "#495057"
            fontSize = 12
        else:
          # Show rendered markdown
          createMarkdownFromContent(
            if currentFile == "sample.md": sampleMarkdown
            else: loadMarkdownFile(currentFile),
            currentTheme
          )

      # Source code panel (when shown)
      if showSource:
        Container:
          width = 500
          backgroundColor = "#f1f3f4"
          padding = 20

          Text:
            content = "Source Code:"
            color = "#495057"
            fontSize = 16
            marginBottom = 10

          Text:
            content = sampleMarkdown
            color = "#212529"
            fontSize = 12

    # Footer information
    Container:
      width = 1000
      height = 60
      backgroundColor = "#e9ecef"
      padding = 20

      Column:
        Text:
          content = "Markdown Component Demo"
          color = "#495057"
          fontSize = 14
          alignSelf = "center"

        Text:
          content = "Supports CommonMark + GitHub Flavored Markdown • Real-time parsing • Multiple themes • Interactive features"
          color = "#6c757d"
          fontSize = 12
          alignSelf = "center"

echo "Markdown Component Demo Started!"
echo "Features demonstrated:"
echo "- Full CommonMark compliance with GitHub Flavored Markdown extensions"
echo "- Real-time parsing and rendering"
echo "- Multiple built-in themes (light, dark, GitHub)"
echo "- File loading and content display"
echo "- Interactive link handling"
echo "- Statistics tracking (words, lines, reading time)"
echo "- Source code view toggle"
echo "- Responsive layout with control panel"
echo ""
echo "Try the controls above to explore different features!"