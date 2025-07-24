//! Lexical analysis for KRY source code

use crate::error::{CompilerError, Result};
use regex::Regex;
use std::fmt;

#[derive(Debug, Clone, PartialEq)]
pub enum TokenType {
    // Keywords
    App,
    Container,
    Text,
    Link,
    Image,
    EmbedView,  // Renamed from ForeignView
    Canvas,     // Core rendering primitive with universal drawing API
    Button,
    Input,
    List,
    Grid,
    Scrollable,
    Tabs,
    Video,
    Style,
    Font,
    Define,
    Properties,
    
    // Directives
    Include,
    Variables,
    Variable,
    Script,
    Function,
    
    // Template control flow
    For,
    If,
    Elif,
    Else,
    End,
    In,
    
    // Operators and punctuation
    LeftBrace,    // {
    RightBrace,   // }
    LeftBracket,  // [
    RightBracket, // ]
    LeftParen,    // (
    RightParen,   // )
    Colon,        // :
    Semicolon,    // ;
    Comma,        // ,
    Equals,       // =
    Ampersand,    // &
    Dollar,       // $
    Dot,          // .
    
    // Comparison operators
    NotEquals,    // !=
    EqualEquals,  // ==
    LessThan,     // <
    LessThanOrEqual, // <=
    GreaterThan,  // >
    GreaterThanOrEqual, // >=
    
    // Ternary operator
    Question,     // ?
    
    // Pseudo-selector
    PseudoSelector(String), // &:hover, &:active, etc.
    
    // Literals
    String(String),
    Number(f64),
    Integer(i64),
    Percentage(f64), // 50%, 100%, etc.
    Boolean(bool),
    Color(String), // #RGB, #RRGGBB, #RRGGBBAA
    Identifier(String),
    ScriptContent(String), // Raw script content inside @function or @script blocks
    
    // Unit types for transform and CSS values
    Pixels(f64),      // 10px
    Em(f64),          // 1.5em
    Rem(f64),         // 2rem
    ViewportWidth(f64), // 50vw
    ViewportHeight(f64), // 100vh
    Degrees(f64),     // 45deg
    Radians(f64),     // 1.57rad
    Turns(f64),       // 0.25turn
    
    // Special
    Newline,
    Comment(String),
    Eof,
}

#[derive(Debug, Clone)]
pub struct Token {
    pub token_type: TokenType,
    pub line: usize,
    pub column: usize,
    pub filename: String,
}



