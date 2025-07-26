//! Property-based testing utilities for fuzzing and edge case discovery

use crate::prelude::*;
use std::collections::HashSet;

/// Property test configuration
#[derive(Debug, Clone)]
pub struct PropertyTestConfig {
    pub name: String,
    pub test_cases: usize,
    pub max_shrink_iters: usize,
    pub timeout: Duration,
}

/// Property test generators
pub mod generators {
    use super::*;
    use proptest::prelude::*;
    
    /// Generate valid KRY source code
    pub fn valid_kry_source() -> impl Strategy<Value = String> {
        prop::collection::vec(element_generator(), 1..10)
            .prop_map(|elements| {
                let mut source = "App {\n".to_string();
                for element in elements {
                    source.push_str(&format!("    {}\n", element));
                }
                source.push_str("}\n");
                source
            })
    }
    
    /// Generate individual elements
    fn element_generator() -> impl Strategy<Value = String> {
        prop_oneof![
            text_element_generator(),
            button_element_generator(),
            container_element_generator(),
        ]
    }
    
    /// Generate Text elements
    fn text_element_generator() -> impl Strategy<Value = String> {
        "[a-zA-Z0-9 ]{1,50}"
            .prop_map(|text| format!("Text {{ text: \"{}\" }}", text))
    }
    
    /// Generate Button elements
    fn button_element_generator() -> impl Strategy<Value = String> {
        "[a-zA-Z0-9 ]{1,30}"
            .prop_map(|text| format!("Button {{ text: \"{}\" }}", text))
    }
    
    /// Generate Container elements
    fn container_element_generator() -> impl Strategy<Value = String> {
        prop_oneof![
            Just("Container { layout: \"row\" }".to_string()),
            Just("Container { layout: \"column\" }".to_string()),
            Just("Container { padding: 16 }".to_string()),
        ]
    }
    
    /// Generate color values
    pub fn color_generator() -> impl Strategy<Value = String> {
        prop_oneof![
            // Hex colors
            "#[0-9a-fA-F]{6}",
            "#[0-9a-fA-F]{8}", // With alpha
            // Named colors
            prop::sample::select(vec![
                "red", "green", "blue", "white", "black",
                "yellow", "cyan", "magenta", "transparent"
            ])
        ]
    }
    
    /// Generate numeric properties
    pub fn numeric_property_generator() -> impl Strategy<Value = (String, i64)> {
        prop_oneof![
            ("width", 1i64..1000),
            ("height", 1i64..1000),
            ("padding", 0i64..100),
            ("margin", 0i64..100),
            ("font_size", 8i64..72),
            ("border_radius", 0i64..50),
        ].prop_map(|(prop, value)| (prop.to_string(), value))
    }
    
    /// Generate layout properties
    pub fn layout_property_generator() -> impl Strategy<Value = (String, String)> {
        prop_oneof![
            ("layout", prop::sample::select(vec!["row", "column", "absolute"])),
            ("justify_content", prop::sample::select(vec![
                "flex-start", "flex-end", "center", "space-between", "space-around"
            ])),
            ("align_items", prop::sample::select(vec![
                "flex-start", "flex-end", "center", "stretch", "baseline"
            ])),
        ].prop_map(|(prop, value)| (prop.to_string(), value.to_string()))
    }
    
    /// Generate malformed/edge case inputs
    pub fn edge_case_generator() -> impl Strategy<Value = String> {
        prop_oneof![
            // Empty elements
            Just("App { }".to_string()),
            // Missing closing braces
            Just("App { Text { text: \"test\" }".to_string()),
            // Invalid property values
            Just("App { Text { font_size: \"not_a_number\" } }".to_string()),
            // Deeply nested structures
            deep_nesting_generator(),
            // Very long text
            "[a-zA-Z ]{1000,5000}".prop_map(|text| {
                format!("App {{ Text {{ text: \"{}\" }} }}", text)
            }),
        ]
    }
    
    fn deep_nesting_generator() -> impl Strategy<Value = String> {
        (1..20usize).prop_map(|depth| {
            let mut source = "App {\n".to_string();
            
            // Create nested containers
            for i in 0..depth {
                source.push_str(&"    ".repeat(i + 1));
                source.push_str("Container {\n");
            }
            
            // Add inner text
            source.push_str(&"    ".repeat(depth + 1));
            source.push_str("Text { text: \"Deep text\" }\n");
            
            // Close all containers
            for i in (0..depth).rev() {
                source.push_str(&"    ".repeat(i + 1));
                source.push_str("}\n");
            }
            
            source.push_str("}\n");
            source
        })
    }
}

