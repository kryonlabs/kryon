//! Recursive descent parser for the KRY language

use crate::compiler::frontend::ast::*;
use crate::error::{CompilerError, Result};
use crate::compiler::frontend::lexer::{Token, TokenType};
use std::collections::HashMap;
use regex::Regex;

pub struct Parser {
    tokens: Vec<Token>,
    current: usize,
}

impl Parser {
    pub fn new(tokens: Vec<Token>) -> Self {
        Self {
            tokens,
            current: 0,
        }
    }
    
    pub fn parse(&mut self) -> Result<AstNode> {
        let mut directives = Vec::new();
        let mut styles = Vec::new();
        let mut fonts = Vec::new();
        let mut components = Vec::new();
        let mut scripts = Vec::new();
        let mut app = None;
        let mut standalone_elements = Vec::new(); // Collect standalone elements for auto-wrapping
        
        while !self.is_at_end() {
            // Skip newlines
            if self.match_token(&TokenType::Newline) {
                continue;
            }
            
            // Skip comments
            if matches!(self.peek().token_type, TokenType::Comment(_)) {
                self.advance();
                continue;
            }
            
            match &self.peek().token_type {
                TokenType::Include => {
                    directives.push(self.parse_include()?);
                }
                TokenType::Variables => {
                    directives.push(self.parse_variables()?);
                }
                TokenType::Variable => {
                    directives.push(self.parse_variable()?);
                }
                TokenType::Script => {
                    scripts.push(self.parse_script()?);
                }
                TokenType::Function => {
                    scripts.push(self.parse_function()?);
                }
                TokenType::For => {
                    // @for can appear at root level for generating multiple elements
                    let for_node = self.parse_for()?;
                    standalone_elements.push(for_node);
                }
                TokenType::If => {
                    // @if can appear at root level for conditional elements
                    let if_node = self.parse_if()?;
                    standalone_elements.push(if_node);
                }
                TokenType::Style => {
                    styles.push(self.parse_style()?);
                }
                TokenType::Font => {
                    fonts.push(self.parse_font()?);
                }
                TokenType::Define => {
                    components.push(self.parse_component()?);
                }
                TokenType::App => {
                    if app.is_some() {
                        return Err(CompilerError::parse_legacy(
                            self.peek().line,
                            "Multiple App elements found. Only one App element is allowed."
                        ));
                    }
                    app = Some(Box::new(self.parse_element()?));
                }
                _ => {
                    // Try parsing as element (for component usage at root level or standalone elements)
                    if self.is_element_start() {
                        if app.is_some() {
                            return Err(CompilerError::parse_legacy(
                                self.peek().line,
                                "Only one root element (App or component) is allowed."
                            ));
                        }
                        
                        let element = self.parse_element()?;
                        // Check if this is a standalone element that needs App wrapping
                        if let AstNode::Element { element_type, .. } = &element {
                            if element_type == "App" {
                                app = Some(Box::new(element));
                            } else {
                                // This is a standalone element - collect it for auto-wrapping
                                standalone_elements.push(element);
                            }
                        } else {
                            app = Some(Box::new(element));
                        }
                    } else {
                        return Err(CompilerError::parse_legacy(
                            self.peek().line,
                            format!("Unexpected token: {}", self.peek().token_type)
                        ));
                    }
                }
            }
        }
        
        // Auto-create App wrapper if none exists and we have standalone elements at root level
        let app = if app.is_none() && !standalone_elements.is_empty() {
            self.create_default_app_wrapper(standalone_elements)?
        } else {
            app
        };
        
        Ok(AstNode::File {
            directives,
            styles,
            fonts,
            components,
            scripts,
            app,
        })
    }
    

    fn skip_whitespace_and_comments(&mut self) {
        while !self.is_at_end() {
            // Peek at the next token without consuming it
            match self.peek().token_type {
                // If it's a newline or comment, consume it and continue the loop
                TokenType::Newline | TokenType::Comment(_) => {
                    self.advance();
                }
                // Otherwise, we've found a meaningful token, so we stop.
                _ => break,
            }
        }
    }
    fn parse_include(&mut self) -> Result<AstNode> {
        self.consume(TokenType::Include, "Expected @include")?;
        
        let path = match &self.advance().token_type {
            TokenType::String(s) => s.clone(),
            _ => return Err(CompilerError::parse_legacy(
                self.previous().line,
                "Expected string path after @include"
            )),
        };
        
        Ok(AstNode::Include { path })
    }

    fn parse_variables(&mut self) -> Result<AstNode> {
        self.consume(TokenType::Variables, "Expected @variables")?;
        self.consume(TokenType::LeftBrace, "Expected '{' after @variables")?;
        
        self.skip_whitespace_and_comments();

        let mut variables = HashMap::new();
        
        while !self.check(&TokenType::RightBrace) && !self.is_at_end() {            
            if self.match_token(&TokenType::Newline) {
                continue;
            }
            if matches!(self.peek().token_type, TokenType::Comment(_)) {
                self.advance();
                continue;
            }
            let name = match &self.advance().token_type {
                TokenType::Identifier(name) => name.clone(),
                _ => return Err(CompilerError::parse_legacy(
                    self.previous().line,
                    "Expected variable name"
                )),
            };
            
            self.consume(TokenType::Colon, "Expected ':' after variable name")?;
            
            self.skip_whitespace_and_comments();

            let value = self.parse_value()?;
            variables.insert(name, value.to_string());

            self.skip_whitespace_and_comments();

        }
        
        self.consume(TokenType::RightBrace, "Expected '}' after variables")?;
        
        Ok(AstNode::Variables { variables })
    }
    
    fn parse_variable(&mut self) -> Result<AstNode> {
        self.consume(TokenType::Variable, "Expected @variable/@var")?;
        
        // Parse variable name
        let name = match &self.advance().token_type {
            TokenType::Identifier(name) => name.clone(),
            _ => return Err(CompilerError::parse_legacy(
                self.previous().line,
                "Expected variable name after @variable/@var"
            )),
        };
        
        self.consume(TokenType::Equals, "Expected '=' after variable name")?;
        
        // Parse variable value as string
        let value = match &self.advance().token_type {
            TokenType::String(s) => s.clone(),
            TokenType::Number(n) => n.to_string(),
            TokenType::Boolean(b) => b.to_string(),
            TokenType::Identifier(id) => id.clone(),
            _ => return Err(CompilerError::parse_legacy(
                self.previous().line,
                "Expected value after '=' in variable declaration"
            )),
        };
        
        // Convert single variable to variables map for consistency
        let mut variables = HashMap::new();
        variables.insert(name, value);
        
        Ok(AstNode::Variables { variables })
    }
    
    fn parse_script(&mut self) -> Result<AstNode> {
        self.consume(TokenType::Script, "Expected @script")?;
        
        let language = match &self.advance().token_type {
            TokenType::String(lang) => lang.clone(),
            _ => return Err(CompilerError::parse_legacy(
                self.previous().line,
                "Expected language string after @script"
            )),
        };
        
        let mut name = None;
        let mut mode = None;
        let mut source = None;
        
        // Parse optional attributes and source
        while !self.is_at_end() && !self.check(&TokenType::LeftBrace) {
            if self.match_token(&TokenType::Newline) {
                continue;
            }
            
            if let TokenType::Identifier(attr) = &self.peek().token_type {
                match attr.as_str() {
                    "name" => {
                        self.advance();
                        self.consume(TokenType::Equals, "Expected '=' after name")?;
                        name = Some(self.parse_value()?.to_string());
                    }
                    "mode" => {
                        self.advance();
                        self.consume(TokenType::Equals, "Expected '=' after mode")?;
                        mode = Some(self.parse_value()?.to_string());
                    }
                    "from" => {
                        self.advance();
                        let file_path = self.parse_value()?;
                        source = Some(ScriptSource::External(file_path.to_string()));
                    }
                    _ => break,
                }
            } else {
                break;
            }
        }
        
        // Parse inline code if no external source
        if source.is_none() {
            self.consume(TokenType::LeftBrace, "Expected '{' for script code")?;
            let code = self.parse_script_code()?;
            source = Some(ScriptSource::Inline(code));
            // Note: parse_script_code already handles the closing brace
        }
        
        Ok(AstNode::Script {
            language,
            name,
            source: source.unwrap(),
            mode,
        })
    }
    