impl fmt::Display for TokenType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            // Keywords and Elements
            TokenType::App => write!(f, "App"),
            TokenType::Container => write!(f, "Container"),
            TokenType::Text => write!(f, "Text"),
            TokenType::Link => write!(f, "Link"),
            TokenType::Image => write!(f, "Image"),
            TokenType::EmbedView => write!(f, "EmbedView"),
            TokenType::Canvas => write!(f, "Canvas"),
            TokenType::Button => write!(f, "Button"),
            TokenType::Input => write!(f, "Input"),
            TokenType::List => write!(f, "List"),
            TokenType::Grid => write!(f, "Grid"),
            TokenType::Scrollable => write!(f, "Scrollable"),
            TokenType::Tabs => write!(f, "Tabs"),
            TokenType::Video => write!(f, "Video"),
            
            // Style and Font
            TokenType::Style => write!(f, "style"),
            TokenType::Font => write!(f, "font"),
            
            // Component Definition
            TokenType::Define => write!(f, "Define"),
            TokenType::Properties => write!(f, "Properties"),
            
            // Directives
            TokenType::Include => write!(f, "@include"),
            TokenType::Variables => write!(f, "@variables"),
            TokenType::Variable => write!(f, "@variable"),
            TokenType::Script => write!(f, "@script"),
            TokenType::Function => write!(f, "@function"),
            
            // Control Flow
            TokenType::For => write!(f, "@for"),
            TokenType::If => write!(f, "@if"),
            TokenType::Elif => write!(f, "@elif"),
            TokenType::Else => write!(f, "@else"),
            TokenType::End => write!(f, "@end"),
            TokenType::In => write!(f, "in"),
            
            // Delimiters
            TokenType::LeftBrace => write!(f, "{{"),
            TokenType::RightBrace => write!(f, "}}"),
            TokenType::LeftBracket => write!(f, "["),
            TokenType::RightBracket => write!(f, "]"),
            TokenType::LeftParen => write!(f, "("),
            TokenType::RightParen => write!(f, ")"),
            
            // Operators and Symbols
            TokenType::Colon => write!(f, ":"),
            TokenType::Semicolon => write!(f, ";"),
            TokenType::Comma => write!(f, ","),
            TokenType::Equals => write!(f, "="),
            TokenType::NotEquals => write!(f, "!="),
            TokenType::EqualEquals => write!(f, "=="),
            TokenType::LessThan => write!(f, "<"),
            TokenType::LessThanOrEqual => write!(f, "<="),
            TokenType::GreaterThan => write!(f, ">"),
            TokenType::GreaterThanOrEqual => write!(f, ">="),
            TokenType::Question => write!(f, "?"),
            TokenType::Ampersand => write!(f, "&"),
            TokenType::Dollar => write!(f, "$"),
            TokenType::Dot => write!(f, "."),
            
            // Values with content - use concise format for better error messages
            TokenType::PseudoSelector(state) => write!(f, "&:{}", state),
            TokenType::String(s) => write!(f, "string \"{}\"", s),
            TokenType::Number(n) => write!(f, "number {}", n),
            TokenType::Integer(i) => write!(f, "integer {}", i),
            TokenType::Boolean(b) => write!(f, "boolean {}", b),
            TokenType::Color(c) => write!(f, "color {}", c),
            TokenType::Identifier(id) => write!(f, "identifier '{}'", id),
            TokenType::ScriptContent(content) => write!(f, "script content ({}...)", &content.chars().take(20).collect::<String>()),
            
            // Units
            TokenType::Pixels(p) => write!(f, "{}px", p),
            TokenType::Em(e) => write!(f, "{}em", e),
            TokenType::Rem(r) => write!(f, "{}rem", r),
            TokenType::ViewportWidth(vw) => write!(f, "{}vw", vw),
            TokenType::ViewportHeight(vh) => write!(f, "{}vh", vh),
            TokenType::Percentage(p) => write!(f, "{}%", p),
            TokenType::Degrees(d) => write!(f, "{}deg", d),
            TokenType::Radians(r) => write!(f, "{}rad", r),
            TokenType::Turns(t) => write!(f, "{}turn", t),
            
            // Special
            TokenType::Newline => write!(f, "newline"),
            TokenType::Comment(c) => write!(f, "comment: {}", c),
            TokenType::Eof => write!(f, "EOF"),
        }
    }
}

pub struct Lexer {
    input: Vec<char>,
    position: usize,
    line: usize,
    column: usize,
    filename: String,
    
    // Store original source for script content reading
    source: String,
    
    // Regex patterns for complex tokens
    color_regex: Regex,
    identifier_regex: Regex,
    pseudo_selector_regex: Regex,
    
    // Source mapping for better error reporting
    source_map: Option<crate::error::SourceMap>,
}

impl Lexer {
    pub fn new(input: &str, filename: String) -> Self {
        Self {
            input: input.chars().collect(),
            position: 0,
            line: 1,
            column: 1,
            filename,
            source: input.to_string(),
            color_regex: Regex::new(r"^#[0-9A-Fa-f]{3,8}$").unwrap(),
            identifier_regex: Regex::new(r"^[a-zA-Z_][a-zA-Z0-9_]*$").unwrap(),
            pseudo_selector_regex: Regex::new(r"^&:(hover|active|focus|disabled|checked)$").unwrap(),
            source_map: None,
        }
    }

    pub fn new_with_source_map(input: &str, filename: String, source_map: crate::error::SourceMap) -> Self {
        Self {
            input: input.chars().collect(),
            position: 0,
            line: 1,
            column: 1,
            filename,
            source: input.to_string(),
            color_regex: Regex::new(r"^#[0-9A-Fa-f]{3,8}$").unwrap(),
            identifier_regex: Regex::new(r"^[a-zA-Z_][a-zA-Z0-9_]*$").unwrap(),
            pseudo_selector_regex: Regex::new(r"^&:(hover|active|focus|disabled|checked)$").unwrap(),
            source_map: Some(source_map),
        }
    }

