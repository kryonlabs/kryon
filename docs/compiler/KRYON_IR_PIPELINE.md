# Kryon Intermediate Representation (IR) Pipeline

Complete documentation of the KRY to KIR (Kryon Intermediate Representation) compilation pipeline, enabling multiple source languages to target the same KRB binary format.

> **🔗 Related Documentation**: 
> - [KRY Language Specification](../language/KRY_LANGUAGE_SPECIFICATION.md) - Source language syntax
> - [KRB Binary Format](../binary-format/KRB_BINARY_FORMAT_SPECIFICATION.md) - Target binary format
> - [Error Reference](../reference/KRYON_ERROR_REFERENCE.md) - Compilation error codes

## Table of Contents
- [Pipeline Overview](#pipeline-overview)
- [Compilation Stages](#compilation-stages)
- [Kryon IR (KIR) Format](#kryon-ir-kir-format)
- [Frontend: KRY to AST](#frontend-kry-to-ast)
- [Middle-End: AST to IR](#middle-end-ast-to-ir)
- [Backend: IR to KRB](#backend-ir-to-krb)
- [Multi-Language Support](#multi-language-support)
- [Development Tools](#development-tools)
- [Pipeline Validation](#pipeline-validation)
- [Performance Optimizations](#performance-optimizations)

## Pipeline Overview

The Kryon IR pipeline compiles source files from multiple Kryon languages through multiple stages to produce optimized KRB binary files for cross-platform execution.

```
┌─────────────────────────────────────────────────────────────┐
│                   Source Languages                          │
│  ┌─────────────────────┐    ┌─────────────────────┐         │
│  │     KRY Source      │    │     KRL Source      │         │
│  │   (CSS-like syntax) │    │   (Lisp syntax)     │         │
│  │                     │    │                     │         │
│  │ Container {         │    │ (container          │         │
│  │   Text {            │    │   (text             │         │
│  │     text: "Hello"   │    │     :text "Hello"))  │         │
│  │   }                 │    │                     │         │
│  │ }                   │    │                     │         │
│  └─────────────────────┘    └─────────────────────┘         │
└─────────────────┬─────────────────┬───────────────────────────┘
                  │                 │ Language-Specific Frontends
                  ▼                 ▼
┌─────────────────────────────────────────────────────────────┐
│              Kryon Intermediate Representation             │
│  ┌─────────────────────────────────────────────────────────┐│
│  │                    KIR JSON                             ││
│  │  • Elements        • Templates      • Scripts          ││
│  │  • Styles          • Components     • Resources        ││
│  │  • Metadata        • Dependencies   • Optimizations    ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────┬───────────────────────────────────────┘
                      │ Binary Generation
                      ▼
┌─────────────────────────────────────────────────────────────┐
│                     KRB Binary                              │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐           │
│  │    WGPU     │ │   Raylib    │ │    HTML     │  Multi-   │
│  │  (Desktop)  │ │  (Graphics) │ │   (Web)     │  Backend  │
│  └─────────────┘ └─────────────┘ └─────────────┘  Runtime  │
└─────────────────────────────────────────────────────────────┘
```

### Key Benefits

1. **Multiple Syntax Options**: Choose between KRY (CSS-like) or KRL (Lisp) syntax
2. **Unified IR**: Both languages compile to the same intermediate representation
3. **Optimized Binary**: Efficient KRB format for fast loading and execution
4. **Semantic Analysis**: Comprehensive validation during compilation
5. **Template Processing**: Reactive variable system fully analyzed at compile time
6. **Cross-Platform**: Single KRB file runs on all supported backends
7. **Developer Tools**: Rich debugging and analysis capabilities

## Compilation Stages

### Stage 1: Frontend (Source → AST)
Language-specific parsing and semantic analysis.

### Stage 2: Middle-End (AST → IR)  
Language-agnostic analysis, optimization, and transformation.

### Stage 3: Backend (IR → KRB)
Binary generation with platform-specific optimizations.

```rust
// Complete compilation pipeline
pub fn compile_with_options(
    input_path: &str,
    output_path: &str,
    options: CompilerOptions,
) -> Result<CompilationStats> {
    let start_time = std::time::Instant::now();
    
    // Stage 1: Frontend - Parse source to AST
    let mut preprocessor = Preprocessor::new();
    let module_graph = preprocessor.process_includes_isolated(input_path)?;
    
    let mut parser = Parser::new(lexer.tokenize()?);
    let ast = parser.parse()?;
    
    // Stage 2: Middle-End - AST to IR
    let ir_builder = IRBuilder::new();
    let ir = ir_builder.build_from_ast(&ast, input_path)?;
    
    // Stage 3: Backend - IR to KRB
    let krb_generator = KRBGenerator::new();
    let compiler_state = krb_generator.convert_ir_to_state(&ir)?;
    let krb_data = CodeGenerator::generate_krb(compiler_state)?;
    
    // Write output
    std::fs::write(output_path, krb_data)?;
    
    Ok(CompilationStats::from_compilation(start_time.elapsed()))
}
```

## Kryon IR (KIR) Format

The Kryon Intermediate Representation is a JSON-based format that captures all semantic information from source languages in a standardized structure.

### Core KIR Structure

```json
{
  "metadata": {
    "version": "1.0.0",
    "source_files": [/* SourceFile objects */],
    "compilation_timestamp": "2024-01-15T10:30:00Z",
    "target_platform": "cross-platform",
    "optimization_level": 2,
    "compiler_version": "0.1.0"
  },
  "components": {/* Component definitions */},
  "styles": {/* Style definitions */},
  "elements": [/* Element hierarchy */],
  "templates": {/* Template system */},
  "scripts": [/* Script definitions */],
  "imports": [/* Import declarations */]
}
```

### KIR Schema Types

```rust
// Core IR data structures
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KryonIR {
    pub metadata: IRMetadata,
    pub components: HashMap<String, IRComponent>,
    pub styles: HashMap<String, IRStyle>, 
    pub elements: Vec<IRElement>,
    pub templates: IRTemplateSystem,
    pub scripts: Vec<IRScript>,
    pub imports: Vec<IRImport>,
}

// Value types for expressions
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum Value {
    String(String),
    Int(i32),
    Float(f32),
    Bool(bool),
    Color([u8; 4]), // RGBA
    Array(Vec<Value>),
    Object(HashMap<String, Value>),
}

// Expression system
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum IRExpression {
    Literal { value: Value, type_info: ValueType },
    Variable { name: String, scope: VariableScope },
    PropertyAccess { object: Box<IRExpression>, property: String },
    BinaryOp { left: Box<IRExpression>, operator: BinaryOperator, right: Box<IRExpression> },
    TernaryOp { condition: Box<IRExpression>, true_expr: Box<IRExpression>, false_expr: Box<IRExpression> },
    FunctionCall { name: String, args: Vec<IRExpression> },
}
```

### Element Representation

Elements in KIR maintain full hierarchical structure with rich metadata:

```json
{
  "id": "main_button",
  "element_type": "Button",
  "style": "primary_button_style",
  "properties": {
    "text": {
      "value": {
        "Literal": {
          "value": { "String": "Click Me" },
          "type_info": "String"
        }
      },
      "property_id": "Text",
      "source_location": {
        "file": "app.kry",
        "line": 45,
        "column": 12,
        "span": { "start": {"line": 45, "column": 12}, "end": {"line": 45, "column": 21} }
      }
    },
    "onClick": {
      "value": {
        "FunctionCall": {
          "name": "handleButtonClick",
          "args": []
        }
      },
      "property_id": "OnClick",
      "source_location": { /* location info */ }
    }
  },
  "children": [],
  "source_location": { /* element location */ }
}
```

### Template System Representation

The IR captures the complete reactive template system:

```json
{
  "templates": {
    "variables": {
      "counter": {
        "name": "counter",
        "initial_value": { "Int": 0 },
        "type": { "Number": { "unit": null } },
        "scope": "Global",
        "source_location": { /* location */ }
      }
    },
    "bindings": [
      {
        "id": 1,
        "element_path": "elements[2]",
        "property": "text",
        "expression": {
          "BinaryOp": {
            "left": { "Literal": { "value": { "String": "Count: " } } },
            "operator": "Concat",
            "right": { "Variable": { "name": "counter", "scope": "Global" } }
          }
        },
        "update_strategy": "Reactive"
      }
    ],
    "expressions": { /* Compiled expressions */ }
  }
}
```

## Frontend: KRY to AST

The frontend transforms KRY source code into an Abstract Syntax Tree through lexical analysis and parsing.

### Lexical Analysis

```rust
pub struct Lexer {
    input: Vec<char>,
    position: usize,
    line: usize,
    column: usize,
}

// Token types for KRY language
#[derive(Debug, Clone, PartialEq)]
pub enum TokenType {
    // Identifiers and literals
    Identifier(String),
    StringLiteral(String),
    NumberLiteral(f64),
    BooleanLiteral(bool),
    
    // Element types
    App, Container, Text, Button, Input,
    
    // Directives
    Variables, Include, Script, Function,
    Styles, Component, For, If, Const,
    
    // Operators
    Colon, Semicolon, Comma, Dot,
    LeftBrace, RightBrace, LeftParen, RightParen,
    Plus, Minus, Star, Slash, Percent,
    Equal, NotEqual, Less, Greater,
    
    // Special
    Newline, Comment(String), Eof,
}
```

### Parsing Process

```rust
impl Parser {
    pub fn parse(&mut self) -> Result<AstNode> {
        let mut directives = Vec::new();
        let mut styles = Vec::new();
        let mut components = Vec::new();
        let mut scripts = Vec::new();
        let mut app = None;
        let mut standalone_elements = Vec::new();
        
        // Parse top-level constructs
        while !self.is_at_end() {
            match self.peek().token_type {
                TokenType::Include => directives.push(self.parse_include()?),
                TokenType::Variables => directives.push(self.parse_variables()?),
                TokenType::Script => scripts.push(self.parse_script()?),
                TokenType::App => app = Some(Box::new(self.parse_element()?)),
                _ => standalone_elements.push(self.parse_element()?),
            }
        }
        
        // Handle auto-wrapping for standalone elements
        if app.is_none() && !standalone_elements.is_empty() {
            app = Some(Box::new(self.create_auto_wrapper(standalone_elements)?));
        }
        
        Ok(AstNode::File {
            directives, themes: Vec::new(), styles, fonts: Vec::new(),
            components, scripts, app
        })
    }
}
```

### AST Node Types

```rust
#[derive(Debug, Clone)]
pub enum AstNode {
    File {
        directives: Vec<AstDirective>,
        themes: Vec<AstTheme>,
        styles: Vec<AstStyle>,
        fonts: Vec<AstFont>,
        components: Vec<AstComponent>,
        scripts: Vec<AstScript>,
        app: Option<Box<AstNode>>,
    },
    Element {
        element_type: ElementType,
        id: Option<String>,
        style: Option<String>,
        properties: Vec<AstProperty>,
        children: Vec<AstNode>,
        source_location: SourceLocation,
    },
    For {
        variable: String,
        iterable: Expression,
        body: Vec<AstNode>,
        source_location: SourceLocation,
    },
    If {
        condition: Expression,
        then_body: Vec<AstNode>,
        else_body: Vec<AstNode>,
        source_location: SourceLocation,
    },
}
```

## Middle-End: AST to IR

The middle-end processes the AST to generate the intermediate representation, performing semantic analysis and optimization.

### IR Builder Process

```rust
pub struct IRBuilder {
    ir: KryonIR,
    current_component: Option<String>,
    source_map: SourceMap,
    type_checker: TypeChecker,
    expression_cache: HashMap<String, ExpressionId>,
    current_file: String,
    theme_registry: ThemeRegistry,
}

impl IRBuilder {
    pub fn build_from_ast(mut self, ast: &AstNode, source_file: &str) -> Result<KryonIR> {
        self.current_file = source_file.to_string();
        
        // Add source file metadata
        self.ir.metadata.source_files.push(SourceFile {
            path: Path::new(source_file).to_path_buf(),
            content_hash: self.compute_content_hash(source_file)?,
            last_modified: std::fs::metadata(source_file)?.modified()?,
        });
        
        // Process the AST
        self.process_ast_node(ast)?;
        
        // Build dependency graph
        self.build_dependency_graph()?;
        
        // Perform optimizations
        self.optimize_ir()?;
        
        Ok(self.ir)
    }
}
```

### Semantic Analysis

The IR builder performs comprehensive semantic analysis:

```rust
impl IRBuilder {
    fn process_element(&mut self, element: &AstNode) -> Result<IRElement> {
        match element {
            AstNode::Element { element_type, id, style, properties, children, source_location } => {
                // Validate element type
                self.validate_element_type(element_type)?;
                
                // Process properties
                let mut ir_properties = HashMap::new();
                for property in properties {
                    let ir_property = self.process_property(property, element_type)?;
                    ir_properties.insert(property.name.clone(), ir_property);
                }
                
                // Process children
                let mut ir_children = Vec::new();
                for child in children {
                    ir_children.push(self.process_element(child)?);
                }
                
                // Validate element-property relationships
                self.validate_element_properties(element_type, &ir_properties)?;
                
                Ok(IRElement {
                    id: id.clone(),
                    element_type: *element_type,
                    style: style.clone(),
                    properties: ir_properties,
                    children: ir_children,
                    source_location: source_location.clone(),
                })
            }
            _ => Err(CompilerError::SemanticError {
                message: "Expected element node".to_string(),
                location: None,
            })
        }
    }
}
```

### Expression Processing

Expressions are converted to IR with full type information:

```rust
impl IRBuilder {
    fn process_expression(&mut self, expr: &Expression) -> Result<IRExpression> {
        match expr {
            Expression::Literal { value, source_location } => {
                Ok(IRExpression::Literal {
                    value: self.convert_value(value)?,
                    type_info: self.infer_type(value)?,
                })
            }
            Expression::Variable { name, source_location } => {
                self.validate_variable_exists(name)?;
                Ok(IRExpression::Variable {
                    name: name.clone(),
                    scope: self.determine_variable_scope(name)?,
                })
            }
            Expression::BinaryOp { left, operator, right, source_location } => {
                let left_ir = self.process_expression(left)?;
                let right_ir = self.process_expression(right)?;
                
                // Type checking
                self.validate_binary_operation(&left_ir, operator, &right_ir)?;
                
                Ok(IRExpression::BinaryOp {
                    left: Box::new(left_ir),
                    operator: *operator,
                    right: Box::new(right_ir),
                })
            }
            Expression::Ternary { condition, true_expr, false_expr, source_location } => {
                let condition_ir = self.process_expression(condition)?;
                let true_ir = self.process_expression(true_expr)?;
                let false_ir = self.process_expression(false_expr)?;
                
                // Validate condition is boolean
                self.validate_boolean_expression(&condition_ir)?;
                
                // Validate branches return compatible types
                self.validate_compatible_types(&true_ir, &false_ir)?;
                
                Ok(IRExpression::TernaryOp {
                    condition: Box::new(condition_ir),
                    true_expr: Box::new(true_ir),
                    false_expr: Box::new(false_ir),
                })
            }
        }
    }
}
```

### Template System Processing

Reactive templates are analyzed and converted to IR:

```rust
impl IRBuilder {
    fn process_template_system(&mut self, elements: &[IRElement]) -> Result<IRTemplateSystem> {
        let mut template_system = IRTemplateSystem::new();
        
        // Extract variables
        for variable in &self.parsed_variables {
            template_system.variables.insert(
                variable.name.clone(),
                IRTemplateVariable {
                    name: variable.name.clone(),
                    initial_value: self.convert_value(&variable.initial_value)?,
                    var_type: self.infer_type(&variable.initial_value)?,
                    scope: VariableScope::Global,
                    source_location: variable.source_location.clone(),
                }
            );
        }
        
        // Find template bindings
        let mut binding_id = 0;
        for (element_index, element) in elements.iter().enumerate() {
            self.extract_bindings_from_element(
                element, 
                &format!("elements[{}]", element_index),
                &mut template_system.bindings,
                &mut binding_id
            )?;
        }
        
        // Compile expressions
        for binding in &template_system.bindings {
            let compiled_expr = self.compile_expression(&binding.expression)?;
            template_system.expressions.insert(binding.id, compiled_expr);
        }
        
        Ok(template_system)
    }
}
```

## Backend: IR to KRB

The backend converts the IR to the final KRB binary format.

### KRB Generation Process

```rust
pub struct KRBGenerator {
    compiler_state: CompilerState,
    string_interner: StringInterner,
    style_cache: HashMap<String, u32>,
}

impl KRBGenerator {
    pub fn convert_ir_to_state(&mut self, ir: &KryonIR) -> Result<CompilerState> {
        // Initialize compiler state
        self.compiler_state.header.version = "1.0".to_string();
        self.compiler_state.header.flags = 0;
        
        // Process styles
        for (style_name, ir_style) in &ir.styles {
            let style_id = self.convert_style(ir_style)?;
            self.style_cache.insert(style_name.clone(), style_id);
        }
        
        // Process elements
        for ir_element in &ir.elements {
            let element = self.convert_element(ir_element)?;
            self.compiler_state.elements.push(element);
        }
        
        // Process templates
        self.convert_template_system(&ir.templates)?;
        
        // Process scripts
        for ir_script in &ir.scripts {
            self.convert_script(ir_script)?;
        }
        
        Ok(self.compiler_state)
    }
}
```

### Element Conversion

```rust
impl KRBGenerator {
    fn convert_element(&mut self, ir_element: &IRElement) -> Result<Element> {
        let mut element = Element::new(ir_element.element_type);
        
        // Set basic properties
        element.id = ir_element.id.clone().unwrap_or_default();
        
        // Apply style if specified
        if let Some(style_name) = &ir_element.style {
            element.style_id = *self.style_cache.get(style_name)
                .ok_or_else(|| CompilerError::UnknownStyle { 
                    name: style_name.clone() 
                })?;
        }
        
        // Convert properties
        for (property_name, ir_property) in &ir_element.properties {
            self.apply_property_to_element(&mut element, ir_property)?;
        }
        
        // Process children
        for ir_child in &ir_element.children {
            let child_element = self.convert_element(ir_child)?;
            element.children.push(child_element.id);
            self.compiler_state.elements.push(child_element);
        }
        
        Ok(element)
    }
}
```

### Binary Serialization

```rust
pub struct CodeGenerator;

impl CodeGenerator {
    pub fn generate_krb(state: CompilerState) -> Result<Vec<u8>> {
        let mut writer = BinaryWriter::new();
        
        // Write header
        writer.write_header(&state.header)?;
        
        // Write string table
        writer.write_string_table(&state.strings)?;
        
        // Write styles
        writer.write_styles(&state.styles)?;
        
        // Write elements
        writer.write_elements(&state.elements)?;
        
        // Write template data
        writer.write_template_data(&state.template_data)?;
        
        // Write scripts
        writer.write_scripts(&state.scripts)?;
        
        Ok(writer.finalize())
    }
}
```

## Component System

The IR pipeline supports Kryon's component system with full encapsulation and reusability.

### Component Processing

```rust
impl IRBuilder {
    fn process_component(&mut self, component: &AstComponent) -> Result<IRComponent> {
        let mut ir_component = IRComponent {
            name: component.name.clone(),
            properties: HashMap::new(),
            elements: Vec::new(),
            methods: Vec::new(),
            source_location: component.source_location.clone(),
        };
        
        // Process component properties
        for property in &component.properties {
            let ir_property = self.process_component_property(property)?;
            ir_component.properties.insert(property.name.clone(), ir_property);
        }
        
        // Process component body elements
        for element in &component.elements {
            ir_component.elements.push(self.process_element(element)?);
        }
        
        // Process component methods
        for method in &component.methods {
            ir_component.methods.push(self.process_component_method(method)?);
        }
        
        Ok(ir_component)
    }
}
```

### Component Usage in IR

```json
{
  "components": {
    "UserCard": {
      "name": "UserCard",
      "properties": {
        "username": {
          "name": "username",
          "type": "String",
          "required": true,
          "default_value": null
        },
        "avatar": {
          "name": "avatar", 
          "type": "String",
          "required": false,
          "default_value": { "String": "default-avatar.png" }
        }
      },
      "elements": [
        {
          "element_type": "Container",
          "properties": {
            "display": { "value": { "Literal": { "value": { "String": "flex" } } } }
          },
          "children": [
            {
              "element_type": "Image",
              "properties": {
                "src": { 
                  "value": { 
                    "Variable": { "name": "avatar", "scope": "Component" } 
                  } 
                }
              }
            },
            {
              "element_type": "Text",
              "properties": {
                "text": { 
                  "value": { 
                    "Variable": { "name": "username", "scope": "Component" } 
                  } 
                }
              }
            }
          ]
        }
      ]
    }
  }
}
```

## Development Tools

### KIR Inspector

```bash
# Inspect generated IR
kryc inspect app.kry --format json --output app.kir.json

# Validate IR structure
kryc validate app.kir.json

# Compare IR between versions
kryc diff app.v1.kir.json app.v2.kir.json
```

### Pipeline Debugger

```bash
# Debug compilation pipeline
kryc debug app.kry --stage all --verbose

# Debug specific stage
kryc debug app.kry --stage ir-generation --output debug.log

# Visual pipeline inspection
kryc visualize app.kir.json --output pipeline.svg
```

### IR Optimization Tools

```bash
# Optimize IR for size
kryc optimize app.kir.json --target size --output app.optimized.kir.json

# Optimize IR for runtime performance  
kryc optimize app.kir.json --target speed --output app.fast.kir.json

# Dead code elimination
kryc optimize app.kir.json --eliminate-dead-code --output app.clean.kir.json
```

## Pipeline Validation

### Automated Testing

```bash
# Run full pipeline tests
cargo test --manifest-path kryon-compiler/Cargo.toml pipeline

# Test specific stages
cargo test --manifest-path kryon-compiler/Cargo.toml frontend
cargo test --manifest-path kryon-compiler/Cargo.toml ir_generation
cargo test --manifest-path kryon-compiler/Cargo.toml krb_generation

# End-to-end validation
cargo test --manifest-path kryon-compiler/Cargo.toml e2e
```

### Pipeline Integrity Checks

```rust
// Validate IR completeness
pub fn validate_ir_completeness(ir: &KryonIR) -> Result<()> {
    // Check all referenced styles exist
    for element in &ir.elements {
        if let Some(style_name) = &element.style {
            if !ir.styles.contains_key(style_name) {
                return Err(CompilerError::MissingStyle { 
                    name: style_name.clone() 
                });
            }
        }
    }
    
    // Check all template variables are defined
    for binding in &ir.templates.bindings {
        validate_expression_variables(&binding.expression, &ir.templates.variables)?;
    }
    
    // Check script dependencies
    for script in &ir.scripts {
        validate_script_dependencies(script, &ir.templates.variables)?;
    }
    
    Ok(())
}
```

### Binary Format Validation

```rust
// Validate generated KRB matches IR
pub fn validate_krb_ir_consistency(ir: &KryonIR, krb_data: &[u8]) -> Result<()> {
    let parsed_krb = kryon_core::parse_krb_file(krb_data)?;
    
    // Check element count matches
    if ir.elements.len() != parsed_krb.elements.len() {
        return Err(CompilerError::BinaryMismatch {
            expected: ir.elements.len(),
            actual: parsed_krb.elements.len(),
        });
    }
    
    // Check string table consistency
    let ir_strings = collect_ir_strings(ir);
    let krb_strings = parsed_krb.strings;
    
    for ir_string in ir_strings {
        if !krb_strings.contains(&ir_string) {
            return Err(CompilerError::MissingString { 
                string: ir_string 
            });
        }
    }
    
    Ok(())
}
```

## Performance Optimizations

### IR-Level Optimizations

```rust
pub struct IROptimizer {
    config: OptimizationConfig,
}

impl IROptimizer {
    pub fn optimize(&mut self, ir: &mut KryonIR) -> Result<OptimizationStats> {
        let mut stats = OptimizationStats::new();
        
        // Dead code elimination
        stats.merge(self.eliminate_dead_code(ir)?);
        
        // Constant folding
        stats.merge(self.fold_constants(ir)?);
        
        // Expression deduplication
        stats.merge(self.deduplicate_expressions(ir)?);
        
        // Style merging
        stats.merge(self.merge_similar_styles(ir)?);
        
        // Template optimization
        stats.merge(self.optimize_templates(ir)?);
        
        Ok(stats)
    }
    
    fn eliminate_dead_code(&mut self, ir: &mut KryonIR) -> Result<OptimizationStats> {
        let mut removed_elements = 0;
        let mut removed_styles = 0;
        
        // Remove unused styles
        let used_styles = self.collect_used_styles(&ir.elements);
        ir.styles.retain(|name, _| {
            let used = used_styles.contains(name);
            if !used { removed_styles += 1; }
            used
        });
        
        // Remove unreachable elements
        let reachable_elements = self.find_reachable_elements(&ir.elements);
        ir.elements.retain(|element| {
            let reachable = reachable_elements.contains(&element.id);
            if !reachable { removed_elements += 1; }
            reachable
        });
        
        Ok(OptimizationStats {
            removed_elements,
            removed_styles,
            ..Default::default()
        })
    }
}
```

### Binary Generation Optimizations

```rust
impl CodeGenerator {
    fn optimize_string_table(&mut self, strings: &mut Vec<String>) {
        // Sort by frequency for better compression
        strings.sort_by(|a, b| {
            self.string_frequency.get(b).unwrap_or(&0)
                .cmp(self.string_frequency.get(a).unwrap_or(&0))
        });
        
        // Remove duplicates
        strings.dedup();
        
        // Apply string interning
        self.apply_string_interning(strings);
    }
    
    fn compress_property_data(&mut self, properties: &mut Vec<Property>) {
        // Group similar properties
        properties.sort_by_key(|p| p.property_id);
        
        // Use delta encoding for numeric values
        self.apply_delta_encoding(properties);
        
        // Compress with LZ compression
        if self.config.enable_compression {
            self.apply_lz_compression(properties);
        }
    }
}
```

### Memory Usage Optimization

```rust
pub struct CompilerConfig {
    pub max_memory_usage: usize,
    pub enable_streaming: bool,
    pub chunk_size: usize,
    pub gc_frequency: u32,
}

pub struct MemoryEfficientCompiler {
    config: CompilerConfig,
    memory_tracker: MemoryTracker,
}

impl MemoryEfficientCompiler {
    pub fn compile_large_file(&mut self, input_path: &str) -> Result<Vec<u8>> {
        if self.config.enable_streaming {
            self.compile_streaming(input_path)
        } else {
            self.compile_standard(input_path)
        }
    }
    
    fn compile_streaming(&mut self, input_path: &str) -> Result<Vec<u8>> {
        let mut output = Vec::new();
        let file_reader = StreamingFileReader::new(input_path, self.config.chunk_size)?;
        
        for chunk in file_reader {
            // Process chunk
            let chunk_ir = self.process_chunk(chunk?)?;
            
            // Convert to KRB
            let chunk_krb = self.generate_krb_chunk(chunk_ir)?;
            output.extend(chunk_krb);
            
            // Garbage collect if needed
            if self.memory_tracker.should_gc() {
                self.perform_gc();
            }
        }
        
        Ok(output)
    }
}
```

This comprehensive IR pipeline documentation enables multiple source languages to target the same KRB runtime while maintaining consistency, performance, and extensibility across the entire Kryon ecosystem.