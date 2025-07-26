//! Integration tests for end-to-end pipeline testing

use crate::prelude::*;
use std::process::{Command, Stdio};
use std::io::Write;

/// Integration test configuration
#[derive(Debug, Clone)]
pub struct IntegrationTestConfig {
    pub name: String,
    pub source_file: Option<PathBuf>,
    pub expected_output: Option<String>,
    pub timeout: Duration,
    pub backend: IntegrationBackend,
    pub validate_rendering: bool,
}

/// Integration test backends
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum IntegrationBackend {
    Ratatui,
    Raylib,
    All,
}

/// Integration test result
#[derive(Debug, Clone)]
pub struct IntegrationTestResult {
    pub config: IntegrationTestConfig,
    pub compilation_success: bool,
    pub rendering_success: bool,
    pub output: String,
    pub errors: Vec<String>,
    pub execution_time: Duration,
    pub success: bool,
}

/// End-to-end pipeline tester
#[derive(Debug)]
pub struct PipelineTester {
    pub temp_dir: PathBuf,
    pub compiler_path: PathBuf,
    pub renderer_paths: HashMap<IntegrationBackend, PathBuf>,
    pub results: Vec<IntegrationTestResult>,
}

/// Integration test suite
#[derive(Debug)]
pub struct IntegrationTestSuite {
    pub tests: Vec<IntegrationTestConfig>,
    pub pipeline_tester: PipelineTester,
    pub config: TestConfig,
}

impl Default for IntegrationTestConfig {
    fn default() -> Self {
        Self {
            name: "integration_test".to_string(),
            source_file: None,
            expected_output: None,
            timeout: Duration::from_secs(30),
            backend: IntegrationBackend::Ratatui,
            validate_rendering: true,
        }
    }
}

impl IntegrationTestConfig {
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            ..Default::default()
        }
    }

    pub fn with_source_file(mut self, path: impl Into<PathBuf>) -> Self {
        self.source_file = Some(path.into());
        self
    }

    pub fn with_expected_output(mut self, output: impl Into<String>) -> Self {
        self.expected_output = Some(output.into());
        self
    }

    pub fn with_backend(mut self, backend: IntegrationBackend) -> Self {
        self.backend = backend;
        self
    }

    pub fn with_timeout(mut self, timeout: Duration) -> Self {
        self.timeout = timeout;
        self
    }

    pub fn without_rendering_validation(mut self) -> Self {
        self.validate_rendering = false;
        self
    }
}

impl PipelineTester {
    pub fn new(temp_dir: impl Into<PathBuf>) -> Result<Self> {
        let temp_dir = temp_dir.into();
        fs::create_dir_all(&temp_dir)?;
        
        // Locate compiler and renderer binaries
        let compiler_path = Self::find_binary("kryon-compiler", "kryc")?;
        
        let mut renderer_paths = HashMap::new();
        
        if let Ok(ratatui_path) = Self::find_binary("kryon-ratatui", "kryon-renderer-ratatui") {
            renderer_paths.insert(IntegrationBackend::Ratatui, ratatui_path);
        }
        
        if let Ok(raylib_path) = Self::find_binary("kryon-raylib", "kryon-renderer-raylib") {
            renderer_paths.insert(IntegrationBackend::Raylib, raylib_path);
        }
        
        Ok(Self {
            temp_dir,
            compiler_path,
            renderer_paths,
            results: Vec::new(),
        })
    }

    fn find_binary(package_name: &str, binary_name: &str) -> Result<PathBuf> {
        // First try target/debug
        let debug_path = PathBuf::from("target/debug").join(binary_name);
        if debug_path.exists() {
            return Ok(debug_path);
        }
        
        // Then try target/release
        let release_path = PathBuf::from("target/release").join(binary_name);
        if release_path.exists() {
            return Ok(release_path);
        }
        
        // Try building it
        println!("Building {} binary...", package_name);
        let output = Command::new("cargo")
            .args(&["build", "--package", package_name, "--bin", binary_name])
            .output()
            .context("Failed to build binary")?;
        
        if !output.status.success() {
            bail!(
                "Failed to build {}: {}",
                binary_name,
                String::from_utf8_lossy(&output.stderr)
            );
        }
        
        if debug_path.exists() {
            Ok(debug_path)
        } else {
            bail!("Binary {} not found after build", binary_name)
        }
    }

