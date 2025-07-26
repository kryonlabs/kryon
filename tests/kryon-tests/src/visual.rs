//! Visual testing utilities for UI component validation

use crate::prelude::*;
use std::collections::HashMap;

/// Visual test configuration
#[derive(Debug, Clone)]
pub struct VisualTestConfig {
    pub name: String,
    pub source: String,
    pub window_size: (u32, u32),
    pub expected_elements: Vec<VisualElementExpectation>,
    pub layout_expectations: Vec<LayoutExpectation>,
    pub backend: VisualTestBackend,
    pub timeout: Duration,
}

/// Visual test backends
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum VisualTestBackend {
    Ratatui,      // Text-based visual testing
    ImageCompare, // Future: Image-based comparison
}

/// Expected visual element properties
#[derive(Debug, Clone)]
pub struct VisualElementExpectation {
    pub element_type: String,
    pub text_content: Option<String>,
    pub position: Option<(i32, i32)>,
    pub size: Option<(u32, u32)>,
    pub visible: bool,
    pub properties: HashMap<String, PropertyValue>,
}

/// Layout positioning and sizing expectations
#[derive(Debug, Clone)]
pub struct LayoutExpectation {
    pub element_index: usize,
    pub expected_layout: LayoutType,
    pub expected_alignment: Option<String>,
    pub expected_justification: Option<String>,
    pub expected_bounds: Option<VisualBounds>,
}

/// Visual bounds for element positioning
#[derive(Debug, Clone, PartialEq)]
pub struct VisualBounds {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
}

/// Layout types for visual testing
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum LayoutType {
    Row,
    Column,
    Absolute,
    Auto,
}

/// Visual test result
#[derive(Debug, Clone)]
pub struct VisualTestResult {
    pub config: VisualTestConfig,
    pub element_validations: Vec<ElementValidationResult>,
    pub layout_validations: Vec<LayoutValidationResult>,
    pub rendering_output: String,
    pub success: bool,
    pub execution_time: Duration,
    pub errors: Vec<String>,
}

/// Element validation result
#[derive(Debug, Clone)]
pub struct ElementValidationResult {
    pub expectation: VisualElementExpectation,
    pub found: bool,
    pub actual_properties: HashMap<String, PropertyValue>,
    pub validation_errors: Vec<String>,
}

/// Layout validation result
#[derive(Debug, Clone)]
pub struct LayoutValidationResult {
    pub expectation: LayoutExpectation,
    pub actual_layout: Option<LayoutType>,
    pub actual_bounds: Option<VisualBounds>,
    pub success: bool,
    pub errors: Vec<String>,
}

/// Visual testing framework
#[derive(Debug)]
pub struct VisualTester {
    pub config: TestConfig,
    pub results: Vec<VisualTestResult>,
    pub temp_dir: PathBuf,
}

impl VisualElementExpectation {
    pub fn new(element_type: impl Into<String>) -> Self {
        Self {
            element_type: element_type.into(),
            text_content: None,
            position: None,
            size: None,
            visible: true,
            properties: HashMap::new(),
        }
    }

    pub fn with_text(mut self, text: impl Into<String>) -> Self {
        self.text_content = Some(text.into());
        self
    }

    pub fn with_position(mut self, x: i32, y: i32) -> Self {
        self.position = Some((x, y));
        self
    }

    pub fn with_size(mut self, width: u32, height: u32) -> Self {
        self.size = Some((width, height));
        self
    }

    pub fn with_property(mut self, name: impl Into<String>, value: PropertyValue) -> Self {
        self.properties.insert(name.into(), value);
        self
    }

    pub fn hidden(mut self) -> Self {
        self.visible = false;
        self
    }
}

impl LayoutExpectation {
    pub fn new(element_index: usize, layout: LayoutType) -> Self {
        Self {
            element_index,
            expected_layout: layout,
            expected_alignment: None,
            expected_justification: None,
            expected_bounds: None,
        }
    }

    pub fn with_alignment(mut self, alignment: impl Into<String>) -> Self {
        self.expected_alignment = Some(alignment.into());
        self
    }

    pub fn with_justification(mut self, justification: impl Into<String>) -> Self {
        self.expected_justification = Some(justification.into());
        self
    }

    pub fn with_bounds(mut self, x: f32, y: f32, width: f32, height: f32) -> Self {
        self.expected_bounds = Some(VisualBounds { x, y, width, height });
        self
    }
}