    /// Get the original source location for error reporting
    fn get_source_location(&self) -> (String, usize) {
        if let Some(ref source_map) = self.source_map {
            source_map.resolve_location(self.line, &self.filename)
        } else {
            (self.filename.clone(), self.line)
        }
    }

    /// Create a parse error with proper source location
    fn parse_error(&self, message: impl Into<String>) -> CompilerError {
        let (file, line) = self.get_source_location();
        log::debug!("Parse error at combined line {}, mapped to {}:{}", self.line, file, line);
        CompilerError::parse(file, line, message)
    }
    
    pub fn tokenize(&mut self) -> Result<Vec<Token>> {
        let mut tokens = Vec::new();
        let mut pending_script_function = false;
        let mut pending_script_block = false;
        
        while !self.is_at_end() {
            if let Some(token) = self.next_token()? {
                // Check if we just encountered @function or @script
                if matches!(token.token_type, TokenType::Function) {
                    pending_script_function = true;
                    tokens.push(token);
                    continue;
                }
                
                if matches!(token.token_type, TokenType::Script) {
                    pending_script_block = true;
                    tokens.push(token);
                    continue;
                }
                
                // If we're pending a script function and hit opening brace, switch to script mode
                if pending_script_function && matches!(token.token_type, TokenType::LeftBrace) {
                    pending_script_function = false;
                    tokens.push(token); // Add the opening brace
                    
                    // Now read script content as raw text
                    let script_content = self.read_script_content()?;
                    tokens.push(Token {
                        token_type: TokenType::ScriptContent(script_content),
                        line: self.line,
                        column: self.column,
                        filename: self.filename.clone(),
                    });
                    
                    // Add the closing brace token
                    tokens.push(Token {
                        token_type: TokenType::RightBrace,
                        line: self.line,
                        column: self.column,
                        filename: self.filename.clone(),
                    });
                    
                    continue;
                }
                
                // If we're pending a script block and hit opening brace, switch to script mode
                if pending_script_block && matches!(token.token_type, TokenType::LeftBrace) {
                    pending_script_block = false;
                    tokens.push(token); // Add the opening brace
                    
                    // Now read script content as raw text
                    let script_content = self.read_script_content()?;
                    tokens.push(Token {
                        token_type: TokenType::ScriptContent(script_content),
                        line: self.line,
                        column: self.column,
                        filename: self.filename.clone(),
                    });
                    
                    // Add the closing brace token
                    tokens.push(Token {
                        token_type: TokenType::RightBrace,
                        line: self.line,
                        column: self.column,
                        filename: self.filename.clone(),
                    });
                    
                    continue;
                }
                
                // Reset pending flags if we hit anything else that indicates we're not in a script signature
                if pending_script_function && !matches!(token.token_type, TokenType::String(_) | TokenType::Identifier(_) | TokenType::LeftParen | TokenType::RightParen | TokenType::Comma | TokenType::Newline) {
                    pending_script_function = false;
                }
                
                if pending_script_block && !matches!(token.token_type, TokenType::String(_) | TokenType::Newline) {
                    pending_script_block = false;
                }
                
                tokens.push(token);
            }
        }
        
        tokens.push(Token {
            token_type: TokenType::Eof,
            line: self.line,
            column: self.column,
            filename: self.filename.clone(),
        });
        
        Ok(tokens)
    }
    
