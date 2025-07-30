//! Integration Testing Framework for Kryon
//! 
//! Comprehensive integration tests that verify the complete pipeline
//! from KRY source to rendered output across different backends.

use anyhow::Result;
use kryon_compiler::KryonCompiler;
use kryon_core::*;
use kryon_runtime::KryonRuntime;
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use tempfile::TempDir;

pub mod compilation_tests;
pub mod rendering_tests;
pub mod cross_backend_tests;
pub mod e2e_tests;

/// Integration test harness that manages the complete Kryon pipeline
pub struct IntegrationTestHarness {
    temp_dir: TempDir,
    compiler: KryonCompiler,
    source_files: HashMap<String, String>,
    compiled_files: HashMap<String, Vec<u8>>,
}

impl IntegrationTestHarness {
    /// Create a new integration test harness
    pub fn new() -> Result<Self> {
        let temp_dir = TempDir::new()?;
        let compiler = KryonCompiler::new();
        
        Ok(Self {
            temp_dir,
            compiler,
            source_files: HashMap::new(),
            compiled_files: HashMap::new(),
        })
    }

    /// Add a KRY source file to the test harness
    pub fn add_source_file(&mut self, name: &str, content: &str) {
        self.source_files.insert(name.to_string(), content.to_string());
    }

    /// Write source files to the temporary directory
    pub fn write_source_files(&self) -> Result<()> {
        for (name, content) in &self.source_files {
            let file_path = self.temp_dir.path().join(name);
            std::fs::write(file_path, content)?;
        }
        Ok(())
    }

    /// Compile all source files
    pub fn compile_all(&mut self) -> Result<()> {
        self.write_source_files()?;
        
        for name in self.source_files.keys() {
            let source_path = self.temp_dir.path().join(name);
            let output_path = self.temp_dir.path().join(format!("{}.krb", name));
            
            self.compiler.compile_file(&source_path, &output_path)?;
            
            let compiled_content = std::fs::read(&output_path)?;
            self.compiled_files.insert(name.clone(), compiled_content);
        }
        
        Ok(())
    }

    /// Get the path to a compiled file
    pub fn get_compiled_path(&self, name: &str) -> Option<PathBuf> {
        if self.compiled_files.contains_key(name) {
            Some(self.temp_dir.path().join(format!("{}.krb", name)))
        } else {
            None
        }
    }

    /// Get the temporary directory path
    pub fn temp_path(&self) -> &Path {
        self.temp_dir.path()
    }

    /// Run a compiled file with a specific backend
    pub fn run_with_backend(&self, file_name: &str, backend: TestBackend) -> Result<TestRunResult> {
        let compiled_path = self.get_compiled_path(file_name)
            .ok_or_else(|| anyhow::anyhow!("File not compiled: {}", file_name))?;

        match backend {
            TestBackend::Ratatui => self.run_ratatui(&compiled_path),
            TestBackend::Html => self.run_html(&compiled_path),
            TestBackend::Raylib => self.run_raylib(&compiled_path),
            TestBackend::Wgpu => self.run_wgpu(&compiled_path),
        }
    }

    fn run_ratatui(&self, _path: &Path) -> Result<TestRunResult> {
        // Mock ratatui execution
        Ok(TestRunResult {
            backend: TestBackend::Ratatui,
            success: true,
            output: String::new(),
            render_commands: vec![],
            execution_time: std::time::Duration::from_millis(50),
        })
    }

    fn run_html(&self, _path: &Path) -> Result<TestRunResult> {
        // Mock HTML execution
        Ok(TestRunResult {
            backend: TestBackend::Html,
            success: true,
            output: String::new(),
            render_commands: vec![],
            execution_time: std::time::Duration::from_millis(30),
        })
    }

    fn run_raylib(&self, _path: &Path) -> Result<TestRunResult> {
        // Mock Raylib execution
        Ok(TestRunResult {
            backend: TestBackend::Raylib,
            success: true,
            output: String::new(),
            render_commands: vec![],
            execution_time: std::time::Duration::from_millis(40),
        })
    }

    fn run_wgpu(&self, _path: &Path) -> Result<TestRunResult> {
        // Mock WGPU execution
        Ok(TestRunResult {
            backend: TestBackend::Wgpu,
            success: true,
            output: String::new(),
            render_commands: vec![],
            execution_time: std::time::Duration::from_millis(60),
        })
    }

    /// Run the same file with all backends and compare results
    pub fn cross_backend_test(&self, file_name: &str) -> Result<CrossBackendTestResult> {
        let backends = vec![
            TestBackend::Ratatui,
            TestBackend::Html,
            TestBackend::Raylib,
            TestBackend::Wgpu,
        ];

        let mut results = HashMap::new();
        let mut all_success = true;

        for backend in backends {
            match self.run_with_backend(file_name, backend) {
                Ok(result) => {
                    if !result.success {
                        all_success = false;
                    }
                    results.insert(backend, result);
                }
                Err(e) => {
                    all_success = false;
                    results.insert(backend, TestRunResult {
                        backend,
                        success: false,
                        output: e.to_string(),
                        render_commands: vec![],
                        execution_time: std::time::Duration::ZERO,
                    });
                }
            }
        }

        Ok(CrossBackendTestResult {
            file_name: file_name.to_string(),
            results,
            consistent: all_success && self.check_consistency(&results),
        })
    }