    fn parse_script_code(&mut self) -> Result<String> {
        // Check if we have a ScriptContent token (new approach)
        if let TokenType::ScriptContent(content) = &self.peek().token_type {
            let content = content.clone();
            self.advance(); // consume the ScriptContent token
            // Also consume the closing brace that should follow
            self.consume(TokenType::RightBrace, "Expected '}' after script content")?;
            return Ok(content);
        }
        
        // Fallback to old approach for compatibility
        let mut code = String::new();
        let mut brace_count = 1;
        let mut last_was_space = false;
        
        while !self.is_at_end() && brace_count > 0 {
            match &self.advance().token_type {
                TokenType::LeftBrace => {
                    brace_count += 1;
                    code.push('{');
                    last_was_space = false;
                }
                TokenType::RightBrace => {
                    brace_count -= 1;
                    if brace_count > 0 {
                        code.push('}');
                        last_was_space = false;
                    }
                }
                TokenType::String(s) => {
                    code.push_str(&format!("\"{}\"", s));
                    last_was_space = false;
                }
                TokenType::Identifier(id) => {
                    if !last_was_space && !code.is_empty() && !code.ends_with(|c: char| c.is_whitespace() || c == '(' || c == '{' || c == ',' || c == '=' || c == '.') {
                        code.push(' ');
                    }
                    code.push_str(id);
                    last_was_space = false;
                }
                TokenType::Number(n) => {
                    if !last_was_space && !code.is_empty() && !code.ends_with(|c: char| c.is_whitespace() || c == '(' || c == '{' || c == ',' || c == '=' || c == '.') {
                        code.push(' ');
                    }
                    code.push_str(&n.to_string());
                    last_was_space = false;
                }
                TokenType::Integer(i) => {
                    if !last_was_space && !code.is_empty() && !code.ends_with(|c: char| c.is_whitespace() || c == '(' || c == '{' || c == ',' || c == '=' || c == '.') {
                        code.push(' ');
                    }
                    code.push_str(&i.to_string());
                    last_was_space = false;
                }
                TokenType::Newline => {
                    code.push('\n');
                    last_was_space = true;
                }
                TokenType::Dot => {
                    code.push('.');
                    last_was_space = false;
                }
                TokenType::LeftParen => {
                    code.push('(');
                    last_was_space = false;
                }
                TokenType::RightParen => {
                    code.push(')');
                    last_was_space = false;
                }
                TokenType::Comma => {
                    code.push(',');
                    last_was_space = false;
                }
                TokenType::Equals => {
                    code.push_str(" = ");
                    last_was_space = true;
                }
                TokenType::Comment(comment) => {
                    code.push_str(&format!("--{}", comment));
                    last_was_space = false;
                }
                token => {
                    // Handle any other tokens by converting to string
                    let token_str = format!("{}", token);
                    if !token_str.is_empty() {
                        if !last_was_space && !code.is_empty() && !code.ends_with(|c: char| c.is_whitespace()) {
                            code.push(' ');
                        }
                        code.push_str(&token_str);
                        last_was_space = false;
                    }
                }
            }
        }
        
        Ok(code.trim().to_string())
    }
    
    fn parse_function(&mut self) -> Result<AstNode> {
        self.consume(TokenType::Function, "Expected @function")?;
        
        let language = match &self.advance().token_type {
            TokenType::String(lang) => lang.clone(),
            _ => return Err(CompilerError::parse_legacy(
                self.previous().line,
                "Expected language string after @function"
            )),
        };
        
        // Parse function name and parameters (may include template variables)
        let function_name = self.parse_function_name_pattern()?;
        
        // Parse parameter list
        self.consume(TokenType::LeftParen, "Expected '(' after function name")?;
        let mut parameters = Vec::new();
        
        while !self.check(&TokenType::RightParen) && !self.is_at_end() {
            if let TokenType::Identifier(param) = &self.advance().token_type {
                parameters.push(param.clone());
                if self.match_token(&TokenType::Comma) {
                    continue;
                }
            } else {
                return Err(CompilerError::parse_legacy(
                    self.previous().line,
                    "Expected parameter name"
                ));
            }
        }
        
        self.consume(TokenType::RightParen, "Expected ')' after parameters")?;
        
        // Parse function body
        self.consume(TokenType::LeftBrace, "Expected '{' for function body")?;
        let body = self.parse_script_code()?;
        
        // Generate the full function code
        let param_list = parameters.join(", ");
        let full_code = format!("function {}({})\n{}\nend", function_name, param_list, body);
        
        // Return as a script node with the function name as the script name
        Ok(AstNode::Script {
            language,
            name: Some(function_name.clone()),
            source: ScriptSource::Inline(full_code),
            mode: None,
        })
    }
    
    fn parse_style(&mut self) -> Result<AstNode> {
        self.consume(TokenType::Style, "Expected 'style'")?;
        

        let name = if let TokenType::String(name) = &self.peek().token_type {
            name.clone()
        } else {
            return Err(CompilerError::parse_legacy(
                self.peek().line,
                format!("Expected style name string, but found {}", self.peek().token_type)
            ));
        };
        self.advance();

        self.consume(TokenType::LeftBrace, "Expected '{' after style name")?;
        
        let mut extends = Vec::new();
        let mut properties = Vec::new();
        let mut pseudo_selectors = Vec::new();
        
        while !self.check(&TokenType::RightBrace) && !self.is_at_end() {
            if self.match_token(&TokenType::Newline) {
                continue;
            }
            if matches!(self.peek().token_type, TokenType::Comment(_)) {
                self.advance();
                continue;
            }
            
            // Check for pseudo-selectors
            if matches!(self.peek().token_type, TokenType::PseudoSelector(_)) {
                pseudo_selectors.push(self.parse_pseudo_selector()?);
            } else {
                let prop = self.parse_property()?;
                
                // Handle extends specially
                if prop.key == "extends" {
                    extends = self.parse_extends_value(&prop.value)?;
                } else {
                    properties.push(prop);
                }
            }
        }
        
        self.consume(TokenType::RightBrace, "Expected '}' after style properties")?;
        
        Ok(AstNode::Style {
            name,
            extends,
            properties,
            pseudo_selectors,
        })
    }
    
    fn parse_extends_value(&self, value: &PropertyValue) -> Result<Vec<String>> {
        match value {
            PropertyValue::Array(arr) => {
                let mut extends = Vec::new();
                for item in arr {
                    match item {
                        PropertyValue::String(s) => {
                            // Remove quotes if present
                            let cleaned = if s.starts_with('"') && s.ends_with('"') {
                                s[1..s.len()-1].to_string()
                            } else {
                                s.clone()
                            };
                            extends.push(cleaned);
                        }
                        _ => extends.push(item.to_string()),
                    }
                }
                Ok(extends)
            }
            PropertyValue::String(s) => {
                let trimmed = s.trim();
                if trimmed.starts_with('[') && trimmed.ends_with(']') {
                    // Array format: ["style1", "style2"]
                    let content = &trimmed[1..trimmed.len()-1];
                    let mut extends = Vec::new();
                    
                    for item in content.split(',') {
                        let item = item.trim();
                        if item.starts_with('"') && item.ends_with('"') {
                            extends.push(item[1..item.len()-1].to_string());
                        } else if !item.is_empty() {
                            extends.push(item.to_string());
                        }
                    }
                    
                    Ok(extends)
                } else if trimmed.starts_with('"') && trimmed.ends_with('"') {
                    // Single quoted string
                    Ok(vec![trimmed[1..trimmed.len()-1].to_string()])
                } else {
                    // Single unquoted string
                    Ok(vec![trimmed.to_string()])
                }
            }
            _ => Ok(vec![value.to_string()]),
        }
    }
    
    fn parse_font(&mut self) -> Result<AstNode> {
        self.consume(TokenType::Font, "Expected 'font'")?;
        
        // Parse font name (first string)
        let name = if let TokenType::String(name) = &self.peek().token_type {
            name.clone()
        } else {
            return Err(CompilerError::parse_legacy(
                self.peek().line,
                format!("Expected font name string, but found {}", self.peek().token_type)
            ));
        };
        self.advance();
        
        // Parse font path (second string)
        let path = if let TokenType::String(path) = &self.peek().token_type {
            path.clone()
        } else {
            return Err(CompilerError::parse_legacy(
                self.peek().line,
                format!("Expected font path string, but found {}", self.peek().token_type)
            ));
        };
        self.advance();
        
        Ok(AstNode::Font {
            name,
            path,
        })
    }
    
    fn parse_component(&mut self) -> Result<AstNode> {
        self.consume(TokenType::Define, "Expected 'Define'")?;
        
        let name = if let TokenType::Identifier(name) = &self.peek().token_type {
            name.clone()
        } else {
            return Err(CompilerError::parse_legacy(
                self.peek().line,
                "Expected component name after 'Define'"
            ));
        };
        self.advance();
        
        self.consume(TokenType::LeftBrace, "Expected '{' after component name")?;
        
        let mut properties = Vec::new();
        let mut template = None;
        let mut functions = Vec::new();
        
        while !self.check(&TokenType::RightBrace) && !self.is_at_end() {
            if self.match_token(&TokenType::Newline) {
                continue;
            }
            if matches!(self.peek().token_type, TokenType::Comment(_)) {
                self.advance();
                continue;
            }

            
            if self.match_token(&TokenType::Properties) {
                properties = self.parse_component_properties()?;
            } else if self.check(&TokenType::Function) {
                functions.push(self.parse_function()?);
            } else if self.check(&TokenType::Script) {
                functions.push(self.parse_script()?);
            } else if self.is_element_start() {
                if template.is_some() {
                    return Err(CompilerError::parse_legacy(
                        self.peek().line,
                        "Component can only have one root template element"
                    ));
                }
                template = Some(Box::new(self.parse_element()?));
            } else if matches!(self.peek().token_type, TokenType::Identifier(_)) {
                // Parse direct property declaration (without Properties wrapper)
                let property = self.parse_direct_component_property()?;
                properties.push(property);
            } else {
                return Err(CompilerError::parse_legacy(
                    self.peek().line,
                    "Expected 'Properties' block, property declaration, '@function', '@script', or template element in component"
                ));
            }
        }
        
        self.consume(TokenType::RightBrace, "Expected '}' after component definition")?;
        
        let template = template.ok_or_else(|| CompilerError::parse_legacy(
            self.previous().line,
            "Component must have a template element"
        ))?;
        
        Ok(AstNode::Component {
            name,
            properties,
            template,
            functions,
        })
    }
    