    fn next_token(&mut self) -> Result<Option<Token>> {
        self.skip_whitespace_except_newlines();
        
        if self.is_at_end() {
            return Ok(None);
        }
        
        let start_line = self.line;
        let start_column = self.column;
        let ch = self.advance();
        
        let token_type = match ch {
            '\n' => {
                self.line += 1;
                self.column = 1;
                TokenType::Newline
            }
            '\r' => {
                if self.peek() == Some('\n') {
                    self.advance(); // consume \n
                }
                self.line += 1;
                self.column = 1;
                TokenType::Newline
            }
            '{' => TokenType::LeftBrace,
            '}' => TokenType::RightBrace,
            '[' => TokenType::LeftBracket,
            ']' => TokenType::RightBracket,
            '(' => TokenType::LeftParen,
            ')' => TokenType::RightParen,
            ':' => TokenType::Colon,
            ';' => TokenType::Semicolon,
            ',' => TokenType::Comma,
            '=' => {
                // Check for == operator
                if self.peek() == Some('=') {
                    self.advance(); // consume second '='
                    TokenType::EqualEquals
                } else {
                    TokenType::Equals
                }
            }
            '!' => {
                // Check for != operator
                if self.peek() == Some('=') {
                    self.advance(); // consume '='
                    TokenType::NotEquals
                } else {
                    return Err(self.parse_error(format!("Unexpected character: '{}'", ch)));
                }
            }
            '<' => {
                // Check for <= operator
                if self.peek() == Some('=') {
                    self.advance(); // consume '='
                    TokenType::LessThanOrEqual
                } else {
                    TokenType::LessThan
                }
            }
            '>' => {
                // Check for >= operator
                if self.peek() == Some('=') {
                    self.advance(); // consume '='
                    TokenType::GreaterThanOrEqual
                } else {
                    TokenType::GreaterThan
                }
            }
            '?' => TokenType::Question,
            '$' => TokenType::Dollar,
            '.' => {
                // Check if this is part of a number (e.g., .5) or standalone dot
                if self.peek().map_or(false, |c| c.is_ascii_digit()) {
                    // This is a decimal number starting with .
                    let number_str = self.read_number(ch)?;
                    self.parse_number_with_unit(number_str)?
                } else {
                    TokenType::Dot
                }
            }
            '&' => {
                if self.peek() == Some(':') {
                    // Handle pseudo-selectors like &:hover
                    let pseudo = self.read_pseudo_selector()?;
                    if self.pseudo_selector_regex.is_match(&pseudo) {
                        // Extract the state part (everything after &:)
                        let state = pseudo.strip_prefix("&:").unwrap_or("").to_string();
                        TokenType::PseudoSelector(state)
                    } else {
                        return Err(self.parse_error(
                            format!("Invalid pseudo-selector: {}", pseudo)
                        ));
                    }
                } else {
                    TokenType::Ampersand
                }
            }
            '#' => {
                if self.is_hex_digit(self.peek()) {
                    // Color literal
                    let color = self.read_color()?;
                    TokenType::Color(color)
                } else {
                    // Comment
                    let comment = self.read_comment();
                    TokenType::Comment(comment)
                }
            }
            '/' => {
                if self.peek() == Some('/') {
                    // Double-slash comment
                    self.advance(); // consume second '/'
                    let comment = self.read_comment();
                    TokenType::Comment(comment)
                } else {
                    return Err(self.parse_error(
                        format!("Unexpected character: '{}' (single slash not supported)", ch)
                    ));
                }
            }
            '"' => {
                let string_value = self.read_string()?;
                TokenType::String(string_value)
            }
            '@' => {
                let directive = self.read_directive()?;
                match directive.as_str() {
                    "@include" => TokenType::Include,
                    "@variables" | "@vars" => TokenType::Variables,
                    "@variable" | "@var" => TokenType::Variable,
                    "@script" => {
                        // For @script, we need to read the script content specially
                        TokenType::Script
                    },
                    "@function" | "@method" | "@func" => {
                        // For @function/@method/@func, we need to read the script content specially
                        TokenType::Function
                    },
                    "@for" => TokenType::For,
                    "@if" => TokenType::If,
                    "@elif" => TokenType::Elif,
                    "@else" => TokenType::Else,
                    "@end" => TokenType::End,
                    _ => return Err(self.parse_error(
                        format!("Unknown directive: {}", directive)
                    )),
                }
            }
            ch if ch.is_ascii_digit() || (ch == '-' && self.peek().map_or(false, |c| c.is_ascii_digit())) => {
                let number_str = self.read_number(ch)?;
                self.parse_number_with_unit(number_str)?
            }
            '-' => {
                // Handle minus sign (for script content like Lua comments --)
                TokenType::Identifier("-".to_string())
            }
            ch if ch.is_alphabetic() || ch == '_' => {
                let identifier = self.read_identifier(ch)?;
                self.identify_keyword_or_identifier(identifier)
            }
            _ => {
                return Err(self.parse_error(
                    format!("Unexpected character: '{}'", ch)
                ));
            }
        };
        
        Ok(Some(Token {
            token_type,
            line: start_line,
            column: start_column,
            filename: self.filename.clone(),
        }))
    }
    
