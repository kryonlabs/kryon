//! Parser tests for the Kryon compiler

use kryon_compiler::compiler::frontend::parser::Parser;
use kryon_compiler::compiler::frontend::lexer::Lexer;

#[test]
fn test_parse_empty_app() {
    let input = "App { }";
    let mut lexer = Lexer::new(input, "<test>");
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(tokens);
    let ast = parser.parse().unwrap();
    
    assert_eq!(ast.elements.len(), 1);
    assert_eq!(ast.elements[0].element_type, "App");
    assert_eq!(ast.elements[0].properties.len(), 0);
}

#[test]
fn test_parse_nested_elements() {
    let input = r#"
        App {
            Container {
                Text {
                    text: "Hello"
                }
            }
        }
    "#;
    
    let mut lexer = Lexer::new(input, "<test>");
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(tokens);
    let ast = parser.parse().unwrap();
    
    assert_eq!(ast.elements.len(), 1);
    assert_eq!(ast.elements[0].children.len(), 1);
    assert_eq!(ast.elements[0].children[0].children.len(), 1);
}

#[test]
fn test_parse_properties() {
    let input = r#"
        Container {
            width: 100
            height: 200
            background_color: "#ff0000ff"
            display: flex
            align_items: center
        }
    "#;
    
    let mut lexer = Lexer::new(input, "<test>");
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(tokens);
    let ast = parser.parse().unwrap();
    
    let container = &ast.elements[0];
    assert_eq!(container.properties.len(), 5);
    
    // Check property values
    let width_prop = container.properties.iter()
        .find(|p| p.name == "width").unwrap();
    assert!(matches!(width_prop.value, PropertyValue::Integer(100)));
}

#[test]
fn test_parse_id_attribute() {
    let input = r#"
        Container {
            id: "my_container"
            width: 100
        }
    "#;
    
    let mut lexer = Lexer::new(input, "<test>");
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(tokens);
    let ast = parser.parse().unwrap();
    
    let container = &ast.elements[0];
    assert_eq!(container.id.as_ref().unwrap(), "my_container");
}

#[test]
fn test_parse_comments() {
    let input = r#"
        // This is a comment
        App {
            # This is also a comment
            width: 100  // Inline comment
        }
    "#;
    
    let mut lexer = Lexer::new(input, "<test>");
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(tokens);
    let ast = parser.parse().unwrap();
    
    // Comments should be ignored
    assert_eq!(ast.elements.len(), 1);
    assert_eq!(ast.elements[0].properties.len(), 1);
}

#[test]
fn test_parse_percentage_values() {
    let input = r#"
        Container {
            width: 100%
            height: 50%
        }
    "#;
    
    let mut lexer = Lexer::new(input, "<test>");
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(tokens);
    let ast = parser.parse().unwrap();
    
    let container = &ast.elements[0];
    let width_prop = container.properties.iter()
        .find(|p| p.name == "width").unwrap();
    assert!(matches!(width_prop.value, PropertyValue::String(ref s) if s == "100%"));
}

#[test]
fn test_parse_error_recovery() {
    let input = r#"
        App {
            width: 100
            height  // Missing colon and value
            background_color: "#ff0000ff"
        }
    "#;
    
    let mut lexer = Lexer::new(input, "<test>");
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(tokens);
    let result = parser.parse();
    
    assert!(result.is_err(), "Parser should report error for malformed property");
}