    fn parse_component_properties(&mut self) -> Result<Vec<ComponentProperty>> {
        self.consume(TokenType::LeftBrace, "Expected '{' after 'Properties'")?;
        
        let mut properties = Vec::new();
        
        while !self.check(&TokenType::RightBrace) && !self.is_at_end() {
            if self.match_token(&TokenType::Newline) {
                continue;
            }
            if matches!(self.peek().token_type, TokenType::Comment(_)) {
                self.advance();
                continue;
            }
            
            let name = match &self.advance().token_type {
                TokenType::Identifier(name) => name.clone(),
                _ => return Err(CompilerError::parse_legacy(
                    self.previous().line,
                    "Expected property name"
                )),
            };
            
            let property_type = if self.match_token(&TokenType::Colon) {
                // Old syntax: name: Type = value
                match &self.advance().token_type {
                    TokenType::Identifier(type_name) => Some(type_name.clone()),
                    _ => return Err(CompilerError::parse_legacy(
                        self.previous().line,
                        "Expected property type after ':'"
                    )),
                }
            } else {
                // New syntax: name = value (infer type)
                None
            };
            
            self.consume(TokenType::Equals, "Expected '=' after property name or type")?;
            
            let default_value = Some(self.parse_value()?.to_string());
            
            // Infer type if not explicitly provided
            let inferred_type = if property_type.is_none() {
                Some(self.infer_type_from_value(&default_value.as_ref().unwrap())?)
            } else {
                property_type
            };
            
            properties.push(ComponentProperty::new(
                name,
                inferred_type,
                default_value,
                self.previous().line,
            ));
        }
        
        self.consume(TokenType::RightBrace, "Expected '}' after component properties")?;
        
        Ok(properties)
    }
    
    fn parse_direct_component_property(&mut self) -> Result<ComponentProperty> {
        let name = match &self.advance().token_type {
            TokenType::Identifier(name) => name.clone(),
            _ => return Err(CompilerError::parse_legacy(
                self.previous().line,
                "Expected property name"
            )),
        };
        
        self.consume(TokenType::Equals, "Expected '=' after property name")?;
        
        let default_value = Some(self.parse_value()?.to_string());
        
        // Infer type from default value
        let inferred_type = Some(self.infer_type_from_value(&default_value.as_ref().unwrap())?);
        
        Ok(ComponentProperty::new(
            name,
            inferred_type,
            default_value,
            self.previous().line,
        ))
    }
    
    fn infer_type_from_value(&self, value_str: &str) -> Result<String> {
        let trimmed = value_str.trim();
        
        // Check for boolean values
        if trimmed == "true" || trimmed == "false" {
            return Ok("Bool".to_string());
        }
        
        // Check for array values
        if trimmed.starts_with('[') && trimmed.ends_with(']') {
            return Ok("Array".to_string());
        }
        
        // Check for numeric values
        if let Ok(_) = trimmed.parse::<i64>() {
            return Ok("Number".to_string());
        }
        
        if let Ok(_) = trimmed.parse::<f64>() {
            return Ok("Number".to_string());
        }
        
        // Check for color values (hex colors)
        if trimmed.starts_with('#') && (trimmed.len() == 7 || trimmed.len() == 9) {
            return Ok("Color".to_string());
        }
        
        // Default to String for quoted strings and everything else
        Ok("String".to_string())
    }
    
    fn parse_element(&mut self) -> Result<AstNode> {
        let element_type = match &self.peek().token_type {
            TokenType::App => "App".to_string(),
            TokenType::Container => "Container".to_string(),
            TokenType::Text => "Text".to_string(),
            TokenType::Link => "Link".to_string(),
            TokenType::Image => "Image".to_string(),
            TokenType::EmbedView => "EmbedView".to_string(),
            TokenType::Canvas => "Canvas".to_string(),
            TokenType::Button => "Button".to_string(),
            TokenType::Input => "Input".to_string(),
            TokenType::Identifier(name) => name.clone(),
            _ => return Err(CompilerError::parse_legacy(
                self.peek().line,
                "Expected element type"
            )),
        };
        self.advance(); 
        
        let mut properties = Vec::new();
        let mut pseudo_selectors = Vec::new();
        let mut children = Vec::new();
        
        if self.match_token(&TokenType::LeftBrace) {
            while !self.check(&TokenType::RightBrace) && !self.is_at_end() {
                if self.match_token(&TokenType::Newline) {
                    continue;
                }
                if matches!(self.peek().token_type, TokenType::Comment(_)) {
                    self.advance();
                    continue;
                }

                if matches!(self.peek().token_type, TokenType::PseudoSelector(_)) {
                    pseudo_selectors.push(self.parse_pseudo_selector()?);
                } else if self.is_property() {
                    properties.push(self.parse_property()?);
                } else if self.is_element_start() {
                    children.push(self.parse_element()?);
                } else if matches!(self.peek().token_type, TokenType::For) {
                    children.push(self.parse_for()?);
                } else if matches!(self.peek().token_type, TokenType::If) {
                    children.push(self.parse_if()?);
                } else if matches!(self.peek().token_type, TokenType::String(_)) {
                    // Handle Text element shorthand
                    if element_type == "Text" {
                        let string_value = match &self.advance().token_type {
                            TokenType::String(s) => s.clone(),
                            _ => unreachable!(),
                        };
                        properties.push(AstProperty::new(
                            "text".to_string(),
                            PropertyValue::String(format!("\"{}\"", string_value)),
                            self.previous().line,
                        ));
                    } else {
                        return Err(CompilerError::parse(
                            self.peek().token_type.to_string(),
                            self.peek().line,
                            &format!("String literal shorthand only supported for Text elements, not {}", element_type)
                        ));
                    }
                } else {
                    // Better error handling for unexpected tokens
                    match &self.peek().token_type {
                        // These tokens suggest a syntax error in an expression
                        TokenType::Question | TokenType::Colon | 
                        TokenType::EqualEquals | TokenType::NotEquals |
                        TokenType::LessThan | TokenType::LessThanOrEqual |
                        TokenType::GreaterThan | TokenType::GreaterThanOrEqual => {
                            return Err(CompilerError::parse(
                                self.peek().token_type.to_string(),
                                self.peek().line,
                                "Operator found outside of property value. Check your syntax - operators like '?', '==', etc. must be part of a property value expression"
                            ));
                        }
                        _ => {
                            return Err(CompilerError::parse(
                                self.peek().token_type.to_string(),
                                self.peek().line,
                                &format!("Unexpected token in element body. Expected property, child element, or closing brace")
                            ));
                        }
                    }
                }
            }
            
            self.consume(TokenType::RightBrace, "Expected '}' after element body")?;
        }
        
        Ok(AstNode::Element {
            element_type,
            properties,
            pseudo_selectors,
            children,
        })
    }
    
    fn parse_pseudo_selector(&mut self) -> Result<PseudoSelector> {
        // Extract the state from the pseudo-selector token
        let state = match &self.advance().token_type {
            TokenType::PseudoSelector(state) => state.clone(),
            _ => return Err(CompilerError::parse_legacy(
                self.previous().line,
                "Expected pseudo-selector"
            )),
        };
        
        self.consume(TokenType::LeftBrace, "Expected '{' after pseudo-selector")?;
        
        let mut properties = Vec::new();
        while !self.check(&TokenType::RightBrace) && !self.is_at_end() {
            if self.match_token(&TokenType::Newline) {
                continue;
            }
            properties.push(self.parse_property()?);
        }
        
        self.consume(TokenType::RightBrace, "Expected '}' after pseudo-selector properties")?;
        
        Ok(PseudoSelector::new(state, properties, self.previous().line))
    }

    fn parse_property(&mut self) -> Result<AstProperty> {
        let key = match &self.peek().token_type {
            TokenType::Identifier(name) => name.clone(),
            // Allow keywords as property names
            TokenType::Style => "style".to_string(),
            TokenType::Text => "text".to_string(),
            TokenType::Link => "link".to_string(),
            TokenType::Image => "image".to_string(),
            TokenType::EmbedView => "embedview".to_string(),
            TokenType::Canvas => "canvas".to_string(),
            TokenType::Button => "button".to_string(),
            TokenType::Input => "input".to_string(),
            TokenType::Container => "container".to_string(),
            TokenType::App => "app".to_string(),
            TokenType::Define => "define".to_string(),
            TokenType::Properties => "properties".to_string(),
            _ => {
                return Err(CompilerError::parse(
                    self.peek().token_type.to_string(),
                    self.peek().line,
                    &format!("Expected property name, but found '{}'", self.peek().token_type)
                ));
            }
        };
        self.advance();
    
        self.consume(TokenType::Colon, "Expected ':' after property name")?;
        
        // Skip any newlines after the colon
        while self.match_token(&TokenType::Newline) {}
        
        // Parse the value using our robust parse_value method
        let value = if key == "margin" || key == "padding" {
            self.parse_multi_value()?
        } else {
            self.parse_value()?
        };
        
        // Extract template variables based on the value type
        let template_variables = match &value {
            PropertyValue::String(s) => self.extract_template_variables(s),
            PropertyValue::Variable(v) => vec![v.clone()],
            PropertyValue::Expression(expr) => expr.extract_variables(),
            PropertyValue::Array(items) => {
                let mut vars = Vec::new();
                for item in items {
                    match item {
                        PropertyValue::String(s) => vars.extend(self.extract_template_variables(s)),
                        PropertyValue::Variable(v) => vars.push(v.clone()),
                        PropertyValue::Expression(expr) => vars.extend(expr.extract_variables()),
                        _ => {}
                    }
                }
                vars
            }
            _ => Vec::new(),
        };
        
        Ok(AstProperty::new_with_templates(
            key,
            value,
            self.previous().line,
            template_variables,
        ))
    }