    fn skip_whitespace_except_newlines(&mut self) {
        while let Some(ch) = self.peek() {
            if ch.is_whitespace() && ch != '\n' && ch != '\r' {
                self.advance();
            } else {
                break;
            }
        }
    }
    
    fn advance(&mut self) -> char {
        if self.position < self.input.len() {
            let ch = self.input[self.position];
            self.position += 1;
            if ch != '\n' && ch != '\r' {
                self.column += 1;
            }
            ch
        } else {
            '\0'
        }
    }
    
    fn peek(&self) -> Option<char> {
        if self.position < self.input.len() {
            Some(self.input[self.position])
        } else {
            None
        }
    }
    
    fn peek_next(&self) -> Option<char> {
        if self.position + 1 < self.input.len() {
            Some(self.input[self.position + 1])
        } else {
            None
        }
    }
    
    fn is_at_end(&self) -> bool {
        self.position >= self.input.len()
    }
    
    fn is_hex_digit(&self, ch: Option<char>) -> bool {
        ch.map_or(false, |c| c.is_ascii_hexdigit())
    }
    
    fn read_string(&mut self) -> Result<String> {
        let mut value = String::new();
        let mut escaped = false;
        
        while let Some(ch) = self.peek() {
            if escaped {
                match ch {
                    'n' => value.push('\n'),
                    't' => value.push('\t'),
                    'r' => value.push('\r'),
                    '\\' => value.push('\\'),
                    '"' => value.push('"'),
                    '\'' => value.push('\''),
                    _ => {
                        value.push('\\');
                        value.push(ch);
                    }
                }
                escaped = false;
                self.advance();
            } else if ch == '\\' {
                escaped = true;
                self.advance();
            } else if ch == '"' {
                self.advance(); // consume closing quote
                return Ok(value);
            } else if ch == '\n' || ch == '\r' {
                return Err(self.parse_error(
                    "Unterminated string literal"
                ));
            } else {
                value.push(ch);
                self.advance();
            }
        }
        
        Err(self.parse_error(
            "Unterminated string literal"
        ))
    }
    
    fn read_color(&mut self) -> Result<String> {
        let mut color = String::from("#");
        
        while let Some(ch) = self.peek() {
            if ch.is_ascii_hexdigit() {
                color.push(ch);
                self.advance();
            } else {
                break;
            }
        }
        
        // Validate color format
        if !self.color_regex.is_match(&color) {
            return Err(self.parse_error(
                format!("Invalid color format: {}", color)
            ));
        }
        
        // Validate length (3, 4, 6, or 8 hex digits after #)
        let hex_part = &color[1..];
        match hex_part.len() {
            3 | 4 | 6 | 8 => Ok(color),
            _ => Err(self.parse_error(
                format!("Invalid color format: {} (expected 3, 4, 6, or 8 hex digits)", color)
            )),
        }
    }
    
    fn read_comment(&mut self) -> String {
        let mut comment = String::new();
        
        while let Some(ch) = self.peek() {
            if ch == '\n' || ch == '\r' {
                break;
            }
            comment.push(ch);
            self.advance();
        }
        
        comment
    }
    
    fn read_directive(&mut self) -> Result<String> {
        let mut directive = String::from("@");
        
        while let Some(ch) = self.peek() {
            if ch.is_alphanumeric() || ch == '_' {
                directive.push(ch);
                self.advance();
            } else {
                break;
            }
        }
        
        Ok(directive)
    }
    
