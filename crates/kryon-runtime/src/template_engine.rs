// crates/kryon-runtime/src/template_engine.rs
// Complete replacement for the template engine with full expression evaluation

use kryon_core::{KRBFile, Element, ElementId, TemplateVariable, TemplateBinding};
use std::collections::HashMap;
use regex::Regex;

/// Expression AST for runtime evaluation
#[derive(Debug, Clone, PartialEq)]
enum Expression {
    // Literals
    String(String),
    Number(f64),
    Boolean(bool),
    Variable(String),
    
    // Operators
    Equals(Box<Expression>, Box<Expression>),
    NotEquals(Box<Expression>, Box<Expression>),
    LessThan(Box<Expression>, Box<Expression>),
    LessThanOrEqual(Box<Expression>, Box<Expression>),
    GreaterThan(Box<Expression>, Box<Expression>),
    GreaterThanOrEqual(Box<Expression>, Box<Expression>),
    
    // Ternary
    Ternary {
        condition: Box<Expression>,
        true_value: Box<Expression>,
        false_value: Box<Expression>,
    },
}

/// Simple expression parser for the template engine
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
    Question,
    Colon,
    Equals,
    NotEquals,
    LessThan,
    LessThanOrEqual,
    GreaterThan,
    GreaterThanOrEqual,
    LeftParen,
    RightParen,
    Eof,
}

impl ExpressionParser {
    fn new(input: &str) -> Self {
        let tokens = Self::tokenize(input);
        eprintln!("[TEMPLATE_DEBUG] Tokenized '{}' into {} tokens: {:?}", input, tokens.len(), tokens);
        Self { tokens, current: 0 }
    }
    
    fn tokenize(input: &str) -> Vec<Token> {
        let mut tokens = Vec::new();
        let mut chars = input.chars().peekable();
        
        while let Some(&ch) = chars.peek() {
            match ch {
                ' ' | '\t' | '\n' | '\r' => {
                    chars.next();
                }
                '$' => {
                    chars.next(); // consume $
                    let mut var_name = String::new();
                    while let Some(&ch) = chars.peek() {
                        if ch.is_alphanumeric() || ch == '_' {
                            var_name.push(ch);
                            chars.next();
                        } else {
                            break;
                        }
                    }
                    tokens.push(Token::Variable(var_name));
                }
                '"' => {
                    chars.next(); // consume opening "
                    let mut string_value = String::new();
                    while let Some(&ch) = chars.peek() {
                        chars.next();
                        if ch == '"' {
                            break;
                        }
                        string_value.push(ch);
                    }
                    tokens.push(Token::String(string_value));
                }
                '?' => {
                    chars.next();
                    tokens.push(Token::Question);
                }
                ':' => {
                    chars.next();
                    tokens.push(Token::Colon);
                }
                '=' => {
                    chars.next();
                    if chars.peek() == Some(&'=') {
                        chars.next();
                        tokens.push(Token::Equals);
                    }
                }
                '!' => {
                    chars.next();
                    if chars.peek() == Some(&'=') {
                        chars.next();
                        tokens.push(Token::NotEquals);
                    }
                }
                '<' => {
                    chars.next();
                    if chars.peek() == Some(&'=') {
                        chars.next();
                        tokens.push(Token::LessThanOrEqual);
                    } else {
                        tokens.push(Token::LessThan);
                    }
                }
                '>' => {
                    chars.next();
                    if chars.peek() == Some(&'=') {
                        chars.next();
                        tokens.push(Token::GreaterThanOrEqual);
                    } else {
                        tokens.push(Token::GreaterThan);
                    }
                }
                '(' => {
                    chars.next();
                    tokens.push(Token::LeftParen);
                }
                ')' => {
                    chars.next();
                    tokens.push(Token::RightParen);
                }
                _ if ch.is_alphabetic() => {
                    let mut word = String::new();
                    while let Some(&ch) = chars.peek() {
                        if ch.is_alphanumeric() || ch == '_' {
                            word.push(ch);
                            chars.next();
                        } else {
                            break;
                        }
                    }
                    match word.as_str() {
                        "true" => tokens.push(Token::Boolean(true)),
                        "false" => tokens.push(Token::Boolean(false)),
                        _ => tokens.push(Token::String(word)),
                    }
                }
                _ if ch.is_numeric() || ch == '-' => {
                    let mut num_str = String::new();
                    let mut has_dot = false;
                    
                    while let Some(&ch) = chars.peek() {
                        if ch.is_numeric() || (ch == '.' && !has_dot) || (ch == '-' && num_str.is_empty()) {
                            if ch == '.' {
                                has_dot = true;
                            }
                            num_str.push(ch);
                            chars.next();
                        } else {
                            break;
                        }
                    }
                    
                    if let Ok(num) = num_str.parse::<f64>() {
                        tokens.push(Token::Number(num));
                    }
                }
                _ => {
                    chars.next(); // Skip unknown characters
                }
            }
        }
        
        tokens.push(Token::Eof);
        tokens
    }
    