    fn is_function_call(&self) -> bool {
        // A function call is an Identifier followed by a LeftParen
        if let TokenType::Identifier(_) = self.tokens[self.current].token_type {
            if self.current + 1 < self.tokens.len() {
                if let TokenType::LeftParen = self.tokens[self.current + 1].token_type {
                    return true;
                }
            }
        }
        false
    }
        
    /// Parse multi-value properties like "margin: 10 0" or "padding: 5 10 15 20" or "padding: \"10 16\""
    fn parse_multi_value(&mut self) -> Result<PropertyValue> {
        // Check if it's a quoted string first
        if let TokenType::String(s) = &self.peek().token_type {
            let quoted_value = s.clone();
            self.advance();
            
            // Check if the quoted string contains space-separated numbers
            if self.is_space_separated_numbers(&quoted_value) {
                // Parse the numbers from the string
                let numbers = self.parse_numbers_from_string(&quoted_value)?;
                let value_strings: Vec<String> = numbers.iter().map(|n| n.to_string()).collect();
                return Ok(PropertyValue::String(value_strings.join(" ")));
            } else {
                // Regular quoted string, return as is
                return Ok(PropertyValue::String(quoted_value));
            }
        }
        
        let mut values = Vec::new();
        
        // Parse first value
        values.push(self.parse_single_numeric_value()?);
        
        // Parse additional values if they exist (space-separated)
        while self.is_at_numeric_value() {
            values.push(self.parse_single_numeric_value()?);
        }
        
        // Convert to CSS-style string representation
        let value_strings: Vec<String> = values.iter().map(|v| {
            match v {
                PropertyValue::Integer(i) => i.to_string(),
                PropertyValue::Number(n) => n.to_string(),
                PropertyValue::Pixels(p) => format!("{}px", p),
                PropertyValue::Percentage(p) => format!("{}%", p),
                _ => "0".to_string(),
            }
        }).collect();
        
        Ok(PropertyValue::String(value_strings.join(" ")))
    }
    
    /// Parse a single numeric value (integer, number, pixels, etc.)
    fn parse_single_numeric_value(&mut self) -> Result<PropertyValue> {
        match &self.peek().token_type {
            TokenType::Integer(i) => {
                let value = *i;
                self.advance();
                Ok(PropertyValue::Integer(value))
            }
            TokenType::Number(n) => {
                let value = *n;
                self.advance();
                Ok(PropertyValue::Number(value))
            }
            TokenType::Pixels(p) => {
                let value = *p;
                self.advance();
                Ok(PropertyValue::Pixels(value))
            }
            TokenType::Percentage(p) => {
                let value = *p;
                self.advance();
                Ok(PropertyValue::Percentage(value))
            }
            _ => Err(CompilerError::parse(
                self.peek().token_type.to_string(),
                self.peek().line,
                "Expected numeric value for margin/padding"
            ))
        }
    }
    
    /// Check if we're at a numeric value without advancing
    fn is_at_numeric_value(&self) -> bool {
        matches!(self.peek().token_type, 
            TokenType::Integer(_) | 
            TokenType::Number(_) | 
            TokenType::Pixels(_) | 
            TokenType::Percentage(_)
        )
    }
    
    /// Check if a string contains space-separated numbers
    fn is_space_separated_numbers(&self, s: &str) -> bool {
        let parts: Vec<&str> = s.split_whitespace().collect();
        if parts.is_empty() {
            return false;
        }
        
        // All parts must be valid numbers
        parts.iter().all(|part| {
            part.parse::<i32>().is_ok() || part.parse::<f64>().is_ok()
        })
    }
    
    /// Parse numbers from a space-separated string
    fn parse_numbers_from_string(&self, s: &str) -> Result<Vec<i32>> {
        let parts: Vec<&str> = s.split_whitespace().collect();
        let mut numbers = Vec::new();
        
        for part in parts {
            if let Ok(num) = part.parse::<i32>() {
                numbers.push(num);
            } else {
                return Err(CompilerError::parse(
                    format!("string \"{}\"", s),
                    0, // We don't have line info in this context
                    &format!("Invalid number '{}' in space-separated value", part)
                ));
            }
        }
        
        Ok(numbers)
    }

    fn parse_value(&mut self) -> Result<PropertyValue> {
        let start_position = self.current;
        let start_line = self.peek().line;
        
        // Skip any leading whitespace/newlines
        while self.match_token(&TokenType::Newline) {}
        
        // Check for comments and skip them
        if matches!(self.peek().token_type, TokenType::Comment(_)) {
            self.advance();
            return self.parse_value(); // Recursively try again after comment
        }
        
        // FIRST: Check if this is a function call (identifier followed by '(')
        if let TokenType::Identifier(_) = &self.peek().token_type {
            if self.peek_next().map_or(false, |t| matches!(t.token_type, TokenType::LeftParen)) {
                // This is definitely a function call, not an expression
                return self.parse_function_call();
            }
        }
        
        // THEN: Check if this could be an expression
        if self.could_start_expression() {
            match self.try_parse_expression() {
                Ok(expr) => {
                    // Successfully parsed an expression
                    match &expr {
                        // Simple literals that are not part of a larger expression
                        Expression::String(s) if !self.is_followed_by_operator() => {
                            Ok(PropertyValue::String(s.clone()))
                        }
                        Expression::Number(n) if !self.is_followed_by_operator() => {
                            Ok(PropertyValue::Number(*n))
                        }
                        Expression::Integer(i) if !self.is_followed_by_operator() => {
                            Ok(PropertyValue::Integer(*i))
                        }
                        Expression::Boolean(b) if !self.is_followed_by_operator() => {
                            Ok(PropertyValue::Boolean(*b))
                        }
                        // Variables should stay as expressions for proper evaluation
                        Expression::Variable(_) => {
                            Ok(PropertyValue::Expression(Box::new(expr)))
                        }
                        // All complex expressions (comparisons, ternary, etc.) stay wrapped
                        _ => Ok(PropertyValue::Expression(Box::new(expr)))
                    }
                }
                Err(_) => {
                    // Expression parsing failed, reset position
                    self.current = start_position;
                    
                    // Try parsing as a non-expression value
                    self.parse_non_expression_value()
                }
            }
        } else {
            // Definitely not an expression, parse as regular value
            self.parse_non_expression_value()
        }
    }
    
    
    /// Parse non-expression values (units, colors, objects, arrays, etc.)
    fn parse_non_expression_value(&mut self) -> Result<PropertyValue> {
        match &self.peek().token_type {
            // String literals
            TokenType::String(s) => {
                let value = s.clone();
                self.advance();
                Ok(PropertyValue::String(value))
            }
            
            // Numeric values with units
            TokenType::Number(n) => {
                let value = *n;
                self.advance();
                Ok(PropertyValue::Number(value))
            }
            TokenType::Integer(i) => {
                let value = *i;
                self.advance();
                Ok(PropertyValue::Integer(value))
            }
            TokenType::Pixels(p) => {
                let value = *p;
                self.advance();
                Ok(PropertyValue::Pixels(value))
            }
            TokenType::Em(e) => {
                let value = *e;
                self.advance();
                Ok(PropertyValue::Em(value))
            }
            TokenType::Rem(r) => {
                let value = *r;
                self.advance();
                Ok(PropertyValue::Rem(value))
            }
            TokenType::ViewportWidth(vw) => {
                let value = *vw;
                self.advance();
                Ok(PropertyValue::ViewportWidth(value))
            }
            TokenType::ViewportHeight(vh) => {
                let value = *vh;
                self.advance();
                Ok(PropertyValue::ViewportHeight(value))
            }
            TokenType::Percentage(p) => {
                let value = *p;
                self.advance();
                Ok(PropertyValue::Percentage(value))
            }
            TokenType::Degrees(d) => {
                let value = *d;
                self.advance();
                Ok(PropertyValue::Degrees(value))
            }
            TokenType::Radians(r) => {
                let value = *r;
                self.advance();
                Ok(PropertyValue::Radians(value))
            }
            TokenType::Turns(t) => {
                let value = *t;
                self.advance();
                Ok(PropertyValue::Turns(value))
            }
            
            // Boolean values
            TokenType::Boolean(b) => {
                let value = *b;
                self.advance();
                Ok(PropertyValue::Boolean(value))
            }
            
            // Color values
            TokenType::Color(c) => {
                let value = c.clone();
                self.advance();
                Ok(PropertyValue::Color(value))
            }
            
            // Object literals
            TokenType::LeftBrace => {
                self.parse_object_literal()
            }
            
            // Array literals
            TokenType::LeftBracket => {
                self.parse_array_literal()
            }
            
            // Identifiers (could be function calls or string values)
            TokenType::Identifier(id) => {
                // Peek ahead to see if this is a function call
                if self.peek_next().map_or(false, |t| matches!(t.token_type, TokenType::LeftParen)) {
                    self.parse_function_call()
                } else {
                    // Regular identifier used as a string value
                    let value = id.clone();
                    self.advance();
                    Ok(PropertyValue::String(value))
                }
            }
            
            // Error cases
            _ => {
                Err(CompilerError::parse(
                    self.peek().token_type.to_string(),
                    self.peek().line,
                    &format!(
                        "Expected a value (string, number, boolean, color, object, array, or expression), but found '{}'",
                        self.peek().token_type
                    )
                ))
            }
        }
    }

