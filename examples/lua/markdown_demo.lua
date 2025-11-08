-- Markdown Component Demo
--
-- Demonstrates the new Markdown rendering capabilities with various themes and content types
-- Shows real-time markdown parsing and rendering with interactive features

local kryon = require("../../../bindings/lua/init")
local markdown = require("../../../bindings/lua/markdown")

-- Demo state
local current_file = "README.md"
local current_theme = "light"
local show_source = false
local word_count = 0
local line_count = 0

-- Sample markdown content for demo
local sample_markdown = [[
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
]]

local function updateStatistics(markdown_text)
    -- Count words (split by whitespace)
    local words = {}
    for word in markdown_text:gmatch("%S+") do
        table.insert(words, word)
    end
    word_count = #words

    -- Count lines
    line_count = 0
    for _ in markdown_text:gmatch("\n") do
        line_count = line_count + 1
    end
    line_count = line_count + 1  -- Add 1 for the last line
end

local function createMarkdownFromContent(content, theme_name)
    local theme_obj = nil
    if theme_name == "dark" then
        theme_obj = markdown.themeDark()
    elseif theme_name == "github" then
        theme_obj = markdown.themeGitHub()
    else
        theme_obj = markdown.themeLight()
    end

    return kryon.markdown({
        source = content,
        width = 700,
        height = 600,
        theme = theme_obj,
        on_link_click = function(url)
            print("Link clicked:", url)
        end
    })
end

local function loadMarkdownFile(filename)
    local file = io.open(filename, "r")
    if file then
        local content = file:read("*all")
        file:close()
        updateStatistics(content)
        return content
    else
        return "# File Not Found\n\nThe file '" .. filename .. "' does not exist."
    end
end

-- Initialize statistics
updateStatistics(sample_markdown)

-- Main application setup
local app = kryon.app({
    width = 1000,
    height = 800,
    title = "Markdown Component Demo",

    body = kryon.container({
        width = 1000,
        height = 800,
        background = 0xf8f9fa,

        children = {
            -- Top control panel
            kryon.container({
                width = 1000,
                height = 80,
                background = 0xffffff,
                x = 0,
                y = 0,

                children = {
                    -- Use row layout for controls
                    kryon.row({
                        x = 20,
                        y = 20,
                        gap = 10,

                        children = {
                            -- File selection
                            kryon.button({
                                text = "README.md",
                                on_click = function()
                                    current_file = "README.md"
                                    updateStatistics(loadMarkdownFile(current_file))
                                end
                            }),

                            kryon.button({
                                text = "Sample.md",
                                on_click = function()
                                    current_file = "sample.md"
                                    updateStatistics(sample_markdown)
                                end
                            }),

                            -- Theme selection
                            kryon.button({
                                text = "Light Theme",
                                on_click = function()
                                    current_theme = "light"
                                end
                            }),

                            kryon.button({
                                text = "Dark Theme",
                                on_click = function()
                                    current_theme = "dark"
                                end
                            }),

                            kryon.button({
                                text = "GitHub Theme",
                                on_click = function()
                                    current_theme = "github"
                                end
                            }),

                            -- Statistics display
                            kryon.container({
                                flexGrow = 1,
                                width = 300,
                                height = 40,
                                background = 0xe9ecef,
                                x = 400,
                                y = 20,

                                children = {
                                    kryon.text({
                                        content = "Words: " .. word_count .. " | Lines: " .. line_count .. " | Reading time: " .. math.floor(word_count / 200) .. " min",
                                        color = 0x495057,
                                        font_size = 12
                                    })
                                }
                            }),

                            -- Toggle source view
                            kryon.button({
                                text = show_source and "Hide Source" or "Show Source",
                                on_click = function()
                                    show_source = not show_source
                                end
                            })
                        }
                    })
                }
            }),

            -- Main content area (positioned below control panel)
            kryon.container({
                width = 1000,
                height = 620,
                x = 0,
                y = 80,

                children = {
                    -- Conditional content based on show_source
                    function()
                        if show_source then
                            -- Show source code side by side
                            return {
                                -- Source code panel
                                kryon.container({
                                    width = 500,
                                    height = 600,
                                    background = 0xf1f3f4,
                                    x = 0,
                                    y = 0,

                                    children = {
                                        kryon.text({
                                            content = "Source Code:",
                                            color = 0x495057,
                                            font_size = 16,
                                            x = 20,
                                            y = 20
                                        }),

                                        kryon.text({
                                            content = sample_markdown,
                                            color = 0x212529,
                                            font_size = 12,
                                            x = 20,
                                            y = 50
                                        })
                                    }
                                }),

                                -- Markdown content panel
                                kryon.container({
                                    width = 500,
                                    height = 600,
                                    background = 0xffffff,
                                    x = 500,
                                    y = 0,

                                    children = {
                                        createMarkdownFromContent(
                                            current_file == "sample.md" and sample_markdown or loadMarkdownFile(current_file),
                                            current_theme
                                        )
                                    }
                                })
                            }
                        else
                            -- Show only markdown content
                            return {
                                kryon.container({
                                    width = 1000,
                                    height = 600,
                                    background = 0xffffff,
                                    x = 0,
                                    y = 0,

                                    children = {
                                        createMarkdownFromContent(
                                            current_file == "sample.md" and sample_markdown or loadMarkdownFile(current_file),
                                            current_theme
                                        )
                                    }
                                })
                            }
                        end
                    end
                }
            }),

            -- Footer information
            kryon.container({
                width = 1000,
                height = 60,
                background = 0xe9ecef,
                x = 0,
                y = 700,

                children = {
                    kryon.column({
                        x = 20,
                        y = 20,
                        gap = 5,

                        children = {
                            kryon.text({
                                content = "Markdown Component Demo",
                                color = 0x495057,
                                font_size = 14
                            }),

                            kryon.text({
                                content = "Supports CommonMark + GitHub Flavored Markdown • Real-time parsing • Multiple themes • Interactive features",
                                color = 0x6c757d,
                                font_size = 12
                            })
                        }
                    })
                }
            })
        }
    })
})

print("Markdown Component Demo Started!")
print("Features demonstrated:")
print("- Full CommonMark compliance with GitHub Flavored Markdown extensions")
print("- Real-time parsing and rendering")
print("- Multiple built-in themes (light, dark, GitHub)")
print("- File loading and content display")
print("- Interactive link handling")
print("- Statistics tracking (words, lines, reading time)")
print("- Source code view toggle")
print("- Responsive layout with control panel")
print("")
print("Try the controls above to explore different features!")