    fn parse(&mut self) -> Result<Expression, String> {
        self.parse_ternary()
    }
    
    fn parse_ternary(&mut self) -> Result<Expression, String> {
        let mut expr = self.parse_comparison()?;
        
        if self.current < self.tokens.len() && self.tokens[self.current] == Token::Question {
            self.current += 1; // consume ?
            let true_expr = self.parse_comparison()?;
            
            if self.current < self.tokens.len() && self.tokens[self.current] == Token::Colon {
                self.current += 1; // consume :
                let false_expr = self.parse_ternary()?; // Right-associative
                
                expr = Expression::Ternary {
                    condition: Box::new(expr),
                    true_value: Box::new(true_expr),
                    false_value: Box::new(false_expr),
                };
            } else {
                return Err("Expected ':' after '?' in ternary expression".to_string());
            }
        }
        
        Ok(expr)
    }
    
    fn parse_comparison(&mut self) -> Result<Expression, String> {
        let mut expr = self.parse_primary()?;
        
        while self.current < self.tokens.len() {
            match &self.tokens[self.current] {
                Token::Equals => {
                    self.current += 1;
                    let right = self.parse_primary()?;
                    expr = Expression::Equals(Box::new(expr), Box::new(right));
                }
                Token::NotEquals => {
                    self.current += 1;
                    let right = self.parse_primary()?;
                    expr = Expression::NotEquals(Box::new(expr), Box::new(right));
                }
                Token::LessThan => {
                    self.current += 1;
                    let right = self.parse_primary()?;
                    expr = Expression::LessThan(Box::new(expr), Box::new(right));
                }
                Token::LessThanOrEqual => {
                    self.current += 1;
                    let right = self.parse_primary()?;
                    expr = Expression::LessThanOrEqual(Box::new(expr), Box::new(right));
                }
                Token::GreaterThan => {
                    self.current += 1;
                    let right = self.parse_primary()?;
                    expr = Expression::GreaterThan(Box::new(expr), Box::new(right));
                }
                Token::GreaterThanOrEqual => {
                    self.current += 1;
                    let right = self.parse_primary()?;
                    expr = Expression::GreaterThanOrEqual(Box::new(expr), Box::new(right));
                }
                _ => break,
            }
        }
        
        Ok(expr)
    }
    
    fn parse_primary(&mut self) -> Result<Expression, String> {
        if self.current >= self.tokens.len() {
            return Err("Unexpected end of expression".to_string());
        }
        
        let token = self.tokens[self.current].clone();
        self.current += 1;
        
        match token {
            Token::String(s) => Ok(Expression::String(s)),
            Token::Number(n) => Ok(Expression::Number(n)),
            Token::Boolean(b) => Ok(Expression::Boolean(b)),
            Token::Variable(v) => Ok(Expression::Variable(v)),
            Token::LeftParen => {
                let expr = self.parse_ternary()?;
                if self.current < self.tokens.len() && self.tokens[self.current] == Token::RightParen {
                    self.current += 1;
                    Ok(expr)
                } else {
                    Err("Expected ')' after expression".to_string())
                }
            }
            _ => Err(format!("Unexpected token: {:?}", token)),
        }
    }
}

/// Template evaluation engine that handles variable substitution and reactive updates
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

