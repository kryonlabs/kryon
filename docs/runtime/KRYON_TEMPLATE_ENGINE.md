# Kryon Template Engine & Reactive Variables

Complete documentation of Kryon's template engine, including reactive variable system, expression evaluation, and automatic UI updates.

> **🔗 Related Documentation**: 
> - [KRYON_SCRIPT_SYSTEM.md](KRYON_SCRIPT_SYSTEM.md) - Script integration with template variables
> - [KRYON_RUNTIME_SYSTEM.md](KRYON_RUNTIME_SYSTEM.md) - Runtime system architecture

## Table of Contents
- [Template Engine Overview](#template-engine-overview)
- [Reactive Variables](#reactive-variables)
- [Expression Language](#expression-language)
- [Template Bindings](#template-bindings)
- [Context Variables](#context-variables)
- [Expression Evaluation](#expression-evaluation)
- [Performance Optimization](#performance-optimization)
- [Advanced Features](#advanced-features)

## Template Engine Overview

Kryon's Template Engine provides reactive programming capabilities through template variables and automatic UI updates. When template variables change, all bound UI elements update automatically.

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    TemplateEngine                           │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐│
│  │    Variables    │ │    Bindings     │ │   Expression    ││
│  │  (State Store)  │ │ (Element Links) │ │   Evaluator     ││
│  └─────────────────┘ └─────────────────┘ └─────────────────┘│
└─────────────────────────────────────────────────────────────┘
            │                    │                    │
            ▼                    ▼                    ▼
    ┌───────────────┐    ┌───────────────┐    ┌───────────────┐
    │   Variable    │    │    Element    │    │  Expression   │
    │     Store     │    │   Property    │    │    Parser     │
    │              │    │   Updates     │    │              │
    └───────────────┘    └───────────────┘    └───────────────┘
```

### Core Components

```rust
pub struct TemplateEngine {
    /// Template variables with their current values
    variables: HashMap<String, String>,
    /// Template bindings from KRB file
    bindings: Vec<TemplateBinding>,
    /// Template variables from KRB file
    template_variables: Vec<TemplateVariable>,
    /// Compiled regex for template variable extraction
    template_regex: Regex,
}

pub struct TemplateBinding {
    pub element_index: u32,
    pub property_id: u8,
    pub template_expression: String,
}
```

## Reactive Variables

### Variable Declaration

Variables are declared in KRY source and compiled into the KRB format:

```kry
App {
    @variables {
        counter: 0
        message: "Hello World"
        isVisible: true
        username: ""
        theme: "light"
    }
    
    Container {
        Text {
            text: "Count: " + $counter
            visible: $isVisible
        }
        
        Button {
            text: $counter > 10 ? "Reset" : "Increment"
            onClick: "handleClick()"
        }
    }
}
```

### Variable Types

Kryon supports multiple variable types with automatic conversion:

| Type | Example | Description | Default Conversion |
|------|---------|-------------|-------------------|
| **String** | `"hello"` | Text values | Direct string |
| **Number** | `42`, `3.14` | Numeric values | `.to_string()` |
| **Boolean** | `true`, `false` | Boolean values | `"true"/"false"` |
| **Null** | `null` | Empty/undefined | `""` (empty string) |

### Variable Access Patterns

```kry
Container {
    # Simple variable reference
    Text { text: $message }
    
    # Variable in expression
    Text { text: "Hello " + $username }
    
    # Variable comparison
    Button { 
        text: $counter > 0 ? "Reset" : "Start"
        visible: $isLoggedIn
    }
    
    # Multiple variables
    Text { text: $firstName + " " + $lastName }
}
```

## Expression Language

Kryon's template expression language supports complex expressions with JavaScript-like syntax.

### Expression AST

```rust
#[derive(Debug, Clone, PartialEq)]
enum Expression {
    // Literals
    String(String),
    Number(f64),
    Boolean(bool),
    Variable(String),
    
    // Comparison Operators
    Equals(Box<Expression>, Box<Expression>),
    NotEquals(Box<Expression>, Box<Expression>),
    LessThan(Box<Expression>, Box<Expression>),
    LessThanOrEqual(Box<Expression>, Box<Expression>),
    GreaterThan(Box<Expression>, Box<Expression>),
    GreaterThanOrEqual(Box<Expression>, Box<Expression>),
    
    // Ternary Conditional
    Ternary {
        condition: Box<Expression>,
        true_value: Box<Expression>,
        false_value: Box<Expression>,
    },
}
```

### Supported Operations

#### Comparison Operators

```kry
Container {
    # Equality
    Text { visible: $status == "active" }
    Text { visible: $count != 0 }
    
    # Numeric comparison
    Button { visible: $score > 100 }
    Button { visible: $lives >= 1 }
    ProgressBar { visible: $progress < 100 }
    Button { visible: $level <= 5 }
}
```

#### Ternary Conditional

```kry
Container {
    # Simple ternary
    Text { text: $isLoggedIn ? "Welcome!" : "Please log in" }
    
    # Nested ternary
    Text { 
        text: $score > 90 ? "Excellent" : $score > 70 ? "Good" : "Try again"
    }
    
    # Complex conditions
    Button {
        text: $lives > 0 && $gameStarted ? "Continue" : "New Game"
        backgroundColor: $lives > 3 ? "#00FF00" : $lives > 1 ? "#FFFF00" : "#FF0000"
    }
}
```

#### String Concatenation

```kry
Container {
    # String concatenation with +
    Text { text: "Hello " + $firstName + " " + $lastName }
    
    # Mixed types
    Text { text: "Score: " + $currentScore + "/" + $maxScore }
    
    # Complex expressions
    Text { 
        text: $isWinner ? "You won with " + $score + " points!" : "Try again"
    }
}
```

### Expression Parser

The template engine includes a full expression parser:

```rust
struct ExpressionParser {
    tokens: Vec<Token>,
    current: usize,
}

#[derive(Debug, Clone, PartialEq)]
enum Token {
    String(String),
    Number(f64),
    Boolean(bool),
    Variable(String),
    Question,      // ?
    Colon,         // :
    Equals,        // ==
    NotEquals,     // !=
    LessThan,      // <
    GreaterThan,   // >
    // ... more operators
}

impl ExpressionParser {
    fn parse_ternary(&mut self) -> Result<Expression, String> {
        let mut expr = self.parse_comparison()?;
        
        if self.tokens[self.current] == Token::Question {
            self.current += 1; // consume ?
            let true_expr = self.parse_comparison()?;
            
            if self.tokens[self.current] == Token::Colon {
                self.current += 1; // consume :
                let false_expr = self.parse_ternary()?; // Right-associative
                
                expr = Expression::Ternary {
                    condition: Box::new(expr),
                    true_value: Box::new(true_expr),
                    false_value: Box::new(false_expr),
                };
            }
        }
        
        Ok(expr)
    }
}
```

## Template Bindings

### Binding Creation

Template bindings are created during KRY compilation and stored in the KRB format:

```rust
pub struct TemplateBinding {
    pub element_index: u32,        // Which element
    pub property_id: u8,           // Which property (hex code)
    pub template_expression: String, // Expression to evaluate
}
```

### Property Binding Types

| Property ID | Property Name | Description | Example |
|-------------|---------------|-------------|---------|
| `0x08` | textContent | Element text | `"Count: " + $counter` |
| `0x10` | visibility | Show/hide element | `$isVisible` |
| `0x1D` | styleId | Dynamic styling | `$theme == "dark" ? "dark_style" : "light_style"` |

### Binding Resolution Process

```rust
impl TemplateEngine {
    pub fn update_elements(&self, elements: &mut HashMap<ElementId, Element>, krb_file: &KRBFile) {
        for binding in &self.bindings {
            // 1. Get element reference
            if let Some(element) = elements.get(&(binding.element_index as u32)) {
                // 2. Evaluate expression with context
                let evaluated_value = self.evaluate_expression_with_context(
                    &binding.template_expression, 
                    element, 
                    elements
                );
                
                // 3. Apply value to element property
                if let Some(element) = elements.get_mut(&(binding.element_index as u32)) {
                    match binding.property_id {
                        0x08 => element.text = evaluated_value,
                        0x10 => element.visible = self.evaluate_truthiness(&evaluated_value),
                        0x1D => {
                            // Find style by name and set style_id
                            element.style_id = self.find_style_by_name(&evaluated_value, krb_file);
                        }
                        _ => {}
                    }
                }
            }
        }
    }
}
```

## Context Variables

Kryon supports special context variables for accessing parent and root element properties.

### Parent Context

```kry
Container {
    width: 400px
    height: 300px
    
    Text {
        # Access parent properties
        text: "Parent width: " + $parent.width
        x: $parent.width / 2
        y: $parent.height / 2
    }
    
    Button {
        # Position relative to parent
        x: $parent.width - 100
        y: $parent.height - 50
        visible: $parent.visible
    }
}
```

### Root Context

```kry
App {
    windowWidth: 1200
    windowHeight: 800
    theme: "dark"
    
    Container {
        Container {
            Container {
                Text {
                    # Access root App properties from deep nesting
                    text: "Window: " + $root.windowWidth + "x" + $root.windowHeight
                    visible: $root.theme == "dark"
                }
            }
        }
    }
}
```

### Context Resolution Implementation

```rust
impl TemplateEngine {
    fn resolve_simple_variable_with_context(
        &self, 
        expression: &str, 
        current_element: &Element, 
        elements: &HashMap<ElementId, Element>
    ) -> String {
        let var_name = &expression[1..]; // Remove the $
        
        // Check for parent context
        if var_name.starts_with("parent.") {
            if let Some(parent_id) = current_element.parent {
                if let Some(parent_element) = elements.get(&parent_id) {
                    let property = &var_name[7..]; // Remove "parent."
                    return self.get_element_property_value(parent_element, property);
                }
            }
            return "0".to_string(); // Default if no parent
        }
        
        // Check for root context
        if var_name.starts_with("root.") {
            if let Some((_, root_element)) = elements.iter().find(|(_, element)| {
                element.element_type == ElementType::App && element.parent.is_none()
            }) {
                let property = &var_name[5..]; // Remove "root."
                return self.get_element_property_value(root_element, property);
            }
            return "0".to_string(); // Default if no root
        }
        
        // Regular variable lookup
        self.variables.get(var_name).cloned().unwrap_or_default()
    }
    
    fn get_element_property_value(&self, element: &Element, property: &str) -> String {
        match property {
            // Position properties
            "pos_x" | "x" => element.layout_position.x.to_pixels(800.0).to_string(),
            "pos_y" | "y" => element.layout_position.y.to_pixels(600.0).to_string(),
            
            // Size properties
            "width" => element.layout_size.width.to_pixels(800.0).to_string(),
            "height" => element.layout_size.height.to_pixels(600.0).to_string(),
            
            // Visual properties
            "background_color" => format!("#{:02X}{:02X}{:02X}", 
                (element.background_color.x * 255.0) as u8,
                (element.background_color.y * 255.0) as u8,
                (element.background_color.z * 255.0) as u8),
            
            // Text properties
            "text" => element.text.clone(),
            "font_size" => element.font_size.to_string(),
            
            // State properties
            "visible" => element.visible.to_string(),
            "id" => element.id.clone(),
            
            // Custom properties
            _ => {
                element.custom_properties.get(property)
                    .map(|prop| prop.to_string())
                    .unwrap_or_else(|| "0".to_string())
            }
        }
    }
}
```

## Expression Evaluation

### Truthiness Evaluation

Kryon uses JavaScript-like truthiness rules:

```rust
impl TemplateEngine {
    fn evaluate_truthiness(&self, value: &str) -> bool {
        match value {
            // Explicit boolean strings
            "true" => true,
            "false" => false,
            // Empty string is falsy
            "" => false,
            // Zero values are falsy
            "0" | "0.0" => false,
            // Null/undefined-like values are falsy
            "null" | "undefined" | "NaN" => false,
            // Try to parse as number
            _ => {
                if let Ok(num) = value.parse::<f64>() {
                    num != 0.0 && !num.is_nan()
                } else {
                    // Non-empty strings are truthy
                    true
                }
            }
        }
    }
}
```

### Expression Evaluation Examples

```kry
Container {
    # Truthiness examples
    Text { visible: $counter }           # 0 = false, anything else = true
    Text { visible: $message }           # "" = false, any text = true
    Text { visible: $isActive }          # "true"/"false" strings
    
    # Numeric comparisons
    Button { visible: $score > 0 }       # Numeric comparison
    Button { visible: $lives >= 1 }      # Greater than or equal
    ProgressBar { value: $progress < 100 ? $progress : 100 }
    
    # String comparisons
    Text { visible: $status == "ready" } # String equality
    Text { visible: $mode != "loading" } # String inequality
    
    # Complex expressions
    Button { 
        text: $gameState == "playing" ? 
              ($lives > 1 ? "Continue" : "Last Life!") : 
              "Start Game"
    }
}
```

## Performance Optimization

### Change Detection

The template engine uses efficient change detection to minimize updates:

```rust
impl TemplateEngine {
    pub fn set_variable(&mut self, name: &str, value: &str) -> bool {
        if let Some(var_value) = self.variables.get_mut(name) {
            if *var_value != value {
                *var_value = value.to_string();
                return true; // Variable changed
            }
        }
        false // Variable not found or unchanged
    }
    
    pub fn update_variable_and_get_affected_elements(&mut self, name: &str, value: &str) -> Vec<u32> {
        let mut affected_elements = Vec::new();
        
        if self.set_variable(name, value) {
            // Variable changed, find all affected elements
            let bindings = self.get_bindings_for_variable(name);
            for binding in bindings {
                affected_elements.push(binding.element_index as u32);
            }
        }
        
        affected_elements
    }
}
```

### Binding Optimization

```rust
impl TemplateEngine {
    pub fn get_bindings_for_variable(&self, variable_name: &str) -> Vec<&TemplateBinding> {
        // Use regex to find bindings that reference this variable
        self.bindings.iter()
            .filter(|binding| {
                self.template_regex.captures_iter(&binding.template_expression)
                    .any(|capture| {
                        capture.get(1).map_or(false, |m| m.as_str() == variable_name)
                    })
            })
            .collect()
    }
}
```

### Memory Optimization

```rust
// Efficient template regex compilation (done once)
let template_regex = Regex::new(r"\$([a-zA-Z_][a-zA-Z0-9_]*)").unwrap();

// Avoid duplicate binding processing
let mut processed_bindings = std::collections::HashSet::new();
for binding in &self.bindings {
    let binding_key = (binding.element_index, binding.property_id);
    if processed_bindings.contains(&binding_key) {
        continue; // Skip duplicate
    }
    processed_bindings.insert(binding_key);
    // ... process binding
}
```

## Advanced Features

### Dynamic Style Changes

```kry
App {
    @variables {
        theme: "light"
        userLevel: 1
    }
    
    @styles {
        "light_theme" {
            backgroundColor: "#FFFFFF"
            textColor: "#000000"
        }
        
        "dark_theme" {
            backgroundColor: "#000000"
            textColor: "#FFFFFF"
        }
        
        "beginner_style" {
            fontSize: 14
            fontWeight: 400
        }
        
        "expert_style" {
            fontSize: 12
            fontWeight: 700
        }
    }
    
    Container {
        # Dynamic style selection
        styleId: $theme == "dark" ? "dark_theme" : "light_theme"
        
        Text {
            text: "Welcome!"
            styleId: $userLevel > 10 ? "expert_style" : "beginner_style"
        }
    }
}
```

### Computed Properties

```lua
-- Lua script integration with template variables
function updateComputedProperties()
    local firstName = kryon.getVariable("firstName") or ""
    local lastName = kryon.getVariable("lastName") or ""
    
    -- Computed full name
    kryon.setVariable("fullName", firstName .. " " .. lastName)
    
    -- Computed status
    local score = tonumber(kryon.getVariable("score")) or 0
    local status = score > 90 and "Excellent" or score > 70 and "Good" or "Needs Improvement"
    kryon.setVariable("status", status)
    
    -- Complex computation
    local level = math.floor(score / 10) + 1
    kryon.setVariable("level", tostring(level))
end
```

### Conditional Rendering

```kry
Container {
    # Show/hide entire sections
    Container {
        visible: $isLoggedIn
        
        Text { text: "Welcome, " + $username }
        Button { text: "Logout" }
    }
    
    Container {
        visible: !$isLoggedIn
        
        Text { text: "Please log in" }
        Button { text: "Login" }
    }
    
    # Conditional content
    Text {
        text: $gameState == "playing" ? 
              "Lives: " + $lives : 
              $gameState == "won" ? 
              "Congratulations!" : 
              "Game Over"
    }
}
```

### Animation Integration

```kry
Container {
    # Animated properties based on state
    Button {
        backgroundColor: $isHovered ? "#FF6600" : "#FF9900"
        scale: $isPressed ? 0.95 : 1.0
        transition: "all 0.2s ease"
    }
    
    # Progress animation
    ProgressBar {
        value: $loadingProgress
        visible: $isLoading
        animationDuration: 300
    }
}
```

### Error Handling

```rust
impl TemplateEngine {
    pub fn evaluate_expression_safe(&self, expression: &str) -> Result<String, String> {
        match self.parse_expression(expression) {
            Ok(expr) => Ok(self.evaluate_expr(&expr)),
            Err(parse_error) => {
                tracing::warn!("Template expression parse error: {} in '{}'", parse_error, expression);
                
                // Fallback to simple variable substitution
                let mut result = expression.to_string();
                for capture in self.template_regex.captures_iter(expression) {
                    if let Some(var_name) = capture.get(1) {
                        let var_name_str = var_name.as_str();
                        if let Some(value) = self.variables.get(var_name_str) {
                            let pattern = format!("${}", var_name_str);
                            result = result.replace(&pattern, value);
                        }
                    }
                }
                
                Ok(result)
            }
        }
    }
}
```

### Development Tools

```rust
impl TemplateEngine {
    pub fn debug_variables(&self) {
        tracing::debug!("Template Variables:");
        for (name, value) in &self.variables {
            tracing::debug!("  ${} = '{}'", name, value);
        }
    }
    
    pub fn debug_bindings(&self) {
        tracing::debug!("Template Bindings:");
        for binding in &self.bindings {
            tracing::debug!("  Element {}, Property 0x{:02X}: '{}'", 
                binding.element_index, 
                binding.property_id, 
                binding.template_expression
            );
        }
    }
    
    pub fn validate_expressions(&self) -> Vec<String> {
        let mut errors = Vec::new();
        
        for binding in &self.bindings {
            if let Err(error) = self.parse_expression(&binding.template_expression) {
                errors.push(format!(
                    "Element {}: {} in '{}'", 
                    binding.element_index, 
                    error, 
                    binding.template_expression
                ));
            }
        }
        
        errors
    }
}
```

This comprehensive template engine provides powerful reactive programming capabilities while maintaining excellent performance and developer experience through efficient change detection, expression parsing, and context-aware variable resolution.