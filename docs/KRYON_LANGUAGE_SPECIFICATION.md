# Kryon Language Specification v1.0

## Overview
Kryon is a self-hosted language designed for creating native UI applications with integrated rendering capabilities. Unlike traditional frameworks that rely on host languages, Kryon provides a complete runtime environment where all components are written in Kryon itself.

## Core Language Features

### 1. Data Types
```kryon
// Primitives
let intValue: int = 42
let floatValue: float = 3.14
let boolValue: bool = true
let stringValue: string = "Hello Kryon"

// Collections
let array: [int] = [1, 2, 3, 4, 5]
let map: {string: int} = {"key1": 10, "key2": 20}

// Optional types
let optional: int? = null
let maybeValue: string? = "exists"
```

### 2. Functions and Modules
```kryon
// Function definition
func calculateArea(width: float, height: float) -> float {
    return width * height
}

// Module system
module Math {
    export func sin(x: float) -> float {
        // Native implementation
        return nativeSin(x)
    }
    
    export func cos(x: float) -> float {
        return nativeCos(x)
    }
}

// Import modules
import Math
let result = Math.sin(1.57)
```

### 3. UI Definition Language (KRY)
```kryon
// UI components are defined using KRY syntax within Kryon
component WelcomeScreen() -> Element {
    return @element "container" {
        @style {
            display: "flex"
            flexDirection: "column"
            alignItems: "center"
            justifyContent: "center"
            padding: 20px
            backgroundColor: "#191970FF"
        }
        
        @element "text" {
            @properties {
                text: "Welcome to Kryon"
                fontSize: 24
                color: "#FFFFFFFF"
                fontWeight: "bold"
            }
        }
        
        @element "button" {
            @properties {
                text: "Click Me"
                onClick: handleClick
            }
            @style {
                padding: 10px
                backgroundColor: "#0099FFFF"
                borderRadius: 5px
                cursor: "pointer"
            }
        }
    }
}

func handleClick() {
    print("Button clicked!")
}
```

## Self-Hosted Architecture

### 1. Kryon Compiler (Written in Kryon)
```kryon
module KryonCompiler {
    struct Token {
        type: TokenType
        value: string
        line: int
        column: int
    }
    
    struct Lexer {
        source: string
        position: int
        line: int
        column: int
    }
    
    export func tokenize(source: string) -> [Token] {
        let lexer = Lexer {
            source: source,
            position: 0,
            line: 1,
            column: 1
        }
        
        let tokens: [Token] = []
        
        while lexer.position < lexer.source.length {
            let token = nextToken(&lexer)
            tokens.append(token)
        }
        
        return tokens
    }
    
    func nextToken(lexer: &Lexer) -> Token {
        skipWhitespace(lexer)
        
        if isLetter(lexer.current()) {
            return readIdentifier(lexer)
        } else if isDigit(lexer.current()) {
            return readNumber(lexer)
        } else if lexer.current() == '"' {
            return readString(lexer)
        }
        
        // Handle operators and punctuation
        return readOperator(lexer)
    }
}
```

### 2. Parser Implementation (Kryon Native)
```kryon
module KryonParser {
    struct ASTNode {
        type: NodeType
        value: string?
        children: [ASTNode]
        position: Position
    }
    
    struct Parser {
        tokens: [Token]
        current: int
    }
    
    export func parseProgram(tokens: [Token]) -> ASTNode {
        let parser = Parser {
            tokens: tokens,
            current: 0
        }
        
        return parseModule(&parser)
    }
    
    func parseModule(parser: &Parser) -> ASTNode {
        let module = ASTNode {
            type: .Module,
            value: null,
            children: [],
            position: getCurrentPosition(parser)
        }
        
        while !isAtEnd(parser) {
            let statement = parseStatement(parser)
            module.children.append(statement)
        }
        
        return module
    }
    
    func parseStatement(parser: &Parser) -> ASTNode {
        if match(parser, .FUNC) {
            return parseFunction(parser)
        } else if match(parser, .LET) {
            return parseVariableDeclaration(parser)
        } else if match(parser, .COMPONENT) {
            return parseComponent(parser)
        }
        
        return parseExpression(parser)
    }
}
```

### 3. Code Generator (Kryon to KRB)
```kryon
module KryonCodeGen {
    struct CodeGenerator {
        output: BinaryWriter
        symbolTable: SymbolTable
    }
    
    export func generateCode(ast: ASTNode) -> BinaryData {
        let generator = CodeGenerator {
            output: BinaryWriter.create(),
            symbolTable: SymbolTable.create()
        }
        
        visitNode(&generator, ast)
        return generator.output.toBytes()
    }
    
    func visitNode(generator: &CodeGenerator, node: ASTNode) {
        switch node.type {
            case .Module:
                generateModule(generator, node)
            case .Function:
                generateFunction(generator, node)
            case .Component:
                generateComponent(generator, node)
            case .Expression:
                generateExpression(generator, node)
        }
    }
    
    func generateComponent(generator: &CodeGenerator, node: ASTNode) {
        // Generate KRB bytecode for UI components
        generator.output.writeByte(0x01) // Component header
        generator.output.writeString(node.name)
        
        for child in node.children {
            visitNode(generator, child)
        }
    }
}
```