    /// Helper: Check if the current token could start an expression
    fn could_start_expression(&self) -> bool {
        match &self.peek().token_type {
            TokenType::Dollar |           // Variables
            TokenType::String(_) |        // String literals in expressions
            TokenType::Number(_) |        // Numeric literals
            TokenType::Integer(_) |       // Integer literals
            TokenType::Boolean(_) |       // Boolean literals
            TokenType::LeftParen |        // Parenthesized expressions
            TokenType::Identifier(_) => { // Identifiers that might be compared
                // Check if followed by an operator or could be part of expression
                true
            }
            _ => false
        }
    }

    /// Helper: Check if tokens ahead look like an expression
    fn looks_like_expression(&self) -> bool {
        // Look ahead for expression operators
        for i in 0..5 {
            if let Some(token) = self.peek_ahead(i) {
                match &token.token_type {
                    TokenType::Question |      // Ternary
                    TokenType::Colon |         // Ternary
                    TokenType::EqualEquals |   // Comparison
                    TokenType::NotEquals |     // Comparison
                    TokenType::LessThan |      // Comparison
                    TokenType::LessThanOrEqual |
                    TokenType::GreaterThan |
                    TokenType::GreaterThanOrEqual => {
                        return true;
                    }
                    _ => continue,
                }
            }
        }
        false
    }

    /// Helper: Check if the next token is an operator (for expression detection)
    fn is_followed_by_operator(&self) -> bool {
        if let Some(next) = self.peek_next() {
            matches!(next.token_type,
                TokenType::Question |
                TokenType::Colon |
                TokenType::EqualEquals |
                TokenType::NotEquals |
                TokenType::LessThan |
                TokenType::LessThanOrEqual |
                TokenType::GreaterThan |
                TokenType::GreaterThanOrEqual
            )
        } else {
            false
        }
    }

    /// Helper: Validate that an expression is valid in the current context
    fn validate_expression_context(&self, expr: &Expression) -> bool {
        // All expressions are valid in property values
        // This method exists for future extensibility
        match expr {
            Expression::Ternary { .. } |
            Expression::NotEquals(_, _) |
            Expression::EqualEquals(_, _) |
            Expression::LessThan(_, _) |
            Expression::LessThanOrEqual(_, _) |
            Expression::GreaterThan(_, _) |
            Expression::GreaterThanOrEqual(_, _) |
            Expression::Variable(_) |
            Expression::String(_) |
            Expression::Number(_) |
            Expression::Integer(_) |
            Expression::Boolean(_) => true,
        }
    }

    /// Helper: Peek at the next token without advancing
    fn peek_next(&self) -> Option<&Token> {
        self.tokens.get(self.current + 1)
    }

    /// Helper: Peek ahead by n tokens without advancing
    fn peek_ahead(&self, offset: usize) -> Option<&Token> {
        self.tokens.get(self.current + offset)
    }

    /// Parse function call: identifier(arg1, arg2, ...)
    fn parse_function_call(&mut self) -> Result<PropertyValue> {
        let name = match &self.advance().token_type {
            TokenType::Identifier(n) => n.clone(),
            _ => return Err(CompilerError::parse(
                self.previous().token_type.to_string(),
                self.previous().line,
                "Expected function name"
            )),
        };
        
        self.consume(TokenType::LeftParen, "Expected '(' after function name")?;
        
        let mut args = Vec::new();
        
        // Parse arguments
        if !self.check(&TokenType::RightParen) {
            loop {
                // Skip newlines between arguments
                while self.match_token(&TokenType::Newline) {}
                
                // Parse each argument as a value
                args.push(self.parse_value()?);
                
                // Skip newlines after argument
                while self.match_token(&TokenType::Newline) {}
                
                // Check for comma or end of arguments
                if !self.match_token(&TokenType::Comma) {
                    break;
                }
                
                // Skip newlines after comma
                while self.match_token(&TokenType::Newline) {}
            }
        }
        
        self.consume(TokenType::RightParen, "Expected ')' after function arguments")?;
        
        Ok(PropertyValue::FunctionCall { name, args })
    }

    /// Parse array literal: [value1, value2, ...]
    fn parse_array_literal(&mut self) -> Result<PropertyValue> {
        self.consume(TokenType::LeftBracket, "Expected '['")?;
        let mut array = Vec::new();
        
        // Skip newlines after opening bracket
        while self.match_token(&TokenType::Newline) {}
        
        // Parse array elements
        while !self.check(&TokenType::RightBracket) && !self.is_at_end() {
            // Skip newlines
            if self.match_token(&TokenType::Newline) {
                continue;
            }
            
            // Parse element value
            let value = self.parse_value()?;
            array.push(value);
            
            // Optional comma
            self.match_token(&TokenType::Comma);
            
            // Skip newlines after comma
            while self.match_token(&TokenType::Newline) {}
        }
        
        self.consume(TokenType::RightBracket, "Expected ']' after array elements")?;
        Ok(PropertyValue::Array(array))
    }

    /// Parse object literal: { key: value, key: value }
    fn parse_object_literal(&mut self) -> Result<PropertyValue> {
        self.consume(TokenType::LeftBrace, "Expected '{'")?;
        let mut object = HashMap::new();
        
        // Skip newlines after opening brace
        while self.match_token(&TokenType::Newline) {}
        
        // Parse key-value pairs
        while !self.check(&TokenType::RightBrace) && !self.is_at_end() {
            // Skip newlines and comments
            if self.match_token(&TokenType::Newline) {
                continue;
            }
            if matches!(self.peek().token_type, TokenType::Comment(_)) {
                self.advance();
                continue;
            }
            
            // Parse key
            let key = match &self.peek().token_type {
                TokenType::Identifier(id) => {
                    let key = id.clone();
                    self.advance();
                    key
                }
                TokenType::String(s) => {
                    let key = s.clone();
                    self.advance();
                    key
                }
                _ => return Err(CompilerError::parse(
                    self.peek().token_type.to_string(),
                    self.peek().line,
                    "Expected property name (identifier or string) in object"
                )),
            };
            
            self.consume(TokenType::Colon, "Expected ':' after object property name")?;
            
            // Skip newlines after colon
            while self.match_token(&TokenType::Newline) {}
            
            // Parse value
            let value = self.parse_value()?;
            
            // Check for duplicate keys
            if object.contains_key(&key) {
                log::warn!("Duplicate key '{}' in object literal at line {}", key, self.previous().line);
            }
            
            object.insert(key, value);
            
            // Optional comma
            self.match_token(&TokenType::Comma);
            
            // Skip newlines after comma
            while self.match_token(&TokenType::Newline) {}
        }
        
        self.consume(TokenType::RightBrace, "Expected '}' after object properties")?;
        Ok(PropertyValue::Object(object))
    }

    // Utility methods

    fn is_property(&self) -> bool {
        // Check if current token is an identifier or keyword followed by a colon
        let is_property_name = match &self.peek().token_type {
            TokenType::Identifier(_) => true,
            // Allow keywords to be used as property names
            TokenType::Style => true,
            TokenType::Text => true,
            TokenType::Link => true,
            TokenType::Image => true,
            TokenType::EmbedView => true,
            TokenType::Canvas => true,
            TokenType::Button => true,
            TokenType::Input => true,
            TokenType::Container => true,
            TokenType::App => true,
            _ => false,
        };
        
        if is_property_name {
            // Look ahead to see if the next token is a colon
            if self.current + 1 < self.tokens.len() {
                matches!(self.tokens[self.current + 1].token_type, TokenType::Colon)
            } else {
                false
            }
        } else {
            false
        }
    }

    fn is_element_start(&self) -> bool {
        match &self.peek().token_type {
            // Known element types
            TokenType::App | TokenType::Container | TokenType::Text |
            TokenType::Link | TokenType::Image | TokenType::EmbedView | TokenType::Canvas | TokenType::Button | TokenType::Input => true,
            
            // For identifiers, check if they're followed by an opening brace (element)
            // rather than a colon (property)
            TokenType::Identifier(_) => {
                if self.current + 1 < self.tokens.len() {
                    matches!(self.tokens[self.current + 1].token_type, TokenType::LeftBrace)
                } else {
                    false
                }
            }
            
            _ => false
        }
    }
    
    /// Check if the current token matches and consume it if it does
    fn match_token(&mut self, token_type: &TokenType) -> bool {
        if self.check(token_type) {
            self.advance();
            true
        } else {
            false
        }
    }
    
    /// Check if the current token matches the given type (without consuming)
    fn check(&self, token_type: &TokenType) -> bool {
        if self.is_at_end() {
            false
        } else {
            std::mem::discriminant(&self.peek().token_type) == std::mem::discriminant(token_type)
        }
    }
    
    /// Consume a token of the given type or return an error
    fn consume(&mut self, token_type: TokenType, message: &str) -> Result<&Token> {
        if self.check(&token_type) {
            Ok(self.advance())
        } else {
            Err(CompilerError::parse(
                self.peek().token_type.to_string(),
                self.peek().line,
                message
            ))
        }
    }