    fn read_pseudo_selector(&mut self) -> Result<String> {
        let mut pseudo = String::from("&");
        
        // We already know the next char is ':'
        pseudo.push(self.advance());
        
        // Read the selector name
        while let Some(ch) = self.peek() {
            if ch.is_alphanumeric() || ch == '_' {
                pseudo.push(ch);
                self.advance();
            } else {
                break;
            }
        }
        
        Ok(pseudo)
    }
    
    fn read_number(&mut self, first_char: char) -> Result<String> {
        let mut number = String::new();
        number.push(first_char);
        
        let mut has_dot = false;
        
        while let Some(ch) = self.peek() {
            if ch.is_ascii_digit() {
                number.push(ch);
                self.advance();
            } else if ch == '.' && !has_dot {
                has_dot = true;
                number.push(ch);
                self.advance();
                
                // Must have digit after decimal point
                if !self.peek().map_or(false, |c| c.is_ascii_digit()) {
                    return Err(self.parse_error(
                        format!("Invalid number format: {}", number)
                    ));
                }
            } else {
                break;
            }
        }
        
        Ok(number)
    }
    
    /// Parse a number with optional unit suffix (px, em, rem, deg, etc.)
    fn parse_number_with_unit(&mut self, number_str: String) -> Result<TokenType> {
        let value = number_str.parse::<f64>().map_err(|_| {
            self.parse_error(format!("Invalid number: {}", number_str))
        })?;
        
        // Check for unit suffix
        let unit = self.read_unit_suffix();
        
        match unit.as_str() {
            "%" => Ok(TokenType::Percentage(value)),
            "px" => Ok(TokenType::Pixels(value)),
            "em" => Ok(TokenType::Em(value)),
            "rem" => Ok(TokenType::Rem(value)),
            "vw" => Ok(TokenType::ViewportWidth(value)),
            "vh" => Ok(TokenType::ViewportHeight(value)),
            "deg" => Ok(TokenType::Degrees(value)),
            "rad" => Ok(TokenType::Radians(value)),
            "turn" => Ok(TokenType::Turns(value)),
            "" => {
                // No unit, determine if it's integer or float
                if number_str.contains('.') {
                    Ok(TokenType::Number(value))
                } else {
                    Ok(TokenType::Integer(value as i64))
                }
            }
            _ => Err(self.parse_error(format!("Unknown unit: {}", unit)))
        }
    }
    
    /// Read unit suffix (px, em, rem, deg, etc.)
    fn read_unit_suffix(&mut self) -> String {
        let mut unit = String::new();
        
        // Check for percentage first (single character)
        if self.peek() == Some('%') {
            self.advance();
            return "%".to_string();
        }
        
        // Read alphabetic unit suffix
        while let Some(ch) = self.peek() {
            if ch.is_alphabetic() {
                unit.push(ch);
                self.advance();
            } else {
                break;
            }
        }
        
        unit
    }
    
    fn read_identifier(&mut self, first_char: char) -> Result<String> {
        let mut identifier = String::new();
        identifier.push(first_char);
        
        while let Some(ch) = self.peek() {
            if ch.is_alphanumeric() || ch == '_' || ch == '-' {
                identifier.push(ch);
                self.advance();
            } else {
                break;
            }
        }
        
        Ok(identifier)
    }
    
    
    fn identify_keyword_or_identifier(&self, text: String) -> TokenType {
        match text.as_str() {
            // Element types
            "App" => TokenType::App,
            "Container" => TokenType::Container,
            "Text" => TokenType::Text,
            "Link" => TokenType::Link,
            "Image" => TokenType::Image,
            "EmbedView" => TokenType::EmbedView,
            "Canvas" => TokenType::Canvas,
            "Button" => TokenType::Button,
            "Input" => TokenType::Input,
            "List" => TokenType::List,
            "Grid" => TokenType::Grid,
            "Scrollable" => TokenType::Scrollable,
            "Tabs" => TokenType::Tabs,
            "Video" => TokenType::Video,
            
            // Keywords
            "style" => TokenType::Style,
            "font" => TokenType::Font,
            "Define" => TokenType::Define,
            "Properties" => TokenType::Properties,
            "in" => TokenType::In,
            
            // Boolean literals
            "true" => TokenType::Boolean(true),
            "false" => TokenType::Boolean(false),
            
            // Default to identifier
            _ => TokenType::Identifier(text),
        }
    }
    