### 4. Runtime System (Kryon Native)
```kryon
module KryonRuntime {
    struct Runtime {
        elementTree: ElementTree
        renderer: Renderer
        eventSystem: EventSystem
        memoryManager: MemoryManager
    }
    
    export func createRuntime(config: RuntimeConfig) -> Runtime {
        return Runtime {
            elementTree: ElementTree.create(),
            renderer: createRenderer(config.rendererType),
            eventSystem: EventSystem.create(),
            memoryManager: MemoryManager.create()
        }
    }
    
    export func loadKRB(runtime: &Runtime, data: BinaryData) -> bool {
        let reader = BinaryReader.create(data)
        
        while !reader.isAtEnd() {
            let element = parseElement(&reader)
            runtime.elementTree.addElement(element)
        }
        
        return true
    }
    
    export func render(runtime: &Runtime) {
        runtime.renderer.clear()
        renderElementTree(&runtime.renderer, runtime.elementTree)
        runtime.renderer.present()
    }
}
```

### 5. Renderer Implementation (Kryon Native)
```kryon
module KryonRenderer {
    interface Renderer {
        func clear()
        func drawRect(x: float, y: float, width: float, height: float, color: Color)
        func drawText(text: string, x: float, y: float, font: Font, color: Color)
        func present()
    }
    
    // Software renderer implementation
    struct SoftwareRenderer implements Renderer {
        framebuffer: PixelBuffer
        width: int
        height: int
    }
    
    impl SoftwareRenderer {
        export func create(width: int, height: int) -> SoftwareRenderer {
            return SoftwareRenderer {
                framebuffer: PixelBuffer.create(width, height),
                width: width,
                height: height
            }
        }
        
        func clear() {
            framebuffer.fill(Color.black())
        }
        
        func drawRect(x: float, y: float, width: float, height: float, color: Color) {
            let startX = clamp(x as int, 0, this.width)
            let startY = clamp(y as int, 0, this.height)
            let endX = clamp((x + width) as int, 0, this.width)
            let endY = clamp((y + height) as int, 0, this.height)
            
            for py in startY..endY {
                for px in startX..endX {
                    framebuffer.setPixel(px, py, color)
                }
            }
        }
        
        func present() {
            // Platform-specific presentation
            Platform.presentFramebuffer(framebuffer)
        }
    }
    
    // GPU renderer implementation
    struct GPURenderer implements Renderer {
        context: GPUContext
        commandBuffer: CommandBuffer
        shaderProgram: ShaderProgram
    }
    
    impl GPURenderer {
        export func create(backend: GPUBackend) -> GPURenderer? {
            let context = GPUContext.create(backend)
            if context == null {
                return null
            }
            
            return GPURenderer {
                context: context!,
                commandBuffer: CommandBuffer.create(),
                shaderProgram: createDefaultShader()
            }
        }
        
        func clear() {
            commandBuffer.clear(Color.black())
        }
        
        func drawRect(x: float, y: float, width: float, height: float, color: Color) {
            let vertices = createRectVertices(x, y, width, height)
            commandBuffer.drawTriangles(vertices, shaderProgram, color)
        }
        
        func present() {
            context.submit(commandBuffer)
            context.present()
            commandBuffer.reset()
        }
    }
}
```

### 6. Platform Abstraction (Kryon Native)
```kryon
module KryonPlatform {
    interface Platform {
        func createWindow(width: int, height: int, title: string) -> Window?
        func pollEvents() -> [Event]
        func getCurrentTime() -> float
        func readFile(path: string) -> string?
        func writeFile(path: string, content: string) -> bool
    }
    
    // Platform-specific implementations
    #if platform(linux)
    struct LinuxPlatform implements Platform {
        display: X11Display
        
        func createWindow(width: int, height: int, title: string) -> Window? {
            return X11Window.create(display, width, height, title)
        }
        
        func pollEvents() -> [Event] {
            return X11.pollEvents(display)
        }
    }
    #endif
    
    #if platform(web)
    struct WebPlatform implements Platform {
        func createWindow(width: int, height: int, title: string) -> Window? {
            return WebGLCanvas.create(width, height)
        }
        
        func pollEvents() -> [Event] {
            return DOM.getEvents()
        }
    }
    #endif
}
```

## Application Entry Point
```kryon
// main.kryon - Application entry point
import KryonRuntime
import KryonPlatform

func main() {
    let platform = Platform.current()
    let window = platform.createWindow(800, 600, "Kryon App")
    
    if window == null {
        print("Failed to create window")
        return
    }
    
    let runtime = KryonRuntime.createRuntime(RuntimeConfig {
        rendererType: .GPU,
        platform: platform
    })
    
    // Load KRB file compiled from KRY
    let krbData = platform.readFile("app.krb")
    if krbData == null {
        print("Failed to load app.krb")
        return
    }
    
    KryonRuntime.loadKRB(&runtime, krbData!)
    
    // Main application loop
    while window.isOpen() {
        let events = platform.pollEvents()
        
        for event in events {
            handleEvent(&runtime, event)
        }
        
        KryonRuntime.update(&runtime)
        KryonRuntime.render(&runtime)
    }
}
```

## Compilation Pipeline

### 1. KRY to Kryon
UI definitions written in KRY are transpiled to Kryon component functions.

### 2. Kryon to KRB
Kryon source code is compiled to KRB binary format using the self-hosted compiler.

### 3. KRB Execution
The Kryon runtime loads and executes KRB files, managing the UI lifecycle.

## Benefits of Self-Hosting

1. **Consistency**: Everything uses the same language and paradigms
2. **Performance**: No language boundary overhead
3. **Introspection**: Full access to compilation and runtime internals
4. **Extensibility**: Easy to add language features and optimizations
5. **Portability**: Single codebase for all platforms
6. **Debugging**: Unified debugging experience across all components

This specification shows how Kryon can be a complete, self-contained language ecosystem where the compiler, runtime, and renderer are all written in Kryon itself, eliminating the need for C infrastructure.