impl TemplateEngine {
    /// Create a new template engine from KRB file data
    pub fn new(krb_file: &KRBFile) -> Self {
        let template_regex = Regex::new(r"\$([a-zA-Z_][a-zA-Z0-9_]*)").unwrap();
        
        // Initialize variables with their default values
        let mut variables = HashMap::new();
        for template_var in &krb_file.template_variables {
            variables.insert(template_var.name.clone(), template_var.default_value.clone());
        }
        
        Self {
            variables,
            bindings: krb_file.template_bindings.clone(),
            template_variables: krb_file.template_variables.clone(),
            template_regex,
        }
    }
    
    /// Set a template variable value
    pub fn set_variable(&mut self, name: &str, value: &str) -> bool {
        if let Some(var_value) = self.variables.get_mut(name) {
            if *var_value != value {
                *var_value = value.to_string();
                return true; // Variable changed
            }
        }
        false // Variable not found or unchanged
    }
    
    /// Get a template variable value
    pub fn get_variable(&self, name: &str) -> Option<&str> {
        self.variables.get(name).map(|s| s.as_str())
    }
    
    /// Get all template variables
    pub fn get_variables(&self) -> &HashMap<String, String> {
        &self.variables
    }
    
    /// Evaluate an expression AST
    fn evaluate_expr(&self, expr: &Expression) -> String {
        match expr {
            Expression::String(s) => s.clone(),
            Expression::Number(n) => n.to_string(),
            Expression::Boolean(b) => b.to_string(),
            Expression::Variable(name) => {
                self.variables.get(name).cloned().unwrap_or_default()
            }
            Expression::Equals(left, right) => {
                let left_val = self.evaluate_expr(left);
                let right_val = self.evaluate_expr(right);
                (left_val == right_val).to_string()
            }
            Expression::NotEquals(left, right) => {
                let left_val = self.evaluate_expr(left);
                let right_val = self.evaluate_expr(right);
                (left_val != right_val).to_string()
            }
            Expression::LessThan(left, right) => {
                let left_val = self.evaluate_expr(left);
                let right_val = self.evaluate_expr(right);
                
                // Try numeric comparison first
                if let (Ok(l), Ok(r)) = (left_val.parse::<f64>(), right_val.parse::<f64>()) {
                    (l < r).to_string()
                } else {
                    (left_val < right_val).to_string()
                }
            }
            Expression::LessThanOrEqual(left, right) => {
                let left_val = self.evaluate_expr(left);
                let right_val = self.evaluate_expr(right);
                
                if let (Ok(l), Ok(r)) = (left_val.parse::<f64>(), right_val.parse::<f64>()) {
                    (l <= r).to_string()
                } else {
                    (left_val <= right_val).to_string()
                }
            }
            Expression::GreaterThan(left, right) => {
                let left_val = self.evaluate_expr(left);
                let right_val = self.evaluate_expr(right);
                
                if let (Ok(l), Ok(r)) = (left_val.parse::<f64>(), right_val.parse::<f64>()) {
                    (l > r).to_string()
                } else {
                    (left_val > right_val).to_string()
                }
            }
            Expression::GreaterThanOrEqual(left, right) => {
                let left_val = self.evaluate_expr(left);
                let right_val = self.evaluate_expr(right);
                
                if let (Ok(l), Ok(r)) = (left_val.parse::<f64>(), right_val.parse::<f64>()) {
                    (l >= r).to_string()
                } else {
                    (left_val >= right_val).to_string()
                }
            }
            Expression::Ternary { condition, true_value, false_value } => {
                let condition_result = self.evaluate_expr(condition);
                let is_true = self.evaluate_truthiness(&condition_result);
                
                if is_true {
                    self.evaluate_expr(true_value)
                } else {
                    self.evaluate_expr(false_value)
                }
            }
        }
    }
    