/// Property test implementations
pub struct PropertyTester {
    pub config: PropertyTestConfig,
    pub results: Vec<PropertyTestResult>,
}

/// Result of a property test
#[derive(Debug, Clone)]
pub struct PropertyTestResult {
    pub test_name: String,
    pub test_cases_run: usize,
    pub failures: Vec<PropertyTestFailure>,
    pub success: bool,
    pub execution_time: Duration,
}

/// Property test failure information
#[derive(Debug, Clone)]
pub struct PropertyTestFailure {
    pub input: String,
    pub error: String,
    pub shrunk_input: Option<String>,
}

impl Default for PropertyTestConfig {
    fn default() -> Self {
        Self {
            name: "property_test".to_string(),
            test_cases: 100,
            max_shrink_iters: 100,
            timeout: Duration::from_secs(30),
        }
    }
}

impl PropertyTester {
    pub fn new(config: PropertyTestConfig) -> Self {
        Self {
            config,
            results: Vec::new(),
        }
    }
    
    /// Test that valid KRY source always compiles successfully
    pub fn test_valid_compilation_always_succeeds(&mut self) -> Result<()> {
        use proptest::test_runner::TestRunner;
        use proptest::strategy::Strategy;
        
        let mut runner = TestRunner::default();
        let mut failures = Vec::new();
        let start_time = std::time::Instant::now();
        
        for _ in 0..self.config.test_cases {
            let source = generators::valid_kry_source().new_tree(&mut runner)?.current();
            
            let options = CompilerOptions::default();
            match compile_string(&source, options) {
                Ok(_) => {},
                Err(e) => {
                    failures.push(PropertyTestFailure {
                        input: source,
                        error: e.to_string(),
                        shrunk_input: None, // TODO: Implement shrinking
                    });
                }
            }
        }
        
        let result = PropertyTestResult {
            test_name: "valid_compilation_always_succeeds".to_string(),
            test_cases_run: self.config.test_cases,
            failures,
            success: failures.is_empty(),
            execution_time: start_time.elapsed(),
        };
        
        self.results.push(result.clone());
        
        if !result.success {
            bail!("Property test failed with {} failures", result.failures.len());
        }
        
        Ok(())
    }
    
    /// Test that compilation errors are handled gracefully
    pub fn test_error_handling_robustness(&mut self) -> Result<()> {
        use proptest::test_runner::TestRunner;
        use proptest::strategy::Strategy;
        
        let mut runner = TestRunner::default();
        let mut failures = Vec::new();
        let start_time = std::time::Instant::now();
        
        for _ in 0..self.config.test_cases {
            let source = generators::edge_case_generator().new_tree(&mut runner)?.current();
            
            let options = CompilerOptions::default();
            match compile_string(&source, options) {
                Ok(_) => {
                    // Edge cases might succeed, that's okay
                },
                Err(e) => {
                    // Check that error messages are reasonable
                    if e.to_string().is_empty() {
                        failures.push(PropertyTestFailure {
                            input: source,
                            error: "Empty error message".to_string(),
                            shrunk_input: None,
                        });
                    }
                    
                    // Check that compiler doesn't panic (would be caught by test harness)
                }
            }
        }
        
        let result = PropertyTestResult {
            test_name: "error_handling_robustness".to_string(),
            test_cases_run: self.config.test_cases,
            failures,
            success: failures.is_empty(),
            execution_time: start_time.elapsed(),
        };
        
        self.results.push(result.clone());
        
        if !result.success {
            bail!("Property test failed with {} failures", result.failures.len());
        }
        
        Ok(())
    }
    