impl VisualTestConfig {
    pub fn new(name: impl Into<String>, source: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            source: source.into(),
            window_size: (800, 600),
            expected_elements: Vec::new(),
            layout_expectations: Vec::new(),
            backend: VisualTestBackend::Ratatui,
            timeout: Duration::from_secs(30),
        }
    }

    pub fn with_window_size(mut self, width: u32, height: u32) -> Self {
        self.window_size = (width, height);
        self
    }

    pub fn expect_element(mut self, expectation: VisualElementExpectation) -> Self {
        self.expected_elements.push(expectation);
        self
    }

    pub fn expect_layout(mut self, expectation: LayoutExpectation) -> Self {
        self.layout_expectations.push(expectation);
        self
    }

    pub fn with_backend(mut self, backend: VisualTestBackend) -> Self {
        self.backend = backend;
        self
    }

    pub fn with_timeout(mut self, timeout: Duration) -> Self {
        self.timeout = timeout;
        self
    }
}

impl VisualTester {
    pub fn new(config: TestConfig) -> Result<Self> {
        let temp_dir = config.output_directory.join("visual_tests");
        fs::create_dir_all(&temp_dir)?;
        
        Ok(Self {
            config,
            results: Vec::new(),
            temp_dir,
        })
    }

    /// Run a visual test
    pub async fn run_visual_test(&mut self, test_config: VisualTestConfig) -> Result<VisualTestResult> {
        let start_time = std::time::Instant::now();
        let mut errors = Vec::new();
        
        println!("Running visual test: {}", test_config.name);
        
        // Compile the source
        let krb_data = match self.compile_source(&test_config.source) {
            Ok(data) => data,
            Err(e) => {
                errors.push(format!("Compilation failed: {}", e));
                return Ok(VisualTestResult {
                    config: test_config,
                    element_validations: Vec::new(),
                    layout_validations: Vec::new(),
                    rendering_output: String::new(),
                    success: false,
                    execution_time: start_time.elapsed(),
                    errors,
                });
            }
        };
        
        // Parse KRB for analysis
        let krb_file = match KrbFile::parse(&krb_data) {
            Ok(file) => file,
            Err(e) => {
                errors.push(format!("KRB parsing failed: {}", e));
                return Ok(VisualTestResult {
                    config: test_config,
                    element_validations: Vec::new(),
                    layout_validations: Vec::new(),
                    rendering_output: String::new(),
                    success: false,
                    execution_time: start_time.elapsed(),
                    errors,
                });
            }
        };
        
        // Generate visual output
        let rendering_output = match self.generate_visual_output(&test_config, &krb_data).await {
            Ok(output) => output,
            Err(e) => {
                errors.push(format!("Visual rendering failed: {}", e));
                String::new()
            }
        };
        
        // Validate elements
        let element_validations = self.validate_elements(&test_config, &krb_file);
        
        // Validate layout
        let layout_validations = self.validate_layouts(&test_config, &krb_file);
        
        // Check for validation failures
        let element_failures = element_validations.iter()
            .filter(|v| !v.validation_errors.is_empty())
            .count();
        let layout_failures = layout_validations.iter()
            .filter(|v| !v.success)
            .count();
        
        let success = errors.is_empty() && element_failures == 0 && layout_failures == 0;
        
        if !success {
            println!("❌ Visual test failed: {}", test_config.name);
            if element_failures > 0 {
                println!("  Element validation failures: {}", element_failures);
            }
            if layout_failures > 0 {
                println!("  Layout validation failures: {}", layout_failures);
            }
        } else {
            println!("✅ Visual test passed: {}", test_config.name);
        }
        
        let result = VisualTestResult {
            config: test_config,
            element_validations,
            layout_validations,
            rendering_output,
            success,
            execution_time: start_time.elapsed(),
            errors,
        };
        
        self.results.push(result.clone());
        Ok(result)
    }

    fn compile_source(&self, source: &str) -> Result<Vec<u8>> {
        let options = CompilerOptions::default();
        compile_string(source, options)
            .map_err(|e| anyhow::anyhow!("Compilation failed: {}", e))
    }

    async fn generate_visual_output(&self, config: &VisualTestConfig, krb_data: &[u8]) -> Result<String> {
        match config.backend {
            VisualTestBackend::Ratatui => self.generate_ratatui_output(krb_data).await,
            VisualTestBackend::ImageCompare => {
                bail!("Image comparison backend not yet implemented")
            }
        }
    }