    /// Get the current token without consuming it
    fn peek(&self) -> &Token {
        &self.tokens[self.current]
    }
    
    /// Get the previous token
    fn previous(&self) -> &Token {
        &self.tokens[self.current - 1]
    }
    
    /// Advance to the next token and return the previous one
    fn advance(&mut self) -> &Token {
        if !self.is_at_end() {
            self.current += 1;
        }
        self.previous()
    }
    
    /// Check if we've reached the end of tokens
    fn is_at_end(&self) -> bool {
        matches!(self.peek().token_type, TokenType::Eof)
    }
    
    /// Create a default App wrapper when no App element is present in the KRY file
    /// This matches the behavior of the renderer which creates an App wrapper automatically
    fn create_default_app_wrapper(&mut self, standalone_elements: Vec<AstNode>) -> Result<Option<Box<AstNode>>> {
        if standalone_elements.is_empty() {
            return Ok(None);
        }
        
        let element_count = standalone_elements.len();
        let default_app = AstNode::Element {
            element_type: "App".to_string(),
            properties: vec![
                AstProperty::new("window_title".to_string(), PropertyValue::String("\"Auto-generated App\"".to_string()), 1),
                AstProperty::new("window_width".to_string(), PropertyValue::Integer(800), 1),
                AstProperty::new("window_height".to_string(), PropertyValue::Integer(600), 1),
            ],
            pseudo_selectors: Vec::new(),
            children: standalone_elements, // Wrap all standalone elements as children
        };
        
        log::info!("Auto-created App wrapper for {} standalone elements", element_count);
        Ok(Some(Box::new(default_app)))
    }
    
    /// Parse function name pattern that may include template variables
    fn parse_function_name_pattern(&mut self) -> Result<String> {
        let mut name_parts = Vec::new();
        
        // Parse the function name which must use ${variable} syntax to avoid ambiguity
        // Examples: handle_${component_id}_click, toggle_${id}
        loop {
            match &self.peek().token_type {
                TokenType::Identifier(name) => {
                    name_parts.push(name.clone());
                    self.advance();
                }
                TokenType::Dollar => {
                    // Support both $var_name and ${var_name} syntax for variables in function names
                    self.advance(); // consume $
                    
                    match &self.peek().token_type {
                        TokenType::LeftBrace => {
                            // Handle ${variable} syntax
                            self.advance(); // consume '{'
                            if let TokenType::Identifier(var_name) = &self.advance().token_type {
                                name_parts.push(format!("${}", var_name));
                                self.consume(TokenType::RightBrace, "Expected '}' after variable name")?;
                            } else {
                                return Err(CompilerError::parse_legacy(
                                    self.previous().line,
                                    "Expected variable name after '${'",
                                ));
                            }
                        },
                        TokenType::Identifier(var_name) => {
                            // Handle $variable syntax
                            let var_name = var_name.clone();
                            name_parts.push(format!("${}", var_name));
                            self.advance(); // consume identifier
                        },
                        _ => {
                            return Err(CompilerError::parse_legacy(
                                self.peek().line,
                                "Expected variable name after '$' (use $variable or ${variable})"
                            ));
                        }
                    }
                }
                TokenType::LeftParen => {
                    // End of function name, start of parameters
                    break;
                }
                _ => {
                    return Err(CompilerError::parse_legacy(
                        self.peek().line,
                        "Expected identifier, ${variable}, or '(' in function name"
                    ));
                }
            }
        }
        
        if name_parts.is_empty() {
            return Err(CompilerError::parse_legacy(
                self.peek().line,
                "Function name cannot be empty"
            ));
        }
        
        Ok(name_parts.join(""))
    }
    
    /// Check if a property name is a CSS shorthand property
    fn is_shorthand_property(&self, key: &str) -> bool {
        matches!(key, "margin" | "padding" | "border")
    }
    
    /// Parse shorthand property values like "10 0" or "5 10 15 20"
    fn parse_shorthand_value(&mut self, key: &str) -> Result<PropertyValue> {
        let mut values = Vec::new();
        
        // Parse the first value
        let first_value = self.parse_single_value()?;
        values.push(first_value);
        
        // Parse additional space-separated values
        while !self.check(&TokenType::Newline) && 
              !self.check(&TokenType::Semicolon) && 
              !self.check(&TokenType::RightBrace) &&
              !self.is_at_end() &&
              self.is_value_token() {
            
            let value = self.parse_single_value()?;
            values.push(value);
        }
        
        // Convert to shorthand array
        Ok(PropertyValue::Array(values))
    }
    
    /// Parse a single value (number, string, etc.) for shorthand parsing
    fn parse_single_value(&mut self) -> Result<PropertyValue> {
        match &self.peek().token_type {
            TokenType::Number(n) => {
                let value = *n;
                self.advance();
                Ok(PropertyValue::Number(value))
            }
            TokenType::Integer(i) => {
                let value = *i;
                self.advance();
                Ok(PropertyValue::Integer(value))
            }
            TokenType::Pixels(p) => {
                let value = *p;
                self.advance();
                Ok(PropertyValue::Pixels(value))
            }
            TokenType::Em(e) => {
                let value = *e;
                self.advance();
                Ok(PropertyValue::Em(value))
            }
            TokenType::Rem(r) => {
                let value = *r;
                self.advance();
                Ok(PropertyValue::Rem(value))
            }
            TokenType::Percentage(p) => {
                let value = *p;
                self.advance();
                Ok(PropertyValue::Percentage(value))
            }
            TokenType::String(s) => {
                let value = format!("\"{}\"", s);
                self.advance();
                Ok(PropertyValue::String(value))
            }
            TokenType::Identifier(id) => {
                let value = id.clone();
                self.advance();
                Ok(PropertyValue::String(value))
            }
            TokenType::Color(c) => {
                let value = c.clone();
                self.advance();
                Ok(PropertyValue::Color(value))
            }
            _ => {
                Err(CompilerError::parse_legacy(
                    self.peek().line,
                    format!("Expected a value, but found {}", self.peek().token_type)
                ))
            }
        }
    }
    
    /// Check if the current token can be part of a value
    fn is_value_token(&self) -> bool {
        matches!(self.peek().token_type,
            TokenType::Number(_) | TokenType::Integer(_) | TokenType::Pixels(_) |
            TokenType::Em(_) | TokenType::Rem(_) | TokenType::Percentage(_) |
            TokenType::String(_) | TokenType::Identifier(_) | TokenType::Color(_)
        )
    }

    /// Extract template variables from a string value
    /// Finds patterns like $variable_name and returns a list of variable names
    fn extract_template_variables(&self, value: &str) -> Vec<String> {
        let re = Regex::new(r"\$([a-zA-Z_][a-zA-Z0-9_]*)").unwrap();
        let mut variables = Vec::new();
        
        for capture in re.captures_iter(value) {
            if let Some(var_name) = capture.get(1) {
                let name = var_name.as_str().to_string();
                if !variables.contains(&name) {
                    variables.push(name);
                }
            }
        }
        
        variables
    }
    
    /// Try to parse an expression with proper error handling and backtracking
    /// 
    fn try_parse_expression(&mut self) -> Result<Expression> {
        let start_position = self.current;
        let start_line = self.peek().line;
        
        // Try to parse as a full expression (including ternary)
        match self.parse_ternary_expression() {
            Ok(expr) => {
                // Validate the expression makes sense in this context
                if self.validate_expression_context(&expr) {
                    Ok(expr)
                } else {
                    // Invalid expression context, backtrack
                    self.current = start_position;
                    Err(CompilerError::parse(
                        self.peek().token_type.to_string(),
                        start_line,
                        "Expression not valid in this context"
                    ))
                }
            }
            Err(e) => {
                // Ensure we fully backtrack on any error
                self.current = start_position;
                
                // Check if we have tokens that look like they should be part of an expression
                if self.looks_like_expression() {
                    // Provide a more helpful error message
                    Err(CompilerError::parse(
                        self.peek().token_type.to_string(),
                        start_line,
                        &format!("Invalid expression syntax. {}", e)
                    ))
                } else {
                    // Not an expression, return the original error
                    Err(e)
                }
            }
        }
    }

    /// Parse ternary expression: condition ? true_value : false_value
    fn parse_ternary_expression(&mut self) -> Result<Expression> {
        let condition = self.parse_comparison_expression()?;
        
        if self.match_token(&TokenType::Question) {
            let true_value = self.parse_comparison_expression()?;
            self.consume(TokenType::Colon, "Expected ':' after true branch in ternary expression")?;
            // KEY FIX: Parse false branch as ternary (not comparison) for nesting
            let false_value = self.parse_ternary_expression()?;
            
            Ok(Expression::Ternary {
                condition: Box::new(condition),
                true_value: Box::new(true_value),
                false_value: Box::new(false_value),
            })
        } else {
            Ok(condition)
        }
    }