    /// Evaluate truthiness of a value using JavaScript-like rules
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
                // If it's a valid number, check if it's zero
                if let Ok(num) = value.parse::<f64>() {
                    num != 0.0 && !num.is_nan()
                } else {
                    // Non-empty strings are truthy
                    true
                }
            }
        }
    }
    
    /// Evaluate a template expression by parsing and evaluating it
    pub fn evaluate_expression(&self, expression: &str) -> String {
        // First check if it's a simple variable reference
        if expression.starts_with('$') && !expression.contains(' ') {
            let var_name = &expression[1..];
            return self.variables.get(var_name).cloned().unwrap_or_default();
        }
        
        // Try to parse as an expression
        let mut parser = ExpressionParser::new(expression);
        match parser.parse() {
            Ok(expr) => {
                eprintln!("[TEMPLATE_DEBUG] Successfully parsed expression: '{}'", expression);
                self.evaluate_expr(&expr)
            },
            Err(parse_error) => {
                eprintln!("[TEMPLATE_DEBUG] Failed to parse expression: '{}', error: {}", expression, parse_error);
                // If parsing fails, fall back to simple string substitution
                let mut result = expression.to_string();
                
                // Replace all $variable patterns with their values
                for capture in self.template_regex.captures_iter(expression) {
                    if let Some(var_name) = capture.get(1) {
                        let var_name_str = var_name.as_str();
                        if let Some(value) = self.variables.get(var_name_str) {
                            let pattern = format!("${}", var_name_str);
                            result = result.replace(&pattern, value);
                        }
                    }
                }
                
                result
            }
        }
    }
    
    /// Update all elements that have template bindings
    pub fn update_elements(&self, elements: &mut HashMap<ElementId, Element>, krb_file: &kryon_core::KRBFile) {
        let styles = &krb_file.styles;
        eprintln!("[TEMPLATE_UPDATE] Updating {} bindings on {} elements", self.bindings.len(), elements.len());
        let mut elements_with_style_changes = Vec::new();
        
        for binding in &self.bindings {
            if let Some(element) = elements.get_mut(&(binding.element_index as u32)) {
                let evaluated_value = self.evaluate_expression(&binding.template_expression);
                
                eprintln!("[TEMPLATE_UPDATE] Element {}: '{}' -> '{}'", 
                    binding.element_index, binding.template_expression, evaluated_value);
                
                // Update the element property based on property_id
                match binding.property_id {
                    0x08 => { // TextContent property
                        let old_text = element.text.clone();
                        element.text = evaluated_value.clone();
                        eprintln!("[TEMPLATE_UPDATE] Element {} text updated: '{}' -> '{}'", 
                            binding.element_index, old_text, evaluated_value);
                    }
                    0x1D => { // StyleId property
                        let old_style_id = element.style_id;
                        // Find the style ID by name
                        let mut new_style_id = 0;
                        for (style_id, style) in styles {
                            if style.name == evaluated_value {
                                new_style_id = *style_id;
                                break;
                            }
                        }
                        element.style_id = new_style_id;
                        eprintln!("[TEMPLATE_UPDATE] Element {} style_id updated: {} -> {} (name: '{}')", 
                            binding.element_index, old_style_id, new_style_id, evaluated_value);
                        
                        // Track this element for style reapplication
                        if old_style_id != new_style_id {
                            elements_with_style_changes.push(binding.element_index as u32);
                            eprintln!("[TEMPLATE_UPDATE] Marked element {} for style reapplication", binding.element_index);
                        }
                    }
                    // Add more property types as needed
                    _ => {}
                }
            } else {
                eprintln!("[TEMPLATE_UPDATE] Element {} not found in elements map", binding.element_index);
            }
        }
        
        // Re-apply styles for elements that had their style_id changed
        if !elements_with_style_changes.is_empty() {
            eprintln!("[TEMPLATE_UPDATE] Re-applying styles for {} elements with style changes", elements_with_style_changes.len());
            if let Err(e) = krb_file.reapply_styles_for_elements(elements, &elements_with_style_changes) {
                eprintln!("[TEMPLATE_UPDATE] Error re-applying styles: {}", e);
            } else {
                eprintln!("[TEMPLATE_UPDATE] Successfully re-applied styles for {} elements", elements_with_style_changes.len());
            }
        }
    }
    
    /// Get bindings that reference a specific variable
    pub fn get_bindings_for_variable(&self, variable_name: &str) -> Vec<&TemplateBinding> {
        self.bindings.iter()
            .filter(|binding| {
                self.template_regex.captures_iter(&binding.template_expression)
                    .any(|capture| {
                        capture.get(1).map_or(false, |m| m.as_str() == variable_name)
                    })
            })
            .collect()
    }
    
    /// Check if any template bindings exist
    pub fn has_bindings(&self) -> bool {
        !self.bindings.is_empty()
    }
    
    /// Get all template variable names
    pub fn get_variable_names(&self) -> Vec<String> {
        self.variables.keys().cloned().collect()
    }
    
    /// Update a variable and return affected element IDs
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

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_expression_parser_simple() {
        let mut parser = ExpressionParser::new("$test");
        let expr = parser.parse().unwrap();
        assert_eq!(expr, Expression::Variable("test".to_string()));
    }
    
    #[test]
    fn test_expression_parser_ternary() {
        let mut parser = ExpressionParser::new("$active ? \"YES\" : \"NO\"");
        let expr = parser.parse().unwrap();
        
        match expr {
            Expression::Ternary { condition, true_value, false_value } => {
                assert_eq!(*condition, Expression::Variable("active".to_string()));
                assert_eq!(*true_value, Expression::String("YES".to_string()));
                assert_eq!(*false_value, Expression::String("NO".to_string()));
            }
            _ => panic!("Expected ternary expression"),
        }
    }
    
    #[test]
    fn test_expression_parser_comparison() {
        let mut parser = ExpressionParser::new("$count > 5");
        let expr = parser.parse().unwrap();
        
        match expr {
            Expression::GreaterThan(left, right) => {
                assert_eq!(*left, Expression::Variable("count".to_string()));
                assert_eq!(*right, Expression::Number(5.0));
            }
            _ => panic!("Expected greater than expression"),
        }
    }
    
    #[test]
    fn test_expression_parser_nested_ternary() {
        let mut parser = ExpressionParser::new("$x == \"a\" ? \"A\" : $x == \"b\" ? \"B\" : \"C\"");
        let expr = parser.parse().unwrap();
        
        // Should parse as: x == "a" ? "A" : (x == "b" ? "B" : "C")
        match expr {
            Expression::Ternary { condition, true_value, false_value } => {
                // Check first condition
                match &**condition {
                    Expression::Equals(left, right) => {
                        assert_eq!(**left, Expression::Variable("x".to_string()));
                        assert_eq!(**right, Expression::String("a".to_string()));
                    }
                    _ => panic!("Expected equals in first condition"),
                }
                
                // Check true value
                assert_eq!(**true_value, Expression::String("A".to_string()));
                
                // Check nested ternary in false value
                match false_value.as_ref() {
                    Expression::Ternary { condition: nested_cond, true_value: nested_true, false_value: nested_false } => {
                        // Check nested condition
                        match nested_cond.as_ref() {
                            Expression::Equals(left, right) => {
                                assert_eq!(**left, Expression::Variable("x".to_string()));
                                assert_eq!(**right, Expression::String("b".to_string()));
                            }
                            _ => panic!("Expected equals in nested condition"),
                        }
                        
                        assert_eq!(**nested_true, Expression::String("B".to_string()));
                        assert_eq!(**nested_false, Expression::String("C".to_string()));
                    }
                    _ => panic!("Expected nested ternary in false branch"),
                }
            }
            _ => panic!("Expected ternary expression"),
        }
    }
    
    #[test]
    fn test_evaluate_ternary() {
        let krb_file = KRBFile {
            header: KRBHeader {
                magic: [0x4B, 0x52, 0x42, 0x01],
                version: 1,
                flags: 0,
                element_count: 0,
                style_count: 0,
                component_count: 0,
                script_count: 0,
                string_count: 0,
                resource_count: 0,
                template_variable_count: 1,
                template_binding_count: 0,
                transform_count: 0,
            },
            strings: vec![],
            elements: HashMap::new(),
            styles: HashMap::new(),
            root_element_id: None,
            resources: vec![],
            scripts: vec![],
            template_variables: vec![
                TemplateVariable {
                    name: "active".to_string(),
                    value_type: 15, // Boolean
                    default_value: "true".to_string(),
                }
            ],
            template_bindings: vec![],
            transforms: vec![],
            fonts: HashMap::new(),
        };
        
        let engine = TemplateEngine::new(&krb_file);
        
        // Test simple ternary
        let result = engine.evaluate_expression("$active ? \"YES\" : \"NO\"");
        assert_eq!(result, "YES");
        
        // Test with false condition
        let mut engine = TemplateEngine::new(&krb_file);
        engine.set_variable("active", "false");
        let result = engine.evaluate_expression("$active ? \"YES\" : \"NO\"");
        assert_eq!(result, "NO");
    }
    
    #[test]
    fn test_enhanced_truthiness() {
        let krb_file = KRBFile {
            header: KRBHeader {
                magic: [0x4B, 0x52, 0x42, 0x01],
                version: 1,
                flags: 0,
                element_count: 0,
                style_count: 0,
                component_count: 0,
                script_count: 0,
                string_count: 0,
                resource_count: 0,
                template_variable_count: 1,
                template_binding_count: 0,
                transform_count: 0,
            },
            strings: vec![],
            elements: HashMap::new(),
            styles: HashMap::new(),
            root_element_id: None,
            resources: vec![],
            scripts: vec![],
            template_variables: vec![
                TemplateVariable {
                    name: "test_var".to_string(),
                    value_type: 15,
                    default_value: "1".to_string(),
                }
            ],
            template_bindings: vec![],
            transforms: vec![],
            fonts: HashMap::new(),
        };
        
        let mut engine = TemplateEngine::new(&krb_file);
        
        // Test numeric truthiness
        engine.set_variable("test_var", "0");
        assert_eq!(engine.evaluate_expression("$test_var ? \"TRUE\" : \"FALSE\""), "FALSE");
        
        engine.set_variable("test_var", "1");
        assert_eq!(engine.evaluate_expression("$test_var ? \"TRUE\" : \"FALSE\""), "TRUE");
        
        engine.set_variable("test_var", "0.0");
        assert_eq!(engine.evaluate_expression("$test_var ? \"TRUE\" : \"FALSE\""), "FALSE");
        
        engine.set_variable("test_var", "3.14");
        assert_eq!(engine.evaluate_expression("$test_var ? \"TRUE\" : \"FALSE\""), "TRUE");
        
        // Test string truthiness
        engine.set_variable("test_var", "");
        assert_eq!(engine.evaluate_expression("$test_var ? \"TRUE\" : \"FALSE\""), "FALSE");
        
        engine.set_variable("test_var", "hello");
        assert_eq!(engine.evaluate_expression("$test_var ? \"TRUE\" : \"FALSE\""), "TRUE");
        
        // Test special values
        engine.set_variable("test_var", "null");
        assert_eq!(engine.evaluate_expression("$test_var ? \"TRUE\" : \"FALSE\""), "FALSE");
        
        engine.set_variable("test_var", "undefined");
        assert_eq!(engine.evaluate_expression("$test_var ? \"TRUE\" : \"FALSE\""), "FALSE");
        
        engine.set_variable("test_var", "NaN");
        assert_eq!(engine.evaluate_expression("$test_var ? \"TRUE\" : \"FALSE\""), "FALSE");
    }
    
    #[test]
    fn test_evaluate_comparison_ternary() {
        let krb_file = KRBFile {
            header: KRBHeader {
                magic: [0x4B, 0x52, 0x42, 0x01],
                version: 1,
                flags: 0,
                element_count: 0,
                style_count: 0,
                component_count: 0,
                script_count: 0,
                string_count: 0,
                resource_count: 0,
                template_variable_count: 1,
                template_binding_count: 0,
                transform_count: 0,
            },
            strings: vec![],
            elements: HashMap::new(),
            styles: HashMap::new(),
            root_element_id: None,
            resources: vec![],
            scripts: vec![],
            template_variables: vec![
                TemplateVariable {
                    name: "count".to_string(),
                    value_type: 1, // Number
                    default_value: "10".to_string(),
                }
            ],
            template_bindings: vec![],
            transforms: vec![],
            fonts: HashMap::new(),
        };
        
        let engine = TemplateEngine::new(&krb_file);
        
        // Test numeric comparison
        let result = engine.evaluate_expression("$count > 5 ? \"BIG\" : \"SMALL\"");
        assert_eq!(result, "BIG");
        
        // Test with smaller value
        let mut engine = TemplateEngine::new(&krb_file);
        engine.set_variable("count", "3");
        let result = engine.evaluate_expression("$count > 5 ? \"BIG\" : \"SMALL\"");
        assert_eq!(result, "SMALL");
    }
}