    async fn generate_ratatui_output(&self, krb_data: &[u8]) -> Result<String> {
        // Save KRB to temporary file
        let krb_path = self.temp_dir.join("test.krb");
        fs::write(&krb_path, krb_data)?;
        
        // Run ratatui renderer
        let output = std::process::Command::new("cargo")
            .args(&[
                "run",
                "--package", "kryon-ratatui",
                "--bin", "kryon-ratatui",
                "--",
                krb_path.to_str().unwrap(),
                "--text-output",
            ])
            .output()
            .context("Failed to run ratatui renderer")?;
        
        if !output.status.success() {
            bail!(
                "Ratatui rendering failed: {}",
                String::from_utf8_lossy(&output.stderr)
            );
        }
        
        Ok(String::from_utf8_lossy(&output.stdout).to_string())
    }

    fn validate_elements(&self, config: &VisualTestConfig, krb_file: &KrbFile) -> Vec<ElementValidationResult> {
        let mut validations = Vec::new();
        
        for expectation in &config.expected_elements {
            let mut validation_errors = Vec::new();
            let mut found = false;
            let mut actual_properties = HashMap::new();
            
            // Find matching element
            for element in &krb_file.elements {
                if element.element_type == expectation.element_type {
                    found = true;
                    actual_properties = element.custom_properties.clone();
                    
                    // Validate text content
                    if let Some(expected_text) = &expectation.text_content {
                        if let Some(PropertyValue::String(actual_text)) = element.custom_properties.get("text") {
                            if actual_text != expected_text {
                                validation_errors.push(format!(
                                    "Text mismatch: expected '{}', got '{}'",
                                    expected_text, actual_text
                                ));
                            }
                        } else {
                            validation_errors.push("Expected text property not found".to_string());
                        }
                    }
                    
                    // Validate custom properties
                    for (prop_name, expected_value) in &expectation.properties {
                        if let Some(actual_value) = element.custom_properties.get(prop_name) {
                            if actual_value != expected_value {
                                validation_errors.push(format!(
                                    "Property '{}' mismatch: expected {:?}, got {:?}",
                                    prop_name, expected_value, actual_value
                                ));
                            }
                        } else {
                            validation_errors.push(format!(
                                "Expected property '{}' not found",
                                prop_name
                            ));
                        }
                    }
                    
                    break;
                }
            }
            
            if !found {
                validation_errors.push(format!(
                    "Element type '{}' not found",
                    expectation.element_type
                ));
            }
            
            validations.push(ElementValidationResult {
                expectation: expectation.clone(),
                found,
                actual_properties,
                validation_errors,
            });
        }
        
        validations
    }

    fn validate_layouts(&self, config: &VisualTestConfig, krb_file: &KrbFile) -> Vec<LayoutValidationResult> {
        let mut validations = Vec::new();
        
        for expectation in &config.layout_expectations {
            let mut errors = Vec::new();
            let mut actual_layout = None;
            let mut success = true;
            
            if let Some(element) = krb_file.elements.get(expectation.element_index) {
                // Check layout type
                if let Some(PropertyValue::String(layout_str)) = element.custom_properties.get("layout") {
                    actual_layout = match layout_str.as_str() {
                        "row" => Some(LayoutType::Row),
                        "column" => Some(LayoutType::Column),
                        "absolute" => Some(LayoutType::Absolute),
                        _ => Some(LayoutType::Auto),
                    };
                    
                    if actual_layout != Some(expectation.expected_layout.clone()) {
                        errors.push(format!(
                            "Layout type mismatch: expected {:?}, got {:?}",
                            expectation.expected_layout, actual_layout
                        ));
                        success = false;
                    }
                } else {
                    errors.push("Layout property not found".to_string());
                    success = false;
                }
                
                // Check alignment
                if let Some(expected_alignment) = &expectation.expected_alignment {
                    if let Some(PropertyValue::String(actual_alignment)) = element.custom_properties.get("align_items") {
                        if actual_alignment != expected_alignment {
                            errors.push(format!(
                                "Alignment mismatch: expected '{}', got '{}'",
                                expected_alignment, actual_alignment
                            ));
                            success = false;
                        }
                    } else {
                        errors.push("Alignment property not found".to_string());
                        success = false;
                    }
                }
                
                // Check justification
                if let Some(expected_justification) = &expectation.expected_justification {
                    if let Some(PropertyValue::String(actual_justification)) = element.custom_properties.get("justify_content") {
                        if actual_justification != expected_justification {
                            errors.push(format!(
                                "Justification mismatch: expected '{}', got '{}'",
                                expected_justification, actual_justification
                            ));
                            success = false;
                        }
                    } else {
                        errors.push("Justification property not found".to_string());
                        success = false;
                    }
                }
            } else {
                errors.push(format!(
                    "Element at index {} not found",
                    expectation.element_index
                ));
                success = false;
            }
            
            validations.push(LayoutValidationResult {
                expectation: expectation.clone(),
                actual_layout,
                actual_bounds: None, // TODO: Implement bounds checking
                success,
                errors,
            });
        }
        
        validations
    }