    /// Parse comparison expressions: ==, !=, <, <=, >, >=
    fn parse_comparison_expression(&mut self) -> Result<Expression> {
        // First try to parse a primary expression
        let left = match self.parse_primary_expression() {
            Ok(expr) => expr,
            Err(e) => {
                // If we can't parse a primary expression, return the error
                return Err(e);
            }
        };
        
        // Then check for comparison operators
        match &self.peek().token_type {
            TokenType::NotEquals => {
                self.advance();
                let right = self.parse_primary_expression()?;
                Ok(Expression::NotEquals(Box::new(left), Box::new(right)))
            }
            TokenType::EqualEquals => {
                self.advance();
                let right = self.parse_primary_expression()?;
                Ok(Expression::EqualEquals(Box::new(left), Box::new(right)))
            }
            TokenType::LessThan => {
                self.advance();
                let right = self.parse_primary_expression()?;
                Ok(Expression::LessThan(Box::new(left), Box::new(right)))
            }
            TokenType::LessThanOrEqual => {
                self.advance();
                let right = self.parse_primary_expression()?;
                Ok(Expression::LessThanOrEqual(Box::new(left), Box::new(right)))
            }
            TokenType::GreaterThan => {
                self.advance();
                let right = self.parse_primary_expression()?;
                Ok(Expression::GreaterThan(Box::new(left), Box::new(right)))
            }
            TokenType::GreaterThanOrEqual => {
                self.advance();
                let right = self.parse_primary_expression()?;
                Ok(Expression::GreaterThanOrEqual(Box::new(left), Box::new(right)))
            }
            _ => Ok(left) // No comparison operator, just return the primary expression
        }
    }

    /// Parse primary expressions: literals, variables
    /// 
fn parse_primary_expression(&mut self) -> Result<Expression> {
    match &self.peek().token_type {
        TokenType::String(s) => {
            let value = s.clone();
            self.advance();
            Ok(Expression::String(value))
        }
        TokenType::Number(n) => {
            let value = *n;
            self.advance();
            Ok(Expression::Number(value))
        }
        TokenType::Integer(i) => {
            let value = *i;
            self.advance();
            Ok(Expression::Integer(value))
        }
        TokenType::Boolean(b) => {
            let value = *b;
            self.advance();
            Ok(Expression::Boolean(value))
        }
        TokenType::Identifier(id) => {
            // Bare identifiers in expressions should be treated as strings
            // This handles cases like: style == red_style
            let value = id.clone();
            self.advance();
            Ok(Expression::String(value))
        }
        TokenType::Dollar => {
            // Parse variable reference
            self.advance(); // consume '$'
            
            match &self.peek().token_type {
                TokenType::LeftBrace => {
                    // ${variable} syntax
                    self.advance(); // consume '{'
                    match &self.peek().token_type {
                        TokenType::Identifier(var_name) => {
                            let value = var_name.clone();
                            self.advance();
                            self.consume(TokenType::RightBrace, "Expected '}' after variable name")?;
                            Ok(Expression::Variable(value))
                        }
                        _ => Err(CompilerError::parse(
                            self.peek().token_type.to_string(),
                            self.peek().line,
                            "Expected variable name in ${...}"
                        ))
                    }
                },
                TokenType::Identifier(var_name) => {
                    // $variable syntax
                    let value = var_name.clone();
                    self.advance();
                    Ok(Expression::Variable(value))
                },
                _ => Err(CompilerError::parse(
                    self.peek().token_type.to_string(),
                    self.peek().line,
                    "Expected variable name after '$'"
                ))
            }
        }
        TokenType::LeftParen => {
            self.advance(); // consume '('
            let expr = self.parse_ternary_expression()?;
            self.consume(TokenType::RightParen, "Expected ')' after expression")?;
            Ok(expr)
        }
        _ => {
            // Provide better error message with actual token info
            Err(CompilerError::parse(
                self.peek().token_type.to_string(),
                self.peek().line,
                &format!("Expected expression value (string, number, boolean, variable, or parenthesized expression), but found '{}'", 
                    self.peek().token_type)
            ))
        }
    }
}

    
    /// Parse @for loop: @for variable in collection ... @end or @for index, variable in collection ... @end
    fn parse_for(&mut self) -> Result<AstNode> {
        self.consume(TokenType::For, "Expected '@for'")?;
        
        // Parse first variable name
        let first_variable = match &self.peek().token_type {
            TokenType::Identifier(name) => {
                let var = name.clone();
                self.advance();
                var
            }
            _ => return Err(CompilerError::parse_legacy(
                self.peek().line,
                "Expected variable name after '@for'"
            )),
        };
        
        // Check if there's a comma (indicating index, variable syntax)
        let (index_variable, variable) = if self.match_token(&TokenType::Comma) {
            // Parse second variable name for @for index, variable syntax
            let second_variable = match &self.peek().token_type {
                TokenType::Identifier(name) => {
                    let var = name.clone();
                    self.advance();
                    var
                }
                _ => return Err(CompilerError::parse_legacy(
                    self.peek().line,
                    "Expected variable name after comma in '@for'"
                )),
            };
            (Some(first_variable), second_variable)
        } else {
            // Single variable syntax @for variable
            (None, first_variable)
        };
        
        // Parse 'in' keyword
        self.consume(TokenType::In, "Expected 'in' after variable name")?;
        
        // Parse collection (can be a property name, variable reference, or comma-separated list)
        let collection = match &self.peek().token_type {
            TokenType::Identifier(name) => {
                let col = name.clone();
                self.advance();
                col
            }
            TokenType::String(s) => {
                let col = s.clone();
                self.advance();
                col
            }
            TokenType::Dollar => {
                // Handle $variable syntax
                self.advance(); // consume $
                match &self.peek().token_type {
                    TokenType::Identifier(var_name) => {
                        let col = var_name.clone(); // Store the variable name without $
                        self.advance();
                        col
                    }
                    _ => return Err(CompilerError::parse_legacy(
                        self.peek().line,
                        "Expected variable name after '$'"
                    )),
                }
            }
            _ => return Err(CompilerError::parse_legacy(
                self.peek().line,
                "Expected collection name, variable reference, or string after 'in'"
            )),
        };
        
        // Parse body until @end
        let mut body = Vec::new();
        while !self.check(&TokenType::End) && !self.is_at_end() {
            if self.match_token(&TokenType::Newline) {
                continue;
            }
            if matches!(self.peek().token_type, TokenType::Comment(_)) {
                self.advance();
                continue;
            }
            
            if self.is_element_start() {
                body.push(self.parse_element()?);
            } else if matches!(self.peek().token_type, TokenType::For) {
                body.push(self.parse_for()?);
            } else if matches!(self.peek().token_type, TokenType::If) {
                body.push(self.parse_if()?);
            } else {
                return Err(CompilerError::parse_legacy(
                    self.peek().line,
                    format!("Unexpected token in @for body: {}", self.peek().token_type)
                ));
            }
        }
        
        self.consume(TokenType::End, "Expected '@end' after @for body")?;
        
        Ok(AstNode::For {
            index_variable,
            variable,
            collection,
            body,
        })
    }
    
