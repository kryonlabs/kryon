//! Test runners for different types of tests

use crate::prelude::*;
use std::sync::Arc;
use futures::future::join_all;

/// Test case builder for fluent test creation
#[derive(Debug, Clone)]
pub struct TestCase {
    pub name: String,
    pub source: String,
    pub expected_elements: Option<usize>,
    pub expected_properties: Vec<(String, PropertyValue)>,
    pub expected_errors: Vec<String>,
    pub expected_text_content: Vec<String>,
    pub timeout: Duration,
    pub should_fail: bool,
    pub backend: TestBackend,
    pub category: String,
}

/// Available test backends
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum TestBackend {
    Ratatui,
    Raylib,
    All,
}

/// Test execution results
#[derive(Debug, Clone)]
pub struct TestExecutionResult {
    pub test_case: TestCase,
    pub compilation_result: Result<Vec<u8>>,
    pub parse_result: Option<Result<KrbFile>>,
    pub element_count: Option<usize>,
    pub execution_time: Duration,
    pub memory_usage: Option<usize>,
    pub errors: Vec<String>,
    pub success: bool,
}

/// Batch test runner for executing multiple tests
#[derive(Debug)]
pub struct BatchTestRunner {
    pub config: TestConfig,
    pub fixtures: Arc<FixtureManager>,
    pub results: Vec<TestExecutionResult>,
}

/// Sequential test runner for debugging
#[derive(Debug)]
pub struct SequentialTestRunner {
    pub config: TestConfig,
    pub current_test: Option<TestCase>,
}

impl TestCase {
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            source: String::new(),
            expected_elements: None,
            expected_properties: Vec::new(),
            expected_errors: Vec::new(),
            expected_text_content: Vec::new(),
            timeout: Duration::from_secs(30),
            should_fail: false,
            backend: TestBackend::Ratatui,
            category: "general".to_string(),
        }
    }

    pub fn with_source(mut self, source: impl Into<String>) -> Self {
        self.source = source.into();
        self
    }

    pub fn expect_compilation_success(mut self) -> Self {
        self.should_fail = false;
        self
    }

    pub fn expect_compilation_failure(mut self) -> Self {
        self.should_fail = true;
        self
    }

    pub fn expect_elements(mut self, count: usize) -> Self {
        self.expected_elements = Some(count);
        self
    }

    pub fn expect_property(mut self, name: impl Into<String>, value: PropertyValue) -> Self {
        self.expected_properties.push((name.into(), value));
        self
    }

    pub fn expect_text_content(mut self, text: impl Into<String>) -> Self {
        self.expected_text_content.push(text.into());
        self
    }

    pub fn expect_error(mut self, error: impl Into<String>) -> Self {
        self.expected_errors.push(error.into());
        self
    }

    pub fn with_timeout(mut self, timeout: Duration) -> Self {
        self.timeout = timeout;
        self
    }

    pub fn with_backend(mut self, backend: TestBackend) -> Self {
        self.backend = backend;
        self
    }

    pub fn with_category(mut self, category: impl Into<String>) -> Self {
        self.category = category.into();
        self
    }

    /// Run this test case
    pub async fn run(self) -> Result<TestExecutionResult> {
        let start_time = std::time::Instant::now();
        let mut errors = Vec::new();
        let mut success = true;

        // Compile the source
        let compilation_result = self.compile_source();
        
        // Check compilation expectations
        if self.should_fail {
            if compilation_result.is_ok() {
                errors.push("Expected compilation to fail, but it succeeded".to_string());
                success = false;
            }
        } else if let Err(ref e) = compilation_result {
            errors.push(format!("Compilation failed: {}", e));
            success = false;
        }

        // Parse KRB if compilation succeeded
        let parse_result = if let Ok(ref krb_data) = compilation_result {
            Some(self.parse_krb(krb_data))
        } else {
            None
        };

        // Validate element count
        let element_count = if let Some(Ok(ref krb_file)) = parse_result {
            let count = krb_file.elements.len();
            if let Some(expected) = self.expected_elements {
                if count != expected {
                    errors.push(format!("Expected {} elements, found {}", expected, count));
                    success = false;
                }
            }
            Some(count)
        } else {
            None
        };

        // Validate properties
        if let Some(Ok(ref krb_file)) = parse_result {
            for (expected_prop, expected_value) in &self.expected_properties {
                let found = krb_file.elements.iter().any(|element| {
                    element.custom_properties.get(expected_prop) == Some(expected_value)
                });
                if !found {
                    errors.push(format!("Expected property '{}' with value '{:?}' not found", expected_prop, expected_value));
                    success = false;
                }
            }
        }

        // Validate text content
        if let Some(Ok(ref krb_file)) = parse_result {
            for expected_text in &self.expected_text_content {
                let found = krb_file.elements.iter().any(|element| {
                    element.custom_properties.get("text") == Some(&PropertyValue::String(expected_text.clone()))
                });
                if !found {
                    errors.push(format!("Expected text content '{}' not found", expected_text));
                    success = false;
                }
            }
        }

        let execution_time = start_time.elapsed();

        Ok(TestExecutionResult {
            test_case: self,
            compilation_result,
            parse_result,
            element_count,
            execution_time,
            memory_usage: None,
            errors,
            success,
        })
    }

    fn compile_source(&self) -> Result<Vec<u8>> {
        let options = CompilerOptions::default();
        compile_string(&self.source, options)
            .map_err(|e| anyhow::anyhow!("Compilation failed: {}", e))
    }

    fn parse_krb(&self, krb_data: &[u8]) -> Result<KrbFile> {
        KrbFile::parse(krb_data)
            .map_err(|e| anyhow::anyhow!("KRB parsing failed: {}", e))
    }
}