    /// Run a complete pipeline test
    pub async fn run_pipeline_test(&mut self, config: IntegrationTestConfig) -> Result<IntegrationTestResult> {
        let start_time = std::time::Instant::now();
        let mut errors = Vec::new();
        let mut compilation_success = false;
        let mut rendering_success = false;
        let mut output = String::new();
        
        // Step 1: Prepare source file
        let source_path = if let Some(source_file) = &config.source_file {
            source_file.clone()
        } else {
            // Create temporary source file from fixture
            let temp_source = self.temp_dir.join(format!("{}.kry", config.name));
            let fixture_manager = FixtureManager::new("fixtures");
            if let Some(fixture) = fixture_manager.get_fixture(&config.name) {
                fs::write(&temp_source, &fixture.source)?;
            } else {
                bail!("No source file or fixture found for test: {}", config.name);
            }
            temp_source
        };
        
        // Step 2: Compile KRY to KRB
        let krb_path = self.temp_dir.join(format!("{}.krb", config.name));
        match self.compile_source(&source_path, &krb_path).await {
            Ok(_) => {
                compilation_success = true;
                println!("✅ Compilation successful: {}", config.name);
            },
            Err(e) => {
                errors.push(format!("Compilation failed: {}", e));
                println!("❌ Compilation failed: {} - {}", config.name, e);
            }
        }
        
        // Step 3: Render KRB file (if compilation succeeded)
        if compilation_success && config.validate_rendering {
            match self.render_krb_file(&config, &krb_path).await {
                Ok(render_output) => {
                    rendering_success = true;
                    output = render_output;
                    println!("✅ Rendering successful: {}", config.name);
                },
                Err(e) => {
                    errors.push(format!("Rendering failed: {}", e));
                    println!("❌ Rendering failed: {} - {}", config.name, e);
                }
            }
        }
        
        // Step 4: Validate output (if expected)
        if let Some(expected_output) = &config.expected_output {
            if !output.contains(expected_output) {
                errors.push(format!(
                    "Output validation failed: expected '{}' not found in output", 
                    expected_output
                ));
            }
        }
        
        let success = compilation_success && 
                     (!config.validate_rendering || rendering_success) && 
                     errors.is_empty();
        
        let result = IntegrationTestResult {
            config,
            compilation_success,
            rendering_success,
            output,
            errors,
            execution_time: start_time.elapsed(),
            success,
        };
        
        self.results.push(result.clone());
        Ok(result)
    }

    async fn compile_source(&self, source_path: &Path, output_path: &Path) -> Result<()> {
        let output = Command::new(&self.compiler_path)
            .args(&[
                source_path.to_str().unwrap(),
                output_path.to_str().unwrap(),
            ])
            .output()
            .context("Failed to execute compiler")?;
        
        if !output.status.success() {
            bail!(
                "Compiler failed: {}",
                String::from_utf8_lossy(&output.stderr)
            );
        }
        
        if !output_path.exists() {
            bail!("Compiler did not produce output file: {}", output_path.display());
        }
        
        Ok(())
    }

    async fn render_krb_file(&self, config: &IntegrationTestConfig, krb_path: &Path) -> Result<String> {
        let backends_to_test = match config.backend {
            IntegrationBackend::All => vec![IntegrationBackend::Ratatui, IntegrationBackend::Raylib],
            backend => vec![backend],
        };
        
        let mut outputs = Vec::new();
        
        for backend in backends_to_test {
            if let Some(renderer_path) = self.renderer_paths.get(&backend) {
                let output = self.run_renderer(renderer_path, krb_path, &config.timeout).await?;
                outputs.push(format!("=== {:?} Backend ===\n{}", backend, output));
            } else {
                bail!("Renderer binary not found for backend: {:?}", backend);
            }
        }
        
        Ok(outputs.join("\n\n"))
    }

    async fn run_renderer(&self, renderer_path: &Path, krb_path: &Path, timeout: &Duration) -> Result<String> {
        let mut child = Command::new(renderer_path)
            .arg(krb_path)
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
            .context("Failed to start renderer")?;
        
        // Wait for completion with timeout
        let output = tokio::time::timeout(*timeout, async {
            child.wait_with_output()
        }).await
        .context("Renderer timed out")?
        .context("Failed to get renderer output")?;
        
        if !output.status.success() {
            bail!(
                "Renderer failed: {}",
                String::from_utf8_lossy(&output.stderr)
            );
        }
        
        Ok(String::from_utf8_lossy(&output.stdout).to_string())
    }

    /// Validate KRB file structure
    pub fn validate_krb_file(&self, krb_path: &Path) -> Result<()> {
        let krb_data = fs::read(krb_path)
            .with_context(|| format!("Failed to read KRB file: {}", krb_path.display()))?;
        
        let krb_file = KrbFile::parse(&krb_data)
            .with_context(|| format!("Failed to parse KRB file: {}", krb_path.display()))?;
        
        // Basic structural validation
        if krb_file.elements.is_empty() {
            bail!("KRB file contains no elements");
        }
        
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
        
        Ok(())
    }