    // Public method that can be called by the parser to read script content
    pub fn read_script_content(&mut self) -> Result<String> {
        let start_line = self.line;
        log::debug!("Starting to read script content at line {}", start_line);
        
        let mut content = String::new();
        let mut depth = 0; // Track nesting depth for proper closing brace detection
        
        // Read everything until we find the script block closing brace
        // We treat the content as raw text and only track braces for nesting
        while let Some(ch) = self.peek() {
            if ch == '{' {
                // This is a nested opening brace within the script content
                depth += 1;
                content.push(ch);
                self.advance();
            } else if ch == '}' {
                if depth > 0 {
                    // This is a nested closing brace within the script content
                    depth -= 1;
                    content.push(ch);
                    self.advance();
                } else {
                    // This is the script block closing brace
                    self.advance();
                    break;
                }
            } else {
                content.push(ch);
                if ch == '\n' {
                    self.line += 1;
                }
                self.advance();
            }
        }
        
        if depth > 0 {
            log::debug!("Script content parsing failed: depth = {} at line {}", depth, self.line);
            log::debug!("Content so far: {}", content);
            return Err(self.parse_error("Unclosed script block - missing '}'"));
        }
        
        log::debug!("Successfully read script content: {} characters", content.len());
        log::debug!("Script content: {}", content);
        Ok(content)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_basic_tokens() {
        let mut lexer = Lexer::new("{ } : ; = $ &", "test.kry".to_string());
        let tokens = lexer.tokenize().unwrap();
        
        assert_eq!(tokens[0].token_type, TokenType::LeftBrace);
        assert_eq!(tokens[1].token_type, TokenType::RightBrace);
        assert_eq!(tokens[2].token_type, TokenType::Colon);
        assert_eq!(tokens[3].token_type, TokenType::Semicolon);
        assert_eq!(tokens[4].token_type, TokenType::Equals);
        assert_eq!(tokens[5].token_type, TokenType::Dollar);
        assert_eq!(tokens[6].token_type, TokenType::Ampersand);
    }
    
    #[test]
    fn test_string_literal() {
        let mut lexer = Lexer::new(r#""hello world""#, "test.kry".to_string());
        let tokens = lexer.tokenize().unwrap();
        
        match &tokens[0].token_type {
            TokenType::String(s) => assert_eq!(s, "hello world"),
            _ => panic!("Expected string token"),
        }
    }
    
    #[test]
    fn test_string_escaping() {
        let mut lexer = Lexer::new(r#""hello\nworld\t\"test\"""#, "test.kry".to_string());
        let tokens = lexer.tokenize().unwrap();
        
        match &tokens[0].token_type {
            TokenType::String(s) => assert_eq!(s, "hello\nworld\t\"test\""),
            _ => panic!("Expected string token"),
        }
    }
    
    #[test]
    fn test_numbers() {
        let mut lexer = Lexer::new("42 3.14 -10", "test.kry".to_string());
        let tokens = lexer.tokenize().unwrap();
        
        match tokens[0].token_type {
            TokenType::Integer(i) => assert_eq!(i, 42),
            _ => panic!("Expected integer token"),
        }
        
        match tokens[1].token_type {
            TokenType::Number(n) => assert_eq!(n, 3.14),
            _ => panic!("Expected number token"),
        }
        
        match tokens[2].token_type {
            TokenType::Integer(i) => assert_eq!(i, -10),
            _ => panic!("Expected integer token"),
        }
    }
    
    #[test]
    fn test_colors() {
        let mut lexer = Lexer::new("#FF0000 #ABC #12345678", "test.kry".to_string());
        let tokens = lexer.tokenize().unwrap();
        
        match &tokens[0].token_type {
            TokenType::Color(c) => assert_eq!(c, "#FF0000"),
            _ => panic!("Expected color token"),
        }
        
        match &tokens[1].token_type {
            TokenType::Color(c) => assert_eq!(c, "#ABC"),
            _ => panic!("Expected color token"),
        }
        
        match &tokens[2].token_type {
            TokenType::Color(c) => assert_eq!(c, "#12345678"),
            _ => panic!("Expected color token"),
        }
    }
    
    #[test]
    fn test_pseudo_selector() {
        let mut lexer = Lexer::new("&:hover &:active &:focus", "test.kry".to_string());
        let tokens = lexer.tokenize().unwrap();
        
        match &tokens[0].token_type {
            TokenType::PseudoSelector(state) => assert_eq!(state, "hover"),
            _ => panic!("Expected PseudoSelector"),
        }
        match &tokens[1].token_type {
            TokenType::PseudoSelector(state) => assert_eq!(state, "active"),
            _ => panic!("Expected PseudoSelector"),
        }
        match &tokens[2].token_type {
            TokenType::PseudoSelector(state) => assert_eq!(state, "focus"),
            _ => panic!("Expected PseudoSelector"),
        }
    }
    
    #[test]
    fn test_keywords() {
        let mut lexer = Lexer::new("App Container style Define @include @variables", "test.kry".to_string());
        let tokens = lexer.tokenize().unwrap();
        
        assert_eq!(tokens[0].token_type, TokenType::App);
        assert_eq!(tokens[1].token_type, TokenType::Container);
        assert_eq!(tokens[2].token_type, TokenType::Style);
        assert_eq!(tokens[3].token_type, TokenType::Define);
        assert_eq!(tokens[4].token_type, TokenType::Include);
        assert_eq!(tokens[5].token_type, TokenType::Variables);
    }
    
    #[test]
    fn test_comments() {
        let mut lexer = Lexer::new("# This is a comment\nApp", "test.kry".to_string());
        let tokens = lexer.tokenize().unwrap();
        
        match &tokens[0].token_type {
            TokenType::Comment(c) => assert_eq!(c, " This is a comment"),
            _ => panic!("Expected comment token"),
        }
        assert_eq!(tokens[1].token_type, TokenType::Newline);
        assert_eq!(tokens[2].token_type, TokenType::App);
    }
    
    #[test]
    fn test_double_slash_comments() {
        let mut lexer = Lexer::new("// This is a double-slash comment\nContainer", "test.kry".to_string());
        let tokens = lexer.tokenize().unwrap();
        
        match &tokens[0].token_type {
            TokenType::Comment(c) => assert_eq!(c, " This is a double-slash comment"),
            _ => panic!("Expected comment token"),
        }
        assert_eq!(tokens[1].token_type, TokenType::Newline);
        assert_eq!(tokens[2].token_type, TokenType::Container);
    }
    
    #[test]
    fn test_both_comment_styles() {
        let input = "# Hash comment\n// Double-slash comment\nApp";
        let mut lexer = Lexer::new(input, "test.kry".to_string());
        let tokens = lexer.tokenize().unwrap();
        
        match &tokens[0].token_type {
            TokenType::Comment(c) => assert_eq!(c, " Hash comment"),
            _ => panic!("Expected hash comment token"),
        }
        assert_eq!(tokens[1].token_type, TokenType::Newline);
        
        match &tokens[2].token_type {
            TokenType::Comment(c) => assert_eq!(c, " Double-slash comment"),
            _ => panic!("Expected double-slash comment token"),
        }
        assert_eq!(tokens[3].token_type, TokenType::Newline);
        assert_eq!(tokens[4].token_type, TokenType::App);
    }
    
    #[test]
    fn test_complex_example() {
        let input = r##"
App {
    window_title: "My App"
    background_color: #FF0000
    
    Container {
        id: "main_container"
        padding: 16
        
        Button {
            text: "Click me"
            &:hover {
                background_color: #00FF00
            }
        }
    }
}
"##;
        
        let mut lexer = Lexer::new(input, "test.kry".to_string());
        let tokens = lexer.tokenize().unwrap();
        
        // Just verify we can tokenize without errors and get reasonable tokens
        assert!(tokens.len() > 10);
        assert_eq!(tokens.last().unwrap().token_type, TokenType::Eof);
    }
}