impl BatchTestRunner {
    pub fn new(config: TestConfig, fixtures: Arc<FixtureManager>) -> Self {
        Self {
            config,
            fixtures,
            results: Vec::new(),
        }
    }

    /// Run all fixtures in parallel
    pub async fn run_all_fixtures(&mut self) -> Result<()> {
        let test_cases: Vec<TestCase> = self.fixtures.all_fixtures()
            .map(|fixture| {
                let mut test_case = TestCase::new(&fixture.name)
                    .with_source(&fixture.source)
                    .with_category(format!("{:?}", fixture.category));

                if let Some(expected_elements) = fixture.expected_elements {
                    test_case = test_case.expect_elements(expected_elements);
                }

                for (prop_name, prop_value) in &fixture.expected_properties {
                    test_case = test_case.expect_property(prop_name, prop_value.clone());
                }

                if fixture.should_fail() {
                    test_case = test_case.expect_compilation_failure();
                }

                test_case
            })
            .collect();

        self.run_test_cases(test_cases).await
    }

    /// Run specific test cases
    pub async fn run_test_cases(&mut self, test_cases: Vec<TestCase>) -> Result<()> {
        if self.config.parallel_execution {
            self.run_parallel(test_cases).await
        } else {
            self.run_sequential(test_cases).await
        }
    }

    async fn run_parallel(&mut self, test_cases: Vec<TestCase>) -> Result<()> {
        let futures: Vec<_> = test_cases.into_iter()
            .map(|test_case| async move {
                tokio::time::timeout(
                    Duration::from_secs(self.config.timeout_seconds),
                    test_case.run()
                ).await
            })
            .collect();

        let results = join_all(futures).await;
        
        for result in results {
            match result {
                Ok(Ok(test_result)) => self.results.push(test_result),
                Ok(Err(e)) => {
                    eprintln!("Test execution error: {}", e);
                }
                Err(_) => {
                    eprintln!("Test timeout");
                }
            }
        }

        Ok(())
    }

