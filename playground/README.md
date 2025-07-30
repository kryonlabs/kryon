# Kryon Playground

A live code editor and preview environment for Kryon, built entirely in Kryon itself.

## Features

### Core Features
- ✅ **Split-pane Interface**: Resizable code editor on the left, live preview on the right
- ✅ **Syntax Highlighting**: Full Kryon syntax highlighting with semantic coloring
- ✅ **Auto-compilation**: Automatically compiles code as you type (with debouncing)
- ✅ **Live Preview**: See your Kryon applications running in real-time
- ✅ **Line Numbers**: Synchronized line numbers in the editor
- ✅ **Status Bar**: Shows compilation status, cursor position, and file type

### Toolbar Features
- ✅ **Run**: Manually compile and preview code
- ✅ **Save**: Save your work to disk
- ✅ **Open**: Load existing Kryon files
- ✅ **Examples**: Quick access to example templates
- ✅ **Settings**: Configure editor preferences (coming soon)
- ✅ **Export**: Export compiled .krb files

### Editor Features
- ✅ **Multiline Editing**: Full text editor with multiline support
- ✅ **Syntax Highlighting**: Keywords, strings, numbers, comments, properties
- ✅ **Auto-indentation**: Smart indentation (planned)
- ✅ **Code Folding**: Collapse/expand code blocks (planned)
- ✅ **Autocomplete**: IntelliSense-style completion (planned)

## Project Structure

```
playground/
├── src/
│   ├── playground.kry           # Basic playground implementation
│   ├── playground_enhanced.kry   # Enhanced version with all features
│   └── syntax_highlighter.kry    # Reusable syntax highlighting component
└── README.md
```

## Usage

To run the playground:

```bash
# Compile the enhanced playground
kryon compile playground/src/playground_enhanced.kry

# Run with HTML renderer for web-based experience
kryon -r html playground/src/playground_enhanced.krb

# Or run with native renderer
kryon -r wgpu playground/src/playground_enhanced.krb
```

## Architecture

### Components

1. **Main Application** (`playground_enhanced.kry`)
   - Manages the overall UI layout
   - Handles file operations
   - Coordinates compilation and preview

2. **Syntax Highlighter** (`syntax_highlighter.kry`)
   - Tokenizes Kryon code
   - Applies semantic highlighting
   - Reusable component

3. **Editor Pane**
   - Code editor with transparent text input
   - Syntax highlighting overlay
   - Line numbers display

4. **Preview Pane**
   - EmbedView for compiled applications
   - Error display for compilation failures
   - Export functionality

### Key Design Decisions

1. **Transparent Editor**: The actual TextInput is transparent, with syntax highlighting rendered underneath. This allows for native text editing while maintaining visual syntax highlighting.

2. **Component-based Architecture**: The syntax highlighter is a separate, reusable component that can be used in other projects.

3. **Auto-compilation with Debouncing**: Changes trigger compilation after 1.5 seconds of inactivity to avoid excessive compilations.

4. **Embedded Preview**: Uses Kryon's EmbedView to run compiled applications in an isolated context.

## Example Templates

The playground includes two built-in examples:

1. **Hello World**: Simple centered text application
2. **Counter App**: Interactive counter with increase/decrease/reset buttons

## Future Enhancements

### Phase 1 (Current)
- ✅ Basic editor with syntax highlighting
- ✅ Live preview
- ✅ File operations
- ✅ Example templates

### Phase 2 (Planned)
- [ ] Code completion/IntelliSense
- [ ] Error highlighting in editor
- [ ] Code folding
- [ ] Find and replace
- [ ] Multiple file support (tabs)

### Phase 3 (Future)
- [ ] Git integration
- [ ] Plugin system
- [ ] Collaborative editing
- [ ] Deployment options
- [ ] Component library browser

## Technical Details

### Syntax Highlighting

The syntax highlighter recognizes:
- **Keywords**: `style`, `App`, `Container`, `Text`, etc.
- **Properties**: `background_color`, `text_color`, `width`, etc.
- **Strings**: Double-quoted strings with escape sequences
- **Numbers**: Integer and floating-point numbers
- **Comments**: Single-line comments starting with `//`
- **Brackets**: Curly braces, square brackets, parentheses

### Compilation Process

1. Code is written to a temporary file
2. Kryon compiler is invoked via command line
3. On success, the .krb file is loaded in an EmbedView
4. On failure, error messages are displayed in the preview pane

## Contributing

This playground is part of the Kryon project and serves as both a development tool and a showcase of Kryon's capabilities. Contributions are welcome!

## License

Same as the Kryon project - 0BSD License