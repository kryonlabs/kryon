//! Semantic analysis tests

use kryon_compiler::compiler::frontend::semantic::SemanticAnalyzer;
use kryon_compiler::compiler::frontend::parser::Parser;
use kryon_compiler::compiler::frontend::lexer::Lexer;

#[test]
fn test_type_checking() {
    let input = r#"
        App {
            window_width: 800      // Valid: number
            window_height: "600"   // Invalid: should be number
        }
    "#;
    
    let mut lexer = Lexer::new(input, "<test>");
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(tokens);
    let ast = parser.parse().unwrap();
    
    let mut analyzer = SemanticAnalyzer::new();
    let result = analyzer.analyze(&ast);
    
    assert!(result.is_err(), "Should fail type checking");
    let err = result.unwrap_err();
    assert!(err.to_string().contains("window_height"));
}

#[test]
fn test_required_properties() {
    let input = r#"
        App {
            // Missing required window_width and window_height
            background_color: "#000000ff"
        }
    "#;
    
    let mut lexer = Lexer::new(input, "<test>");
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(tokens);
    let ast = parser.parse().unwrap();
    
    let mut analyzer = SemanticAnalyzer::new();
    let result = analyzer.analyze(&ast);
    
    // App should have default values for window dimensions
    assert!(result.is_ok(), "Should use defaults for missing properties");
}

#[test]
fn test_unknown_properties() {
    let input = r#"
        Container {
            width: 100
            unknown_property: "value"  // Should warn or error
        }
    "#;
    
    let mut lexer = Lexer::new(input, "<test>");
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(tokens);
    let ast = parser.parse().unwrap();
    
    let mut analyzer = SemanticAnalyzer::new();
    let result = analyzer.analyze(&ast);
    
    // Could either error or warn about unknown properties
    // For now, we'll allow unknown properties for extensibility
    assert!(result.is_ok());
}

#[test]
fn test_property_value_validation() {
    let input = r#"
        Container {
            opacity: 1.5        // Invalid: should be 0.0 to 1.0
            border_radius: -5   // Invalid: should be non-negative
        }
    "#;
    
    let mut lexer = Lexer::new(input, "<test>");
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(tokens);
    let ast = parser.parse().unwrap();
    
    let mut analyzer = SemanticAnalyzer::new();
    let result = analyzer.analyze(&ast);
    
    // Should validate property value ranges
    assert!(result.is_err() || analyzer.has_warnings());
}

#[test]
fn test_id_uniqueness() {
    let input = r#"
        App {
            Container {
                id: "my_id"
            }
            Container {
                id: "my_id"  // Duplicate ID
            }
        }
    "#;
    
    let mut lexer = Lexer::new(input, "<test>");
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(tokens);
    let ast = parser.parse().unwrap();
    
    let mut analyzer = SemanticAnalyzer::new();
    let result = analyzer.analyze(&ast);
    
    assert!(result.is_err(), "Should error on duplicate IDs");
}