    async fn run_sequential(&mut self, test_cases: Vec<TestCase>) -> Result<()> {
        for test_case in test_cases {
            match tokio::time::timeout(
                Duration::from_secs(self.config.timeout_seconds),
                test_case.run()
            ).await {
                Ok(Ok(result)) => self.results.push(result),
                Ok(Err(e)) => eprintln!("Test '{}' failed: {}", test_case.name, e),
                Err(_) => eprintln!("Test '{}' timed out", test_case.name),
            }
        }
        Ok(())
    }

    /// Get test summary
    pub fn summary(&self) -> TestSummary {
        let total = self.results.len();
        let passed = self.results.iter().filter(|r| r.success).count();
        let failed = total - passed;
        
        let total_time: Duration = self.results.iter()
            .map(|r| r.execution_time)
            .sum();
        
        let average_time = if total > 0 {
            total_time / total as u32
        } else {
            Duration::ZERO
        };

        TestSummary {
            total,
            passed,
            failed,
            total_time,
            average_time,
            slowest_test: self.results.iter()
                .max_by_key(|r| r.execution_time)
                .map(|r| (r.test_case.name.clone(), r.execution_time)),
        }
    }

    /// Print detailed results
    pub fn print_results(&self) {
        println!("\n=== Test Results ===");
        
        for result in &self.results {
            let status = if result.success {
                "PASS".green()
            } else {
                "FAIL".red()
            };
            
            println!(
                "[{}] {} ({:.2}ms)",
                status,
                result.test_case.name,
                result.execution_time.as_secs_f64() * 1000.0
            );
            
            if !result.errors.is_empty() {
                for error in &result.errors {
                    println!("  ❌ {}", error);
                }
            }
            
            if let Some(count) = result.element_count {
                println!("  📊 {} elements parsed", count);
            }
        }
        
        let summary = self.summary();
        println!("\n=== Summary ===");
        println!("Total: {}, Passed: {}, Failed: {}", summary.total, summary.passed, summary.failed);
        println!("Total time: {:.2}ms", summary.total_time.as_secs_f64() * 1000.0);
        println!("Average time: {:.2}ms", summary.average_time.as_secs_f64() * 1000.0);
        
        if let Some((slowest_name, slowest_time)) = summary.slowest_test {
            println!("Slowest test: {} ({:.2}ms)", slowest_name, slowest_time.as_secs_f64() * 1000.0);
        }
    }
}

/// Test execution summary
#[derive(Debug, Clone)]
pub struct TestSummary {
    pub total: usize,
    pub passed: usize,
    pub failed: usize,
    pub total_time: Duration,
    pub average_time: Duration,
    pub slowest_test: Option<(String, Duration)>,
}

impl SequentialTestRunner {
    pub fn new(config: TestConfig) -> Self {
        Self {
            config,
            current_test: None,
        }
    }

    pub async fn run_single(&mut self, test_case: TestCase) -> Result<TestExecutionResult> {
        self.current_test = Some(test_case.clone());
        
        println!("Running test: {}", test_case.name);
        let result = test_case.run().await?;
        
        if result.success {
            println!("✅ Test passed in {:.2}ms", result.execution_time.as_secs_f64() * 1000.0);
        } else {
            println!("❌ Test failed in {:.2}ms", result.execution_time.as_secs_f64() * 1000.0);
            for error in &result.errors {
                println!("   {}", error);
            }
        }
        
        self.current_test = None;
        Ok(result)
    }
}

/// Create a test case from a fixture
pub fn test_case_from_fixture(fixture: &TestFixture) -> TestCase {
    let mut test_case = TestCase::new(&fixture.name)
        .with_source(&fixture.source)
        .with_category(format!("{:?}", fixture.category));

    if let Some(expected_elements) = fixture.expected_elements {
        test_case = test_case.expect_elements(expected_elements);
    }

    for (prop_name, prop_value) in &fixture.expected_properties {
        test_case = test_case.expect_property(prop_name, prop_value.clone());
    }

    if fixture.should_fail() {
        test_case = test_case.expect_compilation_failure();
    }

    test_case
}