    /// Test that parsed KRB files maintain structural integrity
    pub fn test_krb_structural_integrity(&mut self) -> Result<()> {
        use proptest::test_runner::TestRunner;
        use proptest::strategy::Strategy;
        
        let mut runner = TestRunner::default();
        let mut failures = Vec::new();
        let start_time = std::time::Instant::now();
        
        for _ in 0..self.config.test_cases {
            let source = generators::valid_kry_source().new_tree(&mut runner)?.current();
            
            let options = CompilerOptions::default();
            match compile_string(&source, options) {
                Ok(krb_data) => {
                    match KrbFile::parse(&krb_data) {
                        Ok(krb_file) => {
                            // Test structural integrity
                            if let Err(e) = self.validate_krb_structure(&krb_file) {
                                failures.push(PropertyTestFailure {
                                    input: source,
                                    error: format!("KRB structure validation failed: {}", e),
                                    shrunk_input: None,
                                });
                            }
                        },
                        Err(e) => {
                            failures.push(PropertyTestFailure {
                                input: source,
                                error: format!("KRB parsing failed: {}", e),
                                shrunk_input: None,
                            });
                        }
                    }
                },
                Err(_) => {
                    // Compilation failure is acceptable for edge cases
                }
            }
        }
        
        let result = PropertyTestResult {
            test_name: "krb_structural_integrity".to_string(),
            test_cases_run: self.config.test_cases,
            failures,
            success: failures.is_empty(),
            execution_time: start_time.elapsed(),
        };
        
        self.results.push(result.clone());
        
        if !result.success {
            bail!("Property test failed with {} failures", result.failures.len());
        }
        
        Ok(())
    }
    
    fn validate_krb_structure(&self, krb_file: &KrbFile) -> Result<()> {
        // Check for orphaned child references
        for (parent_idx, parent) in krb_file.elements.iter().enumerate() {
            for &child_id in &parent.children {
                let child_idx = child_id as usize;
                if child_idx >= krb_file.elements.len() {
                    bail!(
                        "Element {} references non-existent child {}",
                        parent_idx, child_idx
                    );
                }
            }
        }
        
        // Check for circular references
        for (idx, element) in krb_file.elements.iter().enumerate() {
            if element.children.contains(&(idx as u32)) {
                bail!("Element {} contains itself as child", idx);
            }
        }
        
        // Check for duplicate IDs
        let mut seen_ids = HashSet::new();
        for element in &krb_file.elements {
            if let Some(PropertyValue::String(id)) = element.custom_properties.get("id") {
                if !seen_ids.insert(id.clone()) {
                    bail!("Duplicate element ID: {}", id);
                }
            }
        }
        
        Ok(())
    }
    
    /// Run all property tests
    pub fn run_all_tests(&mut self) -> Result<()> {
        println!("Running property tests with {} test cases each...", self.config.test_cases);
        
        self.test_valid_compilation_always_succeeds()?;
        println!("✅ Valid compilation property test passed");
        
        self.test_error_handling_robustness()?;
        println!("✅ Error handling robustness test passed");
        
        self.test_krb_structural_integrity()?;
        println!("✅ KRB structural integrity test passed");
        
        self.print_summary();
        Ok(())
    }
    
    pub fn print_summary(&self) {
        println!("\n=== Property Test Summary ===");
        
        let total_tests = self.results.len();
        let passed_tests = self.results.iter().filter(|r| r.success).count();
        let total_cases: usize = self.results.iter().map(|r| r.test_cases_run).sum();
        let total_failures: usize = self.results.iter().map(|r| r.failures.len()).sum();
        
        println!("Tests run: {}", total_tests);
        println!("Tests passed: {}", passed_tests);
        println!("Total test cases: {}", total_cases);
        println!("Total failures: {}", total_failures);
        
        if total_failures > 0 {
            println!("\n=== Failures ===");
            for result in &self.results {
                if !result.failures.is_empty() {
                    println!("\n{}: {} failures", result.test_name, result.failures.len());
                    for (i, failure) in result.failures.iter().take(3).enumerate() {
                        println!("  {}. {}", i + 1, failure.error);
                        if failure.input.len() < 200 {
                            println!("     Input: {}", failure.input.replace('\n', "\\n"));
                        }
                    }
                    if result.failures.len() > 3 {
                        println!("     ... and {} more failures", result.failures.len() - 3);
                    }
                }
            }
        }
    }
}

/// Create a property test suite with default tests
pub fn create_property_test_suite() -> PropertyTester {
    let config = PropertyTestConfig {
        name: "kryon_property_tests".to_string(),
        test_cases: 50, // Reasonable default for CI
        max_shrink_iters: 50,
        timeout: Duration::from_secs(60),
    };
    
    PropertyTester::new(config)
}

/// Macro for easy property test creation
#[macro_export]
macro_rules! kryon_property_test {
    ($test_name:expr, $generator:expr, $property:expr) => {
        #[test]
        fn property_test() {
            use proptest::prelude::*;
            
            proptest!(|($test_name in $generator)| {
                $property
            });
        }
    };
}

pub use kryon_property_test;