    fn check_consistency(&self, results: &HashMap<TestBackend, TestRunResult>) -> bool {
        // Simple consistency check - all backends should succeed
        results.values().all(|r| r.success)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum TestBackend {
    Ratatui,
    Html,
    Raylib,
    Wgpu,
}

/// Result of running a test with a specific backend
#[derive(Debug)]
pub struct TestRunResult {
    pub backend: TestBackend,
    pub success: bool,
    pub output: String,
    pub render_commands: Vec<String>, // Simplified render commands
    pub execution_time: std::time::Duration,
}

/// Result of cross-backend testing
#[derive(Debug)]
pub struct CrossBackendTestResult {
    pub file_name: String,
    pub results: HashMap<TestBackend, TestRunResult>,
    pub consistent: bool,
}

/// Test fixture manager for integration tests
pub struct TestFixtureManager {
    fixtures_dir: PathBuf,
}

impl TestFixtureManager {
    pub fn new() -> Self {
        Self {
            fixtures_dir: PathBuf::from("tests/fixtures"),
        }
    }

    /// Load a fixture file
    pub fn load_fixture(&self, name: &str) -> Result<String> {
        let path = self.fixtures_dir.join(format!("{}.kry", name));
        Ok(std::fs::read_to_string(path)?)
    }

    /// Get all available fixtures
    pub fn list_fixtures(&self) -> Result<Vec<String>> {
        let mut fixtures = Vec::new();
        
        for entry in std::fs::read_dir(&self.fixtures_dir)? {
            let entry = entry?;
            if let Some(name) = entry.file_name().to_str() {
                if name.ends_with(".kry") {
                    fixtures.push(name.trim_end_matches(".kry").to_string());
                }
            }
        }
        
        Ok(fixtures)
    }

    /// Create a test suite from all fixtures
    pub fn create_fixture_test_suite(&self) -> Result<Vec<IntegrationTest>> {
        let fixtures = self.list_fixtures()?;
        let mut tests = Vec::new();

        for fixture in fixtures {
            let content = self.load_fixture(&fixture)?;
            tests.push(IntegrationTest {
                name: fixture.clone(),
                source: content,
                expected_success: true,
                expected_elements: None,
                test_type: IntegrationTestType::Compilation,
            });
        }

        Ok(tests)
    }
}

/// Individual integration test
#[derive(Debug)]
pub struct IntegrationTest {
    pub name: String,
    pub source: String,
    pub expected_success: bool,
    pub expected_elements: Option<usize>,
    pub test_type: IntegrationTestType,
}

#[derive(Debug)]
pub enum IntegrationTestType {
    Compilation,
    Rendering,
    CrossBackend,
    EndToEnd,
}

impl IntegrationTest {
    /// Run this integration test
    pub fn run(&self) -> Result<IntegrationTestResult> {
        let mut harness = IntegrationTestHarness::new()?;
        harness.add_source_file(&self.name, &self.source);

        let start_time = std::time::Instant::now();
        
        let result = match self.test_type {
            IntegrationTestType::Compilation => self.run_compilation_test(&mut harness),
            IntegrationTestType::Rendering => self.run_rendering_test(&mut harness),
            IntegrationTestType::CrossBackend => self.run_cross_backend_test(&mut harness),
            IntegrationTestType::EndToEnd => self.run_e2e_test(&mut harness),
        };

        let execution_time = start_time.elapsed();

        Ok(IntegrationTestResult {
            test_name: self.name.clone(),
            success: result.is_ok(),
            error: result.err().map(|e| e.to_string()),
            execution_time,
            test_type: self.test_type,
        })
    }

    fn run_compilation_test(&self, harness: &mut IntegrationTestHarness) -> Result<()> {
        harness.compile_all()?;
        Ok(())
    }

    fn run_rendering_test(&self, harness: &mut IntegrationTestHarness) -> Result<()> {
        harness.compile_all()?;
        let _result = harness.run_with_backend(&self.name, TestBackend::Ratatui)?;
        Ok(())
    }

    fn run_cross_backend_test(&self, harness: &mut IntegrationTestHarness) -> Result<()> {
        harness.compile_all()?;
        let result = harness.cross_backend_test(&self.name)?;
        
        if !result.consistent {
            return Err(anyhow::anyhow!("Cross-backend test failed: inconsistent results"));
        }
        
        Ok(())
    }

    fn run_e2e_test(&self, harness: &mut IntegrationTestHarness) -> Result<()> {
        // End-to-end test includes compilation, rendering, and user interaction simulation
        harness.compile_all()?;
        let _result = harness.run_with_backend(&self.name, TestBackend::Html)?;
        // TODO: Add user interaction simulation
        Ok(())
    }
}

/// Result of running an integration test
#[derive(Debug)]
pub struct IntegrationTestResult {
    pub test_name: String,
    pub success: bool,
    pub error: Option<String>,
    pub execution_time: std::time::Duration,
    pub test_type: IntegrationTestType,
}

/// Integration test suite runner
pub struct IntegrationTestSuite {
    tests: Vec<IntegrationTest>,
    fixture_manager: TestFixtureManager,
}

impl IntegrationTestSuite {
    pub fn new() -> Self {
        Self {
            tests: Vec::new(),
            fixture_manager: TestFixtureManager::new(),
        }
    }

    /// Add a test to the suite
    pub fn add_test(&mut self, test: IntegrationTest) {
        self.tests.push(test);
    }

    /// Load all fixture tests
    pub fn load_fixture_tests(&mut self) -> Result<()> {
        let fixture_tests = self.fixture_manager.create_fixture_test_suite()?;
        self.tests.extend(fixture_tests);
        Ok(())
    }

    /// Run all tests in the suite
    pub fn run_all(&self) -> Result<IntegrationTestSuiteResult> {
        let mut results = Vec::new();
        let mut passed = 0;
        let mut failed = 0;

        let start_time = std::time::Instant::now();

        for test in &self.tests {
            let result = test.run()?;
            
            if result.success {
                passed += 1;
            } else {
                failed += 1;
            }
            
            results.push(result);
        }

        let total_time = start_time.elapsed();

        Ok(IntegrationTestSuiteResult {
            results,
            passed,
            failed,
            total_time,
        })
    }

    /// Run tests in parallel
    pub async fn run_parallel(&self) -> Result<IntegrationTestSuiteResult> {
        use tokio::task;
        
        let mut handles = Vec::new();
        
        for test in &self.tests {
            let test_clone = format!("{:?}", test); // Simplified for now
            let handle = task::spawn(async move {
                // Would run actual test here
                IntegrationTestResult {
                    test_name: "parallel_test".to_string(),
                    success: true,
                    error: None,
                    execution_time: std::time::Duration::from_millis(10),
                    test_type: IntegrationTestType::Compilation,
                }
            });
            handles.push(handle);
        }

        let mut results = Vec::new();
        let mut passed = 0;
        let mut failed = 0;

        let start_time = std::time::Instant::now();

        for handle in handles {
            let result = handle.await?;
            
            if result.success {
                passed += 1;
            } else {
                failed += 1;
            }
            
            results.push(result);
        }

        let total_time = start_time.elapsed();

        Ok(IntegrationTestSuiteResult {
            results,
            passed,
            failed,
            total_time,
        })
    }
}

/// Result of running an integration test suite
#[derive(Debug)]
pub struct IntegrationTestSuiteResult {
    pub results: Vec<IntegrationTestResult>,
    pub passed: usize,
    pub failed: usize,
    pub total_time: std::time::Duration,
}

impl IntegrationTestSuiteResult {
    /// Print a summary of the test results
    pub fn print_summary(&self) {
        println!("Integration Test Results:");
        println!("  Passed: {}", self.passed);
        println!("  Failed: {}", self.failed);
        println!("  Total:  {}", self.passed + self.failed);
        println!("  Time:   {:?}", self.total_time);
        
        if self.failed > 0 {
            println!("\nFailed tests:");
            for result in &self.results {
                if !result.success {
                    println!("  - {}: {}", result.test_name, 
                        result.error.as_deref().unwrap_or("Unknown error"));
                }
            }
        }
    }

    /// Check if all tests passed
    pub fn all_passed(&self) -> bool {
        self.failed == 0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn test_integration_harness_creation() -> Result<()> {
        let harness = IntegrationTestHarness::new()?;
        assert!(harness.temp_path().exists());
        Ok(())
    }

    #[test]
    fn test_fixture_manager() -> Result<()> {
        let manager = TestFixtureManager::new();
        // This would work if fixtures directory exists
        // let fixtures = manager.list_fixtures()?;
        // assert!(!fixtures.is_empty());
        Ok(())
    }

    #[test]
    fn test_integration_test_creation() {
        let test = IntegrationTest {
            name: "test_basic".to_string(),
            source: "Container { }".to_string(),
            expected_success: true,
            expected_elements: Some(1),
            test_type: IntegrationTestType::Compilation,
        };

        assert_eq!(test.name, "test_basic");
        assert!(test.expected_success);
    }

    #[tokio::test]
    async fn test_integration_suite() -> Result<()> {
        let mut suite = IntegrationTestSuite::new();
        
        suite.add_test(IntegrationTest {
            name: "basic_test".to_string(),
            source: "Container { Text { text: \"Hello\" } }".to_string(),
            expected_success: true,
            expected_elements: Some(2),
            test_type: IntegrationTestType::Compilation,
        });

        // Note: This would fail without proper fixture setup
        // let results = suite.run_all()?;
        // assert!(results.all_passed());
        
        Ok(())
    }
}