    /// Print visual test results
    pub fn print_results(&self) {
        println!("\n=== Visual Test Results ===");
        
        let total = self.results.len();
        let passed = self.results.iter().filter(|r| r.success).count();
        
        for result in &self.results {
            let status = if result.success {
                "PASS".green()
            } else {
                "FAIL".red()
            };
            
            println!(
                "[{}] {} ({:.2}ms)",
                status,
                result.config.name,
                result.execution_time.as_secs_f64() * 1000.0
            );
            
            if !result.success {
                // Print element validation failures
                for validation in &result.element_validations {
                    if !validation.validation_errors.is_empty() {
                        println!("  Element '{}' validation failed:", validation.expectation.element_type);
                        for error in &validation.validation_errors {
                            println!("    ❌ {}", error);
                        }
                    }
                }
                
                // Print layout validation failures
                for validation in &result.layout_validations {
                    if !validation.success {
                        println!("  Layout validation failed for element {}:", validation.expectation.element_index);
                        for error in &validation.errors {
                            println!("    ❌ {}", error);
                        }
                    }
                }
                
                // Print general errors
                for error in &result.errors {
                    println!("  ❌ {}", error);
                }
            }
        }
        
        println!("\nVisual Test Summary: {} total, {} passed, {} failed", total, passed, total - passed);
    }
}

/// Create common visual test cases
pub fn create_standard_visual_tests() -> Vec<VisualTestConfig> {
    vec![
        // Basic text rendering
        VisualTestConfig::new(
            "basic_text",
            r#"
App {
    Text { text: "Hello World" }
}
"#,
        )
        .expect_element(
            VisualElementExpectation::new("Text")
                .with_text("Hello World")
        ),
        
        // Flexbox row layout
        VisualTestConfig::new(
            "flex_row",
            r#"
App {
    Container {
        layout: "row"
        justify_content: "space-between"
        align_items: "center"
        
        Button { text: "Left" }
        Button { text: "Right" }
    }
}
"#,
        )
        .expect_layout(
            LayoutExpectation::new(1, LayoutType::Row)
                .with_justification("space-between")
                .with_alignment("center")
        )
        .expect_element(
            VisualElementExpectation::new("Button")
                .with_text("Left")
        )
        .expect_element(
            VisualElementExpectation::new("Button")
                .with_text("Right")
        ),
        
        // Absolute positioning
        VisualTestConfig::new(
            "absolute_position",
            r#"
App {
    Container {
        layout: "absolute"
        
        Text {
            text: "Positioned Text"
            x: 100
            y: 50
        }
    }
}
"#,
        )
        .expect_layout(
            LayoutExpectation::new(1, LayoutType::Absolute)
        )
        .expect_element(
            VisualElementExpectation::new("Text")
                .with_text("Positioned Text")
                .with_property("x", PropertyValue::Integer(100))
                .with_property("y", PropertyValue::Integer(50))
        ),
    ]
}

/// Utility macro for visual tests
#[macro_export]
macro_rules! kryon_visual_test {
    ($test_name:expr, $source:expr, $($expectation:expr),*) => {
        #[tokio::test]
        async fn visual_test() -> anyhow::Result<()> {
            let mut tester = crate::visual::VisualTester::new(
                crate::utils::TestConfig::default()
            )?;
            
            let mut config = crate::visual::VisualTestConfig::new($test_name, $source);
            $(config = config.expect_element($expectation);)*
            
            let result = tester.run_visual_test(config).await?;
            
            if !result.success {
                anyhow::bail!("Visual test failed: {:?}", result.errors);
            }
            
            Ok(())
        }
    };
}

pub use kryon_visual_test;