    /// Print test results summary
    pub fn print_results(&self) {
        println!("\n=== Integration Test Results ===");
        
        let total = self.results.len();
        let passed = self.results.iter().filter(|r| r.success).count();
        let compilation_failures = self.results.iter()
            .filter(|r| !r.compilation_success)
            .count();
        let rendering_failures = self.results.iter()
            .filter(|r| r.compilation_success && !r.rendering_success)
            .count();
        
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
                for error in &result.errors {
                    println!("  ❌ {}", error);
                }
            }
            
            if result.compilation_success {
                println!("  ✅ Compilation");
            }
            
            if result.rendering_success {
                println!("  ✅ Rendering");
            }
        }
        
        println!("\n=== Summary ===");
        println!("Total tests: {}", total);
        println!("Passed: {}", passed);
        println!("Failed: {}", total - passed);
        println!("Compilation failures: {}", compilation_failures);
        println!("Rendering failures: {}", rendering_failures);
        
        if passed == total {
            println!("✅ All integration tests passed!");
        } else {
            println!("❌ {} integration tests failed", total - passed);
        }
    }

    /// Clean up temporary files
    pub fn cleanup(&self) -> Result<()> {
        if self.temp_dir.exists() {
            fs::remove_dir_all(&self.temp_dir)?;
        }
        Ok(())
    }
}

impl IntegrationTestSuite {
    pub fn new(config: TestConfig) -> Result<Self> {
        let temp_dir = config.output_directory.join("integration_tests");
        let pipeline_tester = PipelineTester::new(temp_dir)?;
        
        Ok(Self {
            tests: Vec::new(),
            pipeline_tester,
            config,
        })
    }

    pub fn add_test(&mut self, test: IntegrationTestConfig) {
        self.tests.push(test);
    }

    /// Add standard integration tests
    pub fn add_standard_tests(&mut self) {
        // Basic app test
        self.add_test(
            IntegrationTestConfig::new("basic_app")
                .with_backend(IntegrationBackend::Ratatui)
                .with_expected_output("Hello World")
        );
        
        // Layout test
        self.add_test(
            IntegrationTestConfig::new("flex_layout")
                .with_backend(IntegrationBackend::Ratatui)
                .with_expected_output("Start")
        );
        
        // Theme test
        self.add_test(
            IntegrationTestConfig::new("themed_app")
                .with_backend(IntegrationBackend::Ratatui)
                .with_expected_output("Themed Text")
        );
        
        // Component test
        self.add_test(
            IntegrationTestConfig::new("custom_component")
                .with_backend(IntegrationBackend::Ratatui)
                .with_expected_output("Test Card")
        );
        
        // All backend test (if available)
        self.add_test(
            IntegrationTestConfig::new("basic_app")
                .with_backend(IntegrationBackend::All)
                .with_expected_output("Hello World")
        );
    }

    /// Run all integration tests
    pub async fn run_all(&mut self) -> Result<()> {
        println!("\n=== Running Integration Test Suite ===");
        println!("Total tests: {}", self.tests.len());
        
        for test in &self.tests.clone() {
            match self.pipeline_tester.run_pipeline_test(test.clone()).await {
                Ok(_) => {},
                Err(e) => {
                    eprintln!("Integration test '{}' failed: {}", test.name, e);
                }
            }
        }
        
        self.pipeline_tester.print_results();
        Ok(())
    }

    /// Clean up test artifacts
    pub fn cleanup(&self) -> Result<()> {
        self.pipeline_tester.cleanup()
    }
}

/// Create a comprehensive integration test suite
pub fn create_integration_test_suite() -> Result<IntegrationTestSuite> {
    let mut suite = IntegrationTestSuite::new(TestConfig::default())?;
    suite.add_standard_tests();
    Ok(suite)
}

/// Utility macro for integration tests
#[macro_export]
macro_rules! kryon_integration_test {
    ($test_name:expr, $source_file:expr) => {
        #[tokio::test]
        async fn integration_test() -> anyhow::Result<()> {
            let temp_dir = tempfile::tempdir()?;
            let mut pipeline_tester = crate::integration::PipelineTester::new(temp_dir.path())?;
            
            let config = crate::integration::IntegrationTestConfig::new($test_name)
                .with_source_file($source_file)
                .with_backend(crate::integration::IntegrationBackend::Ratatui);
            
            let result = pipeline_tester.run_pipeline_test(config).await?;
            
            if !result.success {
                anyhow::bail!("Integration test failed: {:?}", result.errors);
            }
            
            Ok(())
        }
    };
}

pub use kryon_integration_test;