    /// Parse @if conditional: @if condition ... [@elif condition ...] [@else ...] @end
    fn parse_if(&mut self) -> Result<AstNode> {
        self.consume(TokenType::If, "Expected '@if'")?;
        
        // Parse condition
        let condition = match &self.peek().token_type {
            TokenType::Identifier(name) => {
                let cond = name.clone();
                self.advance();
                cond
            }
            TokenType::String(s) => {
                let cond = s.clone();
                self.advance();
                cond
            }
            _ => return Err(CompilerError::parse_legacy(
                self.peek().line,
                "Expected condition after '@if'"
            )),
        };
        
        // Parse then body
        let mut then_body = Vec::new();
        while !self.check(&TokenType::Elif) && !self.check(&TokenType::Else) && !self.check(&TokenType::End) && !self.is_at_end() {
            if self.match_token(&TokenType::Newline) {
                continue;
            }
            if matches!(self.peek().token_type, TokenType::Comment(_)) {
                self.advance();
                continue;
            }
            
            if self.is_element_start() {
                then_body.push(self.parse_element()?);
            } else if matches!(self.peek().token_type, TokenType::For) {
                then_body.push(self.parse_for()?);
            } else if matches!(self.peek().token_type, TokenType::If) {
                then_body.push(self.parse_if()?);
            } else {
                return Err(CompilerError::parse_legacy(
                    self.peek().line,
                    format!("Unexpected token in @if body: {}", self.peek().token_type)
                ));
            }
        }
        
        // Parse elif branches
        let mut elif_branches = Vec::new();
        while self.check(&TokenType::Elif) {
            self.advance(); // consume @elif
            
            // Parse elif condition
            let elif_condition = match &self.peek().token_type {
                TokenType::Identifier(name) => {
                    let cond = name.clone();
                    self.advance();
                    cond
                }
                TokenType::String(s) => {
                    let cond = s.clone();
                    self.advance();
                    cond
                }
                _ => return Err(CompilerError::parse_legacy(
                    self.peek().line,
                    "Expected condition after '@elif'"
                )),
            };
            
            // Parse elif body
            let mut elif_body = Vec::new();
            while !self.check(&TokenType::Elif) && !self.check(&TokenType::Else) && !self.check(&TokenType::End) && !self.is_at_end() {
                if self.match_token(&TokenType::Newline) {
                    continue;
                }
                if matches!(self.peek().token_type, TokenType::Comment(_)) {
                    self.advance();
                    continue;
                }
                
                if self.is_element_start() {
                    elif_body.push(self.parse_element()?);
                } else if matches!(self.peek().token_type, TokenType::For) {
                    elif_body.push(self.parse_for()?);
                } else if matches!(self.peek().token_type, TokenType::If) {
                    elif_body.push(self.parse_if()?);
                } else {
                    return Err(CompilerError::parse_legacy(
                        self.peek().line,
                        format!("Unexpected token in @elif body: {}", self.peek().token_type)
                    ));
                }
            }
            
            elif_branches.push((elif_condition, elif_body));
        }
        
        // Parse optional else branch
        let else_body = if self.check(&TokenType::Else) {
            self.advance(); // consume @else
            
            let mut else_body = Vec::new();
            while !self.check(&TokenType::End) && !self.is_at_end() {
                if self.match_token(&TokenType::Newline) {
                    continue;
                }
                if matches!(self.peek().token_type, TokenType::Comment(_)) {
                    self.advance();
                    continue;
                }
                
                if self.is_element_start() {
                    else_body.push(self.parse_element()?);
                } else if matches!(self.peek().token_type, TokenType::For) {
                    else_body.push(self.parse_for()?);
                } else if matches!(self.peek().token_type, TokenType::If) {
                    else_body.push(self.parse_if()?);
                } else {
                    return Err(CompilerError::parse_legacy(
                        self.peek().line,
                        format!("Unexpected token in @else body: {}", self.peek().token_type)
                    ));
                }
            }
            
            Some(else_body)
        } else {
            None
        };
        
        self.consume(TokenType::End, "Expected '@end' after @if")?;
        
        Ok(AstNode::If {
            condition,
            then_body,
            elif_branches,
            else_body,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::compiler::frontend::lexer::Lexer;
    
    #[test]
    fn test_parse_simple_app() {
        let source = r#"
            App {
                window_title: "Test App"
                Text {
                    text: "Hello World"
                }
            }
        "#;
        
        let mut lexer = Lexer::new(source, "test.kry".to_string());
        let tokens = lexer.tokenize().unwrap();
        let mut parser = Parser::new(tokens);
        let ast = parser.parse().unwrap();
        
        match ast {
            AstNode::File { app: Some(app), .. } => {
                match app.as_ref() {
                    AstNode::Element { element_type, .. } => {
                        assert_eq!(element_type, "App");
                    }
                    _ => panic!("Expected App element"),
                }
            }
            _ => panic!("Expected File with App"),
        }
    }
    
    #[test]
    fn test_auto_app_wrapper_creation() {
        // Test that standalone elements are automatically wrapped in an App
        let source = r#"
            Text {
                text: "Hello World"
                font_size: 18
            }
            
            Container {
                layout: "column"
                
                Button {
                    text: "Click Me"
                }
            }
        "#;
        
        let mut lexer = Lexer::new(source, "test.kry".to_string());
        let tokens = lexer.tokenize().unwrap();
        let mut parser = Parser::new(tokens);
        let ast = parser.parse().unwrap();
        
        // Verify that an App element was auto-created
        match ast {
            AstNode::File { app: Some(app), .. } => {
                match app.as_ref() {
                    AstNode::Element { element_type, properties, children, .. } => {
                        assert_eq!(element_type, "App");
                        
                        // Check for default App properties
                        assert!(properties.iter().any(|p| p.key == "window_title"));
                        assert!(properties.iter().any(|p| p.key == "window_width"));
                        assert!(properties.iter().any(|p| p.key == "window_height"));
                        
                        // Check that standalone elements were wrapped as children
                        assert_eq!(children.len(), 2); // Text and Container
                        
                        // Verify first child is Text
                        if let AstNode::Element { element_type, .. } = &children[0] {
                            assert_eq!(element_type, "Text");
                        } else {
                            panic!("Expected first child to be Text element");
                        }
                        
                        // Verify second child is Container with its own children
                        if let AstNode::Element { element_type, children: container_children, .. } = &children[1] {
                            assert_eq!(element_type, "Container");
                            assert_eq!(container_children.len(), 1); // Button
                            
                            if let AstNode::Element { element_type, .. } = &container_children[0] {
                                assert_eq!(element_type, "Button");
                            } else {
                                panic!("Expected Container child to be Button element");
                            }
                        } else {
                            panic!("Expected second child to be Container element");
                        }
                    }
                    _ => panic!("Expected auto-generated App element"),
                }
            }
            _ => panic!("Expected File with auto-generated App"),
        }
    }

    #[test]
    fn test_no_auto_app_wrapper_when_app_exists() {
        // Test that no auto-wrapper is created when App already exists
        let source = r#"
            App {
                window_title: "Existing App"
                
                Text {
                    text: "Hello World"
                }
            }
        "#;
        
        let mut lexer = Lexer::new(source, "test.kry".to_string());
        let tokens = lexer.tokenize().unwrap();
        let mut parser = Parser::new(tokens);
        let ast = parser.parse().unwrap();
        
        // Verify the existing App is preserved, not wrapped
        match ast {
            AstNode::File { app: Some(app), .. } => {
                match app.as_ref() {
                    AstNode::Element { element_type, properties, children, .. } => {
                        assert_eq!(element_type, "App");
                        
                        // Should have the original window_title, not auto-generated
                        let title_prop = properties.iter().find(|p| p.key == "window_title").unwrap();
                        assert_eq!(title_prop.value.to_string(), "\"Existing App\"");
                        
                        // Should have one child (the Text element)
                        assert_eq!(children.len(), 1);
                    }
                    _ => panic!("Expected original App element"),
                }
            }
            _ => panic!("Expected File with original App"),
        }
    }

    #[test]
    fn test_parse_component() {
        let source = r#"
            Define Button {
                Properties {
                    text: String = "Click me"
                    enabled: Bool = true
                }
                Container {
                    Text { text: $text }
                }
            }
        "#;
        
        let mut lexer = Lexer::new(source, "test.kry".to_string());
        let tokens = lexer.tokenize().unwrap();
        let mut parser = Parser::new(tokens);
        let ast = parser.parse().unwrap();
        
        match ast {
            AstNode::File { components, .. } => {
                assert_eq!(components.len(), 1);
                match &components[0] {
                    AstNode::Component { name, properties, .. } => {
                        assert_eq!(name, "Button");
                        assert_eq!(properties.len(), 2);
                    }
                    _ => panic!("Expected Component"),
                }
            }
            _ => panic!("Expected File with components"),
        }
    }

    #[test]
    fn test_template_variable_extraction() {
        let parser = Parser::new(Vec::new());
        
        // Test simple template variable
        let vars = parser.extract_template_variables("Count: $counter_value");
        assert_eq!(vars, vec!["counter_value"]);
        
        // Test multiple template variables
        let vars = parser.extract_template_variables("Hello $user_name, count: $counter_value");
        assert_eq!(vars, vec!["user_name", "counter_value"]);
        
        // Test no template variables
        let vars = parser.extract_template_variables("Plain text");
        assert!(vars.is_empty());
        
        // Test duplicate variables (should only appear once)
        let vars = parser.extract_template_variables("$var and $var again");
        assert_eq!(vars, vec!["var"]);
    }

    #[test]
    fn test_parse_input_with_type() {
        let source = r#"
            App {
                Input {
                    type: "text"
                    placeholder: "Enter text"
                    value: "default"
                }
                Input {
                    type: "checkbox"
                    checked: true
                    text: "Accept terms"
                }
                Input {
                    type: "range"
                    min: 0
                    max: 100
                    step: 1
                    value: 50
                }
            }
        "#;
        
        let mut lexer = Lexer::new(source, "test.kry".to_string());
        let tokens = lexer.tokenize().unwrap();
        let mut parser = Parser::new(tokens);
        let ast = parser.parse().unwrap();
        
        match ast {
            AstNode::File { app: Some(app), .. } => {
                match app.as_ref() {
                    AstNode::Element { element_type, children, .. } => {
                        assert_eq!(element_type, "App");
                        assert_eq!(children.len(), 3);
                        
                        // Check first Input (text type)
                        if let AstNode::Element { element_type, properties, .. } = &children[0] {
                            assert_eq!(element_type, "Input");
                            assert!(properties.iter().any(|p| p.key == "type" && p.value.to_string() == "\"text\""));
                            assert!(properties.iter().any(|p| p.key == "placeholder"));
                            assert!(properties.iter().any(|p| p.key == "value"));
                        } else {
                            panic!("Expected first child to be Input element");
                        }
                        
                        // Check second Input (checkbox type)
                        if let AstNode::Element { element_type, properties, .. } = &children[1] {
                            assert_eq!(element_type, "Input");
                            assert!(properties.iter().any(|p| p.key == "type" && p.value.to_string() == "\"checkbox\""));
                            assert!(properties.iter().any(|p| p.key == "checked"));
                            assert!(properties.iter().any(|p| p.key == "text"));
                        } else {
                            panic!("Expected second child to be Input element");
                        }
                        
                        // Check third Input (range type)
                        if let AstNode::Element { element_type, properties, .. } = &children[2] {
                            assert_eq!(element_type, "Input");
                            assert!(properties.iter().any(|p| p.key == "type" && p.value.to_string() == "\"range\""));
                            assert!(properties.iter().any(|p| p.key == "min"));
                            assert!(properties.iter().any(|p| p.key == "max"));
                            assert!(properties.iter().any(|p| p.key == "step"));
                            assert!(properties.iter().any(|p| p.key == "value"));
                        } else {
                            panic!("Expected third child to be Input element");
                        }
                    }
                    _ => panic!("Expected App element"),
                }
            }
            _ => panic!("Expected File with App"),
        }
    }
}

