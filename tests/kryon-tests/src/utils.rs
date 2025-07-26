//! Testing utilities and helper functions

use crate::prelude::*;
use colored::Colorize;
use std::sync::atomic::{AtomicUsize, Ordering};

/// Global test counter for unique test IDs
static TEST_COUNTER: AtomicUsize = AtomicUsize::new(0);

/// Generate a unique test ID
pub fn generate_test_id() -> String {
    let id = TEST_COUNTER.fetch_add(1, Ordering::SeqCst);
    format!("test_{:06}", id)
}

/// Test execution context with timing and metadata
#[derive(Debug, Clone)]
pub struct TestContext {
    pub test_id: String,
    pub test_name: String,
    pub start_time: std::time::Instant,
    pub temp_dir: Option<PathBuf>,
    pub metadata: HashMap<String, String>,
}

impl TestContext {
    pub fn new(test_name: impl Into<String>) -> Self {
        Self {
            test_id: generate_test_id(),
            test_name: test_name.into(),
            start_time: std::time::Instant::now(),
            temp_dir: None,
            metadata: HashMap::new(),
        }
    }

    pub fn with_temp_dir(mut self) -> Result<Self> {
        let temp_dir = tempfile::tempdir()?;
        self.temp_dir = Some(temp_dir.into_path());
        Ok(self)
    }

    pub fn temp_path(&self) -> Result<&Path> {
        self.temp_dir.as_ref()
            .map(|p| p.as_path())
            .ok_or_else(|| anyhow::anyhow!("No temp directory set"))
    }

    pub fn elapsed(&self) -> Duration {
        self.start_time.elapsed()
    }

    pub fn add_metadata(&mut self, key: impl Into<String>, value: impl Into<String>) {
        self.metadata.insert(key.into(), value.into());
    }
}

/// Test result with detailed information
#[derive(Debug, Clone)]
pub struct TestResult {
    pub context: TestContext,
    pub success: bool,
    pub error: Option<String>,
    pub assertions_passed: usize,
    pub assertions_failed: usize,
    pub output: Vec<String>,
}

impl TestResult {
    pub fn success(context: TestContext, assertions_passed: usize) -> Self {
        Self {
            context,
            success: true,
            error: None,
            assertions_passed,
            assertions_failed: 0,
            output: Vec::new(),
        }
    }

    pub fn failure(context: TestContext, error: String) -> Self {
        Self {
            context,
            success: false,
            error: Some(error),
            assertions_passed: 0,
            assertions_failed: 1,
            output: Vec::new(),
        }
    }

    pub fn print_summary(&self) {
        let status = if self.success {
            "PASS".green()
        } else {
            "FAIL".red()
        };

        println!(
            "[{}] {} ({:.2}ms)",
            status,
            self.context.test_name,
            self.context.elapsed().as_secs_f64() * 1000.0
        );

        if let Some(error) = &self.error {
            println!("  Error: {}", error.red());
        }

        if !self.output.is_empty() {
            println!("  Output:");
            for line in &self.output {
                println!("    {}", line);
            }
        }
    }
}

/// File utilities for test setup
pub mod files {
    use super::*;

    pub fn write_test_file(dir: &Path, name: &str, content: &str) -> Result<PathBuf> {
        let file_path = dir.join(name);
        fs::write(&file_path, content)?;
        Ok(file_path)
    }

    pub fn create_test_kry_file(dir: &Path, name: &str, content: &str) -> Result<PathBuf> {
        let filename = if name.ends_with(".kry") {
            name.to_string()
        } else {
            format!("{}.kry", name)
        };
        write_test_file(dir, &filename, content)
    }

    pub fn read_test_file(path: &Path) -> Result<String> {
        fs::read_to_string(path)
            .with_context(|| format!("Failed to read test file: {}", path.display()))
    }

    pub fn cleanup_test_files(dir: &Path) -> Result<()> {
        if dir.exists() {
            fs::remove_dir_all(dir)?;
        }
        Ok(())
    }
}

/// Assertion utilities with detailed error messages
pub mod assertions {
    use super::*;

    pub fn assert_file_exists(path: &Path) -> Result<()> {
        if !path.exists() {
            bail!("Expected file to exist: {}", path.display());
        }
        Ok(())
    }

    pub fn assert_file_not_exists(path: &Path) -> Result<()> {
        if path.exists() {
            bail!("Expected file to not exist: {}", path.display());
        }
        Ok(())
    }

    pub fn assert_file_contains(path: &Path, content: &str) -> Result<()> {
        let file_content = files::read_test_file(path)?;
        if !file_content.contains(content) {
            bail!(
                "File {} does not contain expected content: {}",
                path.display(),
                content
            );
        }
        Ok(())
    }

    pub fn assert_compilation_success(result: &Result<Vec<u8>>) -> Result<()> {
        match result {
            Ok(_) => Ok(()),
            Err(e) => bail!("Compilation failed: {}", e),
        }
    }

    pub fn assert_compilation_failure(result: &Result<Vec<u8>>) -> Result<()> {
        match result {
            Ok(_) => bail!("Expected compilation to fail, but it succeeded"),
            Err(_) => Ok(()),
        }
    }
}

/// Timing utilities for performance tests
pub mod timing {
    use super::*;

    pub struct Timer {
        start: std::time::Instant,
        name: String,
    }

    impl Timer {
        pub fn new(name: impl Into<String>) -> Self {
            Self {
                start: std::time::Instant::now(),
                name: name.into(),
            }
        }

        pub fn elapsed(&self) -> Duration {
            self.start.elapsed()
        }

        pub fn elapsed_ms(&self) -> f64 {
            self.elapsed().as_secs_f64() * 1000.0
        }

        pub fn finish(self) -> Duration {
            let elapsed = self.elapsed();
            println!("{}: {:.2}ms", self.name, elapsed.as_secs_f64() * 1000.0);
            elapsed
        }
    }

    pub fn measure<F, R>(name: &str, f: F) -> (R, Duration)
    where
        F: FnOnce() -> R,
    {
        let start = std::time::Instant::now();
        let result = f();
        let elapsed = start.elapsed();
        println!("{}: {:.2}ms", name, elapsed.as_secs_f64() * 1000.0);
        (result, elapsed)
    }
}

/// Environment setup for consistent testing
pub fn setup_test_environment() {
    // Initialize logging for tests
    let _ = env_logger::builder()
        .is_test(true)
        .filter_level(log::LevelFilter::Debug)
        .try_init();
}

/// Test configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestConfig {
    pub timeout_seconds: u64,
    pub enable_snapshots: bool,
    pub enable_benchmarks: bool,
    pub parallel_execution: bool,
    pub output_directory: PathBuf,
}

impl Default for TestConfig {
    fn default() -> Self {
        Self {
            timeout_seconds: 30,
            enable_snapshots: true,
            enable_benchmarks: false,
            parallel_execution: true,
            output_directory: PathBuf::from("target/test-output"),
        }
    }
}

impl TestConfig {
    pub fn load_or_default() -> Self {
        if let Ok(config_path) = std::env::var("KRYON_TEST_CONFIG") {
            if let Ok(content) = fs::read_to_string(&config_path) {
                if let Ok(config) = serde_json::from_str(&content) {
                    return config;
                }
            }
        }
        Self::default()
    }
}