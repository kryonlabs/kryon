# Kryon Multi-Target Compilation Pipeline Specification

Complete specification of the Kryon compilation system that transforms KRY source code into optimized platform-specific outputs via the KRB intermediate format.

> **🔗 Related Documentation**: 
> - [KRY_LANGUAGE_SPECIFICATION.md](../language/KRY_LANGUAGE_SPECIFICATION.md) - Source language syntax
> - [KRB_BINARY_FORMAT_SPECIFICATION.md](../binary-format/KRB_BINARY_FORMAT_SPECIFICATION.md) - Intermediate format
> - [KRYON_RUNTIME_ARCHITECTURE.md](KRYON_RUNTIME_ARCHITECTURE.md) - Runtime system

## Table of Contents
- [Pipeline Overview](#pipeline-overview)
- [Compilation Phases](#compilation-phases)
- [Intermediate Representations](#intermediate-representations)
- [Target Platform Generation](#target-platform-generation)
- [Optimization Pipeline](#optimization-pipeline)
- [Build System Integration](#build-system-integration)
- [Development Tools](#development-tools)
- [Performance Characteristics](#performance-characteristics)

---

## Pipeline Overview

The Kryon compilation pipeline is designed for maximum flexibility and performance, supporting compilation to multiple target platforms from a single KRY source codebase:

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   KRY       │    │    AST      │    │    KRB      │    │  Platform   │
│  Source     │───▶│ (Parsed)    │───▶│ (Binary)    │───▶│  Outputs    │
│  Files      │    │ Tree        │    │ Format      │    │             │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
      │                    │                   │                   │
      ▼                    ▼                   ▼                   ▼
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  Lexical    │    │  Semantic   │    │ Binary      │    │ Web HTML/   │
│  Analysis   │    │  Analysis   │    │ Generation  │    │ CSS/JS      │
│             │    │             │    │             │    │             │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
                          │                   │                   │
                          ▼                   ▼                   ▼
                 ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
                 │ Type        │    │ Optimization│    │ Native      │
                 │ Checking    │    │ Passes      │    │ Mobile/     │
                 │             │    │             │    │ Desktop     │
                 └─────────────┘    └─────────────┘    └─────────────┘
```

### Core Design Principles

1. **Single Source, Multiple Targets**: One KRY codebase compiles to all platforms
2. **Incremental Compilation**: Only recompile changed files and dependencies
3. **Aggressive Optimization**: Dead code elimination, constant folding, inlining
4. **Platform Specialization**: Generate optimal code for each target platform
5. **Developer Experience**: Fast compilation with detailed error reporting

---

## Compilation Phases

### Phase 1: Lexical Analysis (Tokenization)

```rust
pub struct Lexer {
    input: String,
    position: usize,
    line: usize,
    column: usize,
    tokens: Vec<Token>,
}

#[derive(Debug, Clone, PartialEq)]
pub enum TokenType {
    // Literals
    Identifier(String),
    StringLiteral(String),
    NumberLiteral(f64),
    BooleanLiteral(bool),
    
    // Keywords
    Define, App, Container, Text, Button, Input,
    
    // Operators
    LeftBrace, RightBrace, LeftParen, RightParen,
    Colon, Semicolon, Comma, Dot,
    Plus, Minus, Star, Slash, Percent,
    Equal, NotEqual, Less, Greater, LessEqual, GreaterEqual,
    
    // Directives
    At, // @
    Dollar, // $ for variables
    
    // Special
    Newline, Eof, Error(String),
}

impl Lexer {
    pub fn tokenize(&mut self) -> Result<Vec<Token>, CompileError> {
        while !self.at_end() {
            self.scan_token()?;
        }
        
        self.tokens.push(Token {
            token_type: TokenType::Eof,
            lexeme: String::new(),
            line: self.line,
            column: self.column,
        });
        
        Ok(self.tokens.clone())
    }
    
    fn scan_token(&mut self) -> Result<(), CompileError> {
        let c = self.advance();
        
        match c {
            ' ' | '\r' | '\t' => {}, // Ignore whitespace
            '\n' => {
                self.add_token(TokenType::Newline);
                self.line += 1;
                self.column = 1;
            },
            '{' => self.add_token(TokenType::LeftBrace),
            '}' => self.add_token(TokenType::RightBrace),
            ':' => self.add_token(TokenType::Colon),
            '@' => self.add_token(TokenType::At),
            '$' => self.add_token(TokenType::Dollar),
            '"' => self.scan_string()?,
            c if c.is_ascii_digit() => self.scan_number()?,
            c if c.is_ascii_alphabetic() || c == '_' => self.scan_identifier()?,
            _ => return Err(CompileError::UnexpectedCharacter { 
                char: c, 
                line: self.line, 
                column: self.column 
            }),
        }
        
        Ok(())
    }
    
    fn scan_string(&mut self) -> Result<(), CompileError> {
        let mut value = String::new();
        
        while self.peek() != '"' && !self.at_end() {
            if self.peek() == '\n' {
                self.line += 1;
                self.column = 1;
            }
            
            // Handle escape sequences
            if self.peek() == '\\' {
                self.advance(); // consume backslash
                match self.advance() {
                    'n' => value.push('\n'),
                    't' => value.push('\t'),
                    'r' => value.push('\r'),
                    '\\' => value.push('\\'),
                    '"' => value.push('"'),
                    c => return Err(CompileError::InvalidEscapeSequence { 
                        char: c,
                        line: self.line,
                        column: self.column 
                    }),
                }
            } else {
                value.push(self.advance());
            }
        }
        
        if self.at_end() {
            return Err(CompileError::UnterminatedString { 
                line: self.line, 
                column: self.column 
            });
        }
        
        self.advance(); // closing "
        self.add_token(TokenType::StringLiteral(value));
        Ok(())
    }
}
```

### Phase 2: Parsing (AST Generation)

```rust
pub struct Parser {
    tokens: Vec<Token>,
    current: usize,
    errors: Vec<ParseError>,
}

#[derive(Debug, Clone)]
pub enum ASTNode {
    App {
        properties: Vec<Property>,
        children: Vec<ASTNode>,
        directives: Vec<Directive>,
        span: Span,
    },
    Element {
        element_type: ElementType,
        properties: Vec<Property>,
        children: Vec<ASTNode>,
        span: Span,
    },
    Property {
        name: String,
        value: PropertyValue,
        span: Span,
    },
    Directive {
        directive_type: DirectiveType,
        content: DirectiveContent,
        span: Span,
    },
    Script {
        language: ScriptLanguage,
        code: String,
        span: Span,
    },
}

#[derive(Debug, Clone)]
pub enum DirectiveType {
    Variables,
    Styles,
    Import,
    Metadata,
    Store,
    Api,
    Mobile,
    Build,
    Security,
    Testing,
    Environment,
}

impl Parser {
    pub fn parse(&mut self) -> Result<ASTNode, Vec<ParseError>> {
        let app = self.parse_app()?;
        
        if !self.errors.is_empty() {
            return Err(self.errors.clone());
        }
        
        Ok(app)
    }
    
    fn parse_app(&mut self) -> Result<ASTNode, ParseError> {
        self.consume(TokenType::Identifier("App".to_string()), "Expected 'App'")?;
        self.consume(TokenType::LeftBrace, "Expected '{' after 'App'")?;
        
        let mut properties = Vec::new();
        let mut children = Vec::new();
        let mut directives = Vec::new();
        
        while !self.check(TokenType::RightBrace) && !self.at_end() {
            if self.check(TokenType::At) {
                // Parse directive
                directives.push(self.parse_directive()?);
            } else if self.is_property() {
                // Parse property
                properties.push(self.parse_property()?);
            } else {
                // Parse child element
                children.push(self.parse_element()?);
            }
        }
        
        self.consume(TokenType::RightBrace, "Expected '}' after app body")?;
        
        Ok(ASTNode::App {
            properties,
            children,
            directives,
            span: self.get_span(),
        })
    }
    
    fn parse_directive(&mut self) -> Result<Directive, ParseError> {
        self.consume(TokenType::At, "Expected '@'")?;
        
        let directive_name = match &self.advance().token_type {
            TokenType::Identifier(name) => name.clone(),
            _ => return Err(ParseError::ExpectedDirectiveName),
        };
        
        let directive_type = match directive_name.as_str() {
            "variables" => DirectiveType::Variables,
            "styles" => DirectiveType::Styles,
            "import" => DirectiveType::Import,
            "store" => DirectiveType::Store,
            "api" => DirectiveType::Api,
            "mobile" => DirectiveType::Mobile,
            "build" => DirectiveType::Build,
            "security" => DirectiveType::Security,
            "testing" => DirectiveType::Testing,
            "environment" => DirectiveType::Environment,
            _ => return Err(ParseError::UnknownDirective(directive_name)),
        };
        
        let content = self.parse_directive_content(directive_type)?;
        
        Ok(Directive {
            directive_type,
            content,
            span: self.get_span(),
        })
    }
    
    fn parse_directive_content(&mut self, directive_type: DirectiveType) -> Result<DirectiveContent, ParseError> {
        match directive_type {
            DirectiveType::Variables => {
                self.consume(TokenType::LeftBrace, "Expected '{' after @variables")?;
                let mut variables = Vec::new();
                
                while !self.check(TokenType::RightBrace) && !self.at_end() {
                    let name = self.consume_identifier("Expected variable name")?;
                    self.consume(TokenType::Colon, "Expected ':' after variable name")?;
                    let value = self.parse_expression()?;
                    
                    variables.push(VariableDeclaration { name, value });
                }
                
                self.consume(TokenType::RightBrace, "Expected '}' after variables")?;
                Ok(DirectiveContent::Variables(variables))
            },
            
            DirectiveType::Store => {
                let name = match &self.advance().token_type {
                    TokenType::StringLiteral(s) => s.clone(),
                    _ => return Err(ParseError::ExpectedStoreName),
                };
                
                self.consume(TokenType::LeftBrace, "Expected '{' after store name")?;
                let store_config = self.parse_store_config()?;
                self.consume(TokenType::RightBrace, "Expected '}' after store config")?;
                
                Ok(DirectiveContent::Store { name, config: store_config })
            },
            
            // ... other directive types
            _ => todo!("Implement other directive parsers"),
        }
    }
}
```

### Phase 3: Semantic Analysis

```rust
pub struct SemanticAnalyzer {
    /// Symbol table for variable/function resolution
    symbol_table: SymbolTable,
    
    /// Type information
    type_checker: TypeChecker,
    
    /// Error collector
    errors: Vec<SemanticError>,
    
    /// Import resolver
    import_resolver: ImportResolver,
}

#[derive(Debug, Clone)]
pub struct SymbolTable {
    scopes: Vec<Scope>,
    current_scope: usize,
}

#[derive(Debug, Clone)]
pub struct Scope {
    symbols: HashMap<String, Symbol>,
    parent: Option<usize>,
}

#[derive(Debug, Clone)]
pub struct Symbol {
    pub name: String,
    pub symbol_type: SymbolType,
    pub span: Span,
    pub is_mutable: bool,
}

impl SemanticAnalyzer {
    pub fn analyze(&mut self, ast: &mut ASTNode) -> Result<(), Vec<SemanticError>> {
        self.visit_node(ast)?;
        
        if !self.errors.is_empty() {
            return Err(self.errors.clone());
        }
        
        Ok(())
    }
    
    fn visit_node(&mut self, node: &mut ASTNode) -> Result<(), SemanticError> {
        match node {
            ASTNode::App { directives, children, .. } => {
                // Process directives first (they might define symbols)
                for directive in directives {
                    self.analyze_directive(directive)?;
                }
                
                // Then process children
                for child in children {
                    self.visit_node(child)?;
                }
            },
            
            ASTNode::Element { properties, children, .. } => {
                // Check property types and values
                for property in properties {
                    self.check_property(property)?;
                }
                
                // Process children
                for child in children {
                    self.visit_node(child)?;
                }
            },
            
            ASTNode::Property { name, value, span } => {
                self.check_property_value(name, value, *span)?;
            },
            
            ASTNode::Script { language, code, span } => {
                self.analyze_script(*language, code, *span)?;
            },
            
            _ => {},
        }
        
        Ok(())
    }
    
    fn analyze_directive(&mut self, directive: &Directive) -> Result<(), SemanticError> {
        match &directive.directive_type {
            DirectiveType::Variables => {
                if let DirectiveContent::Variables(vars) = &directive.content {
                    for var in vars {
                        // Check for redefinition
                        if self.symbol_table.lookup(&var.name).is_some() {
                            return Err(SemanticError::VariableRedefinition {
                                name: var.name.clone(),
                                span: directive.span,
                            });
                        }
                        
                        // Add to symbol table
                        self.symbol_table.define(Symbol {
                            name: var.name.clone(),
                            symbol_type: SymbolType::Variable(self.infer_type(&var.value)),
                            span: directive.span,
                            is_mutable: true,
                        });
                    }
                }
            },
            
            DirectiveType::Import => {
                if let DirectiveContent::Import { path, .. } = &directive.content {
                    // Resolve import and add symbols
                    let imported_symbols = self.import_resolver.resolve(path)?;
                    for symbol in imported_symbols {
                        self.symbol_table.define(symbol);
                    }
                }
            },
            
            // ... other directive types
            _ => {},
        }
        
        Ok(())
    }
    
    fn check_property_value(&mut self, name: &str, value: &PropertyValue, span: Span) -> Result<(), SemanticError> {
        match value {
            PropertyValue::Variable(var_name) => {
                // Check if variable exists
                if self.symbol_table.lookup(var_name).is_none() {
                    return Err(SemanticError::UndefinedVariable {
                        name: var_name.clone(),
                        span,
                    });
                }
            },
            
            PropertyValue::Expression(expr) => {
                self.check_expression(expr, span)?;
            },
            
            PropertyValue::FunctionCall { name, args } => {
                // Check if function exists
                if self.symbol_table.lookup(name).is_none() {
                    return Err(SemanticError::UndefinedFunction {
                        name: name.clone(),
                        span,
                    });
                }
                
                // Check argument types
                for arg in args {
                    self.check_expression(arg, span)?;
                }
            },
            
            _ => {},
        }
        
        Ok(())
    }
}
```

### Phase 4: Optimization Passes

```rust
pub struct OptimizationPipeline {
    passes: Vec<Box<dyn OptimizationPass>>,
    config: OptimizationConfig,
}

pub trait OptimizationPass {
    fn name(&self) -> &str;
    fn run(&mut self, ast: &mut ASTNode, context: &OptimizationContext) -> Result<bool, OptimizationError>;
}

pub struct OptimizationConfig {
    pub level: OptimizationLevel,
    pub target_platform: PlatformType,
    pub preserve_debug_info: bool,
    pub aggressive_inlining: bool,
}

#[derive(Debug, Clone, Copy)]
pub enum OptimizationLevel {
    None,      // -O0: No optimization
    Basic,     // -O1: Basic optimizations
    Standard,  // -O2: Standard optimizations
    Aggressive,// -O3: Aggressive optimizations
}

// Dead Code Elimination Pass
pub struct DeadCodeElimination {
    removed_nodes: usize,
}

impl OptimizationPass for DeadCodeElimination {
    fn name(&self) -> &str { "dead-code-elimination" }
    
    fn run(&mut self, ast: &mut ASTNode, context: &OptimizationContext) -> Result<bool, OptimizationError> {
        let initial_count = self.count_nodes(ast);
        self.eliminate_dead_code(ast, context)?;
        let final_count = self.count_nodes(ast);
        
        self.removed_nodes = initial_count - final_count;
        Ok(self.removed_nodes > 0)
    }
}

impl DeadCodeElimination {
    fn eliminate_dead_code(&mut self, node: &mut ASTNode, context: &OptimizationContext) -> Result<(), OptimizationError> {
        match node {
            ASTNode::App { children, .. } => {
                // Remove unreachable elements
                children.retain(|child| self.is_reachable(child, context));
                
                // Recursively process remaining children
                for child in children {
                    self.eliminate_dead_code(child, context)?;
                }
            },
            
            ASTNode::Element { properties, children, .. } => {
                // Remove elements with visibility: false at compile time
                if let Some(visibility) = self.get_static_visibility(properties) {
                    if !visibility {
                        // Mark for removal by returning early
                        return Ok(());
                    }
                }
                
                // Process children
                children.retain(|child| self.is_reachable(child, context));
                for child in children {
                    self.eliminate_dead_code(child, context)?;
                }
            },
            
            _ => {},
        }
        
        Ok(())
    }
}

// Constant Folding Pass
pub struct ConstantFolding;

impl OptimizationPass for ConstantFolding {
    fn name(&self) -> &str { "constant-folding" }
    
    fn run(&mut self, ast: &mut ASTNode, _context: &OptimizationContext) -> Result<bool, OptimizationError> {
        let mut changed = false;
        self.fold_constants(ast, &mut changed)?;
        Ok(changed)
    }
}

impl ConstantFolding {
    fn fold_constants(&mut self, node: &mut ASTNode, changed: &mut bool) -> Result<(), OptimizationError> {
        match node {
            ASTNode::Property { value, .. } => {
                if let PropertyValue::Expression(expr) = value {
                    if let Some(folded) = self.evaluate_constant_expression(expr)? {
                        *value = PropertyValue::Literal(folded);
                        *changed = true;
                    }
                }
            },
            
            ASTNode::App { children, .. } |
            ASTNode::Element { children, .. } => {
                for child in children {
                    self.fold_constants(child, changed)?;
                }
            },
            
            _ => {},
        }
        
        Ok(())
    }
    
    fn evaluate_constant_expression(&self, expr: &Expression) -> Result<Option<LiteralValue>, OptimizationError> {
        match expr {
            Expression::Binary { left, operator, right } => {
                if let (Some(left_val), Some(right_val)) = (
                    self.evaluate_constant_expression(left)?,
                    self.evaluate_constant_expression(right)?
                ) {
                    match operator {
                        BinaryOperator::Add => {
                            match (left_val, right_val) {
                                (LiteralValue::Number(a), LiteralValue::Number(b)) => {
                                    return Ok(Some(LiteralValue::Number(a + b)));
                                },
                                (LiteralValue::String(a), LiteralValue::String(b)) => {
                                    return Ok(Some(LiteralValue::String(format!("{}{}", a, b))));
                                },
                                _ => {},
                            }
                        },
                        BinaryOperator::Subtract => {
                            if let (LiteralValue::Number(a), LiteralValue::Number(b)) = (left_val, right_val) {
                                return Ok(Some(LiteralValue::Number(a - b)));
                            }
                        },
                        // ... other operators
                        _ => {},
                    }
                }
            },
            
            Expression::Literal(val) => return Ok(Some(val.clone())),
            
            _ => {},
        }
        
        Ok(None)
    }
}

// Template Variable Inlining Pass
pub struct TemplateVariableInlining {
    variable_values: HashMap<String, String>,
}

impl OptimizationPass for TemplateVariableInlining {
    fn name(&self) -> &str { "template-variable-inlining" }
    
    fn run(&mut self, ast: &mut ASTNode, context: &OptimizationContext) -> Result<bool, OptimizationError> {
        // First pass: collect compile-time constant variables
        self.collect_constants(ast)?;
        
        // Second pass: inline constant variables
        let mut changed = false;
        self.inline_variables(ast, &mut changed)?;
        Ok(changed)
    }
}
```

### Phase 5: KRB Binary Generation

```rust
pub struct KRBGenerator {
    /// Output buffer
    buffer: Vec<u8>,
    
    /// String interning table
    string_table: HashMap<String, u16>,
    
    /// Element type mappings
    element_types: HashMap<String, u8>,
    
    /// Property mappings
    property_mappings: HashMap<String, u8>,
    
    /// Directive mappings
    directive_mappings: HashMap<DirectiveType, u8>,
}

impl KRBGenerator {
    pub fn generate(&mut self, ast: &ASTNode) -> Result<Vec<u8>, CodeGenError> {
        // Write file header
        self.write_header()?;
        
        // Build string table
        self.build_string_table(ast)?;
        self.write_string_table()?;
        
        // Generate binary representation
        self.generate_node(ast)?;
        
        Ok(self.buffer.clone())
    }
    
    fn write_header(&mut self) -> Result<(), CodeGenError> {
        // Magic number: "KRBY"
        self.buffer.extend_from_slice(&[0x4B, 0x52, 0x42, 0x59]);
        
        // Version: 1.0.0.0
        self.buffer.extend_from_slice(&[1, 0, 0, 0]);
        
        // Flags: none for now
        self.buffer.extend_from_slice(&[0, 0, 0, 0]);
        
        // Placeholder for element count and total size
        self.buffer.extend_from_slice(&[0, 0, 0, 0]); // element count
        self.buffer.extend_from_slice(&[0, 0, 0, 0]); // total size
        
        Ok(())
    }
    
    fn generate_node(&mut self, node: &ASTNode) -> Result<(), CodeGenError> {
        match node {
            ASTNode::App { properties, children, directives, .. } => {
                // Element type: App (0x00)
                self.write_u8(0x00)?;
                
                // Property count
                let total_properties = properties.len() + directives.len();
                self.write_u16(total_properties as u16)?;
                
                // Write properties
                for property in properties {
                    self.generate_property(property)?;
                }
                
                // Write directives as special properties
                for directive in directives {
                    self.generate_directive(directive)?;
                }
                
                // Child count
                self.write_u16(children.len() as u16)?;
                
                // Write children
                for child in children {
                    self.generate_node(child)?;
                }
            },
            
            ASTNode::Element { element_type, properties, children, .. } => {
                // Element type hex code
                let type_code = self.element_types.get(&element_type.to_string())
                    .ok_or_else(|| CodeGenError::UnknownElementType(element_type.to_string()))?;
                self.write_u8(*type_code)?;
                
                // Property count
                self.write_u16(properties.len() as u16)?;
                
                // Write properties
                for property in properties {
                    self.generate_property(property)?;
                }
                
                // Child count
                self.write_u16(children.len() as u16)?;
                
                // Write children
                for child in children {
                    self.generate_node(child)?;
                }
            },
            
            _ => return Err(CodeGenError::InvalidNodeType),
        }
        
        Ok(())
    }
    
    fn generate_property(&mut self, property: &Property) -> Result<(), CodeGenError> {
        // Property type hex code
        let prop_code = self.property_mappings.get(&property.name)
            .ok_or_else(|| CodeGenError::UnknownProperty(property.name.clone()))?;
        self.write_u8(*prop_code)?;
        
        // Property value
        self.generate_property_value(&property.value)?;
        
        Ok(())
    }
    
    fn generate_property_value(&mut self, value: &PropertyValue) -> Result<(), CodeGenError> {
        match value {
            PropertyValue::String(s) => {
                self.write_u8(0x01)?; // String type
                let string_id = self.get_string_id(s)?;
                self.write_u16(string_id)?;
            },
            
            PropertyValue::Number(n) => {
                self.write_u8(0x02)?; // Number type
                self.write_f32(*n as f32)?;
            },
            
            PropertyValue::Boolean(b) => {
                self.write_u8(0x03)?; // Boolean type
                self.write_u8(if *b { 1 } else { 0 })?;
            },
            
            PropertyValue::Variable(var) => {
                self.write_u8(0x04)?; // Variable reference type
                let string_id = self.get_string_id(var)?;
                self.write_u16(string_id)?;
            },
            
            PropertyValue::Expression(expr) => {
                self.write_u8(0x05)?; // Expression type
                self.generate_expression(expr)?;
            },
            
            _ => return Err(CodeGenError::UnsupportedPropertyValue),
        }
        
        Ok(())
    }
    
    fn generate_directive(&mut self, directive: &Directive) -> Result<(), CodeGenError> {
        let directive_code = self.directive_mappings.get(&directive.directive_type)
            .ok_or_else(|| CodeGenError::UnknownDirective(format!("{:?}", directive.directive_type)))?;
        
        self.write_u8(*directive_code)?;
        
        match &directive.content {
            DirectiveContent::Variables(vars) => {
                self.write_u16(vars.len() as u16)?;
                for var in vars {
                    let name_id = self.get_string_id(&var.name)?;
                    self.write_u16(name_id)?;
                    self.generate_property_value(&var.value)?;
                }
            },
            
            DirectiveContent::Store { name, config } => {
                let name_id = self.get_string_id(name)?;
                self.write_u16(name_id)?;
                self.generate_store_config(config)?;
            },
            
            DirectiveContent::Mobile(config) => {
                self.generate_mobile_config(config)?;
            },
            
            DirectiveContent::Build(config) => {
                self.generate_build_config(config)?;
            },
            
            // ... other directive types
            _ => return Err(CodeGenError::UnsupportedDirective),
        }
        
        Ok(())
    }
    
    // Helper methods for binary writing
    fn write_u8(&mut self, value: u8) -> Result<(), CodeGenError> {
        self.buffer.push(value);
        Ok(())
    }
    
    fn write_u16(&mut self, value: u16) -> Result<(), CodeGenError> {
        self.buffer.extend_from_slice(&value.to_le_bytes());
        Ok(())
    }
    
    fn write_u32(&mut self, value: u32) -> Result<(), CodeGenError> {
        self.buffer.extend_from_slice(&value.to_le_bytes());
        Ok(())
    }
    
    fn write_f32(&mut self, value: f32) -> Result<(), CodeGenError> {
        self.buffer.extend_from_slice(&value.to_le_bytes());
        Ok(())
    }
}
```

---

## Target Platform Generation

### Web Target Generator

```rust
pub struct WebTargetGenerator {
    config: WebTargetConfig,
    template_engine: TemplateEngine,
}

pub struct WebTargetConfig {
    pub output_directory: String,
    pub index_template: String,
    pub bundle_js: bool,
    pub minify: bool,
    pub source_maps: bool,
    pub service_worker: bool,
}

impl WebTargetGenerator {
    pub fn generate(&mut self, krb_data: &KRBFile) -> Result<WebOutput, CodeGenError> {
        // Generate HTML structure
        let html = self.generate_html(krb_data)?;
        
        // Generate CSS styles
        let css = self.generate_css(krb_data)?;
        
        // Generate JavaScript runtime
        let js = self.generate_javascript(krb_data)?;
        
        // Generate service worker if enabled
        let service_worker = if self.config.service_worker {
            Some(self.generate_service_worker(krb_data)?)
        } else {
            None
        };
        
        Ok(WebOutput {
            html,
            css,
            js,
            service_worker,
            assets: self.collect_assets(krb_data)?,
        })
    }
    
    fn generate_html(&mut self, krb_data: &KRBFile) -> Result<String, CodeGenError> {
        let mut html = String::new();
        
        // Generate HTML structure from element tree
        self.generate_html_element(&krb_data.root_element, &mut html, 0)?;
        
        // Wrap in template
        let full_html = self.template_engine.render("index.html", &TemplateData {
            title: krb_data.get_app_title().unwrap_or("Kryon App"),
            body: &html,
            css_file: "styles.css",
            js_file: "app.js",
        })?;
        
        Ok(full_html)
    }
    
    fn generate_html_element(&mut self, element: &Element, html: &mut String, indent: usize) -> Result<(), CodeGenError> {
        let indent_str = "  ".repeat(indent);
        
        match element.element_type {
            ElementType::App => {
                // App becomes a div container
                html.push_str(&format!("{}div id=\"kryon-app\" class=\"kryon-app\">\n", indent_str));
                
                for child_id in &element.children {
                    // Look up child element and recurse
                    if let Some(child) = self.get_element(child_id) {
                        self.generate_html_element(child, html, indent + 1)?;
                    }
                }
                
                html.push_str(&format!("{}div>\n", indent_str));
            },
            
            ElementType::Container => {
                html.push_str(&format!("{}div class=\"kryon-container\"", indent_str));
                self.add_html_attributes(element, html)?;
                html.push_str(">\n");
                
                for child_id in &element.children {
                    if let Some(child) = self.get_element(child_id) {
                        self.generate_html_element(child, html, indent + 1)?;
                    }
                }
                
                html.push_str(&format!("{}div>\n", indent_str));
            },
            
            ElementType::Text => {
                let text_content = element.get_property("textContent").unwrap_or("").to_string();
                html.push_str(&format!("{}span class=\"kryon-text\"", indent_str));
                self.add_html_attributes(element, html)?;
                html.push_str(&format!(">{}</span>\n", html_escape(&text_content)));
            },
            
            ElementType::Button => {
                let button_text = element.get_property("textContent").unwrap_or("Button").to_string();
                html.push_str(&format!("{}button class=\"kryon-button\"", indent_str));
                self.add_html_attributes(element, html)?;
                html.push_str(&format!(">{}</button>\n", html_escape(&button_text)));
            },
            
            // ... other element types
            _ => return Err(CodeGenError::UnsupportedElementType(element.element_type)),
        }
        
        Ok(())
    }
    
    fn generate_css(&mut self, krb_data: &KRBFile) -> Result<String, CodeGenError> {
        let mut css = String::new();
        
        // Base Kryon styles
        css.push_str(include_str!("templates/kryon-base.css"));
        
        // Generated styles from KRB
        for style in &krb_data.styles {
            css.push_str(&format!(".kryon-style-{} {{\n", style.name));
            
            for (property, value) in &style.properties {
                let css_property = self.convert_property_to_css(property);
                let css_value = self.convert_value_to_css(value);
                css.push_str(&format!("  {}: {};\n", css_property, css_value));
            }
            
            css.push_str("}\n\n");
        }
        
        // Element-specific styles
        for element in &krb_data.elements {
            if element.has_inline_styles() {
                css.push_str(&format!("#kryon-element-{} {{\n", element.id));
                
                for (property, value) in element.get_inline_styles() {
                    let css_property = self.convert_property_to_css(&property);
                    let css_value = self.convert_value_to_css(&value);
                    css.push_str(&format!("  {}: {};\n", css_property, css_value));
                }
                
                css.push_str("}\n\n");
            }
        }
        
        // Minify if enabled
        if self.config.minify {
            css = self.minify_css(&css)?;
        }
        
        Ok(css)
    }
    
    fn generate_javascript(&mut self, krb_data: &KRBFile) -> Result<String, CodeGenError> {
        let mut js = String::new();
        
        // Kryon runtime
        js.push_str(include_str!("templates/kryon-runtime.js"));
        
        // KRB data as JavaScript
        js.push_str(&format!("const KRYON_DATA = {};\n", serde_json::to_string(krb_data)?));
        
        // Generate reactive bindings
        for binding in &krb_data.template_bindings {
            js.push_str(&format!(
                "KryonRuntime.bindTemplate('{}', '{}', '{}');\n",
                binding.element_id,
                binding.property,
                binding.expression
            ));
        }
        
        // Script integrations
        for script in &krb_data.scripts {
            match script.language {
                ScriptLanguage::JavaScript => {
                    js.push_str(&format!("// Script: {}\n", script.name));
                    js.push_str(&script.code);
                    js.push_str("\n\n");
                },
                ScriptLanguage::Lua => {
                    // Transpile Lua to JavaScript or include Lua.js
                    js.push_str("// Lua script integration\n");
                    js.push_str(&self.integrate_lua_script(&script.code)?);
                },
                _ => {
                    return Err(CodeGenError::UnsupportedScriptLanguage(script.language));
                }
            }
        }
        
        // Initialize runtime
        js.push_str("KryonRuntime.initialize(KRYON_DATA);\n");
        
        // Minify if enabled
        if self.config.minify {
            js = self.minify_javascript(&js)?;
        }
        
        Ok(js)
    }
}
```

### Desktop Target Generator

```rust
pub struct DesktopTargetGenerator {
    config: DesktopTargetConfig,
    target_platform: PlatformType,
}

pub struct DesktopTargetConfig {
    pub output_directory: String,
    pub executable_name: String,
    pub icon_path: Option<String>,
    pub bundle_runtime: bool,
    pub code_signing: Option<CodeSigningConfig>,
}

impl DesktopTargetGenerator {
    pub fn generate(&mut self, krb_data: &KRBFile) -> Result<DesktopOutput, CodeGenError> {
        match self.target_platform {
            PlatformType::DesktopWindows => self.generate_windows(krb_data),
            PlatformType::DesktopMacOS => self.generate_macos(krb_data),
            PlatformType::DesktopLinux => self.generate_linux(krb_data),
            _ => Err(CodeGenError::UnsupportedPlatform),
        }
    }
    
    fn generate_windows(&mut self, krb_data: &KRBFile) -> Result<DesktopOutput, CodeGenError> {
        // Generate native Windows application
        let mut output = DesktopOutput::new();
        
        // Create main executable wrapper
        let main_rs = self.generate_rust_main(krb_data)?;
        output.add_file("src/main.rs", main_rs);
        
        // Generate Cargo.toml
        let cargo_toml = self.generate_cargo_toml()?;
        output.add_file("Cargo.toml", cargo_toml);
        
        // Generate Windows-specific resources
        if let Some(icon) = &self.config.icon_path {
            let rc_file = self.generate_windows_rc(icon)?;
            output.add_file("resources.rc", rc_file);
        }
        
        // Bundle KRB data
        output.add_binary_file("assets/app.krb", &krb_data.to_binary()?);
        
        Ok(output)
    }
    
    fn generate_rust_main(&mut self, krb_data: &KRBFile) -> Result<String, CodeGenError> {
        let template = r#"
use kryon_runtime::{KryonRuntime, DesktopPlatform};
use std::path::Path;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Load KRB data
    let krb_data = std::fs::read("assets/app.krb")?;
    let parsed_data = kryon_runtime::parse_krb(&krb_data)?;
    
    // Create desktop platform
    let platform = DesktopPlatform::new()?;
    
    // Initialize runtime
    let mut runtime = KryonRuntime::new(parsed_data, Box::new(platform))?;
    
    // Run application
    runtime.run()?;
    
    Ok(())
}
"#;
        
        Ok(template.to_string())
    }
}
```

### Mobile Target Generator

```rust
pub struct MobileTargetGenerator {
    config: MobileTargetConfig,
    target_platform: PlatformType,
}

pub struct MobileTargetConfig {
    pub app_id: String,
    pub app_name: String,
    pub version: String,
    pub min_os_version: String,
    pub permissions: Vec<Permission>,
    pub signing_config: Option<SigningConfig>,
}

impl MobileTargetGenerator {
    pub fn generate(&mut self, krb_data: &KRBFile) -> Result<MobileOutput, CodeGenError> {
        match self.target_platform {
            PlatformType::MobileIOS => self.generate_ios(krb_data),
            PlatformType::MobileAndroid => self.generate_android(krb_data),
            _ => Err(CodeGenError::UnsupportedMobilePlatform),
        }
    }
    
    fn generate_ios(&mut self, krb_data: &KRBFile) -> Result<MobileOutput, CodeGenError> {
        let mut output = MobileOutput::new();
        
        // Generate Info.plist
        let info_plist = self.generate_info_plist()?;
        output.add_file("Info.plist", info_plist);
        
        // Generate main Swift app
        let main_swift = self.generate_swift_main(krb_data)?;
        output.add_file("KryonApp/main.swift", main_swift);
        
        // Generate Xcode project files
        let pbxproj = self.generate_xcode_project()?;
        output.add_file("KryonApp.xcodeproj/project.pbxproj", pbxproj);
        
        // Bundle KRB data as resource
        output.add_binary_file("KryonApp/app.krb", &krb_data.to_binary()?);
        
        Ok(output)
    }
    
    fn generate_android(&mut self, krb_data: &KRBFile) -> Result<MobileOutput, CodeGenError> {
        let mut output = MobileOutput::new();
        
        // Generate AndroidManifest.xml
        let manifest = self.generate_android_manifest()?;
        output.add_file("src/main/AndroidManifest.xml", manifest);
        
        // Generate MainActivity.kt
        let main_activity = self.generate_main_activity(krb_data)?;
        output.add_file("src/main/kotlin/MainActivity.kt", main_activity);
        
        // Generate build.gradle
        let build_gradle = self.generate_build_gradle()?;
        output.add_file("build.gradle", build_gradle);
        
        // Bundle KRB data as asset
        output.add_binary_file("src/main/assets/app.krb", &krb_data.to_binary()?);
        
        Ok(output)
    }
}
```

---

This comprehensive compilation pipeline specification provides the foundation for building a production-quality compiler that can transform KRY source code into optimized, platform-specific applications across all target platforms.