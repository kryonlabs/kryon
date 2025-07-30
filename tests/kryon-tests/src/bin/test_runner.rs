//! Centralized test runner for Kryon project

use kryon_tests::prelude::*;
use kryon_tests::config::{TestSuiteConfig, TestContext};
use std::env;
use std::process;

#[derive(Debug, Clone)]
struct TestRunnerConfig {
    test_type: TestType,
    fixture_filter: Option<String>,
    parallel: bool,
    verbose: bool,
    timeout: Duration,
    output_format: OutputFormat,
    config_file: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum TestType {
    All,
    Unit,
    Integration,
    Snapshot,
    Visual,
    Property,
    Performance,
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum OutputFormat {
    Human,
    Json,
    Junit,
}

#[tokio::main]
async fn main() -> Result<()> {
    setup_test_environment();
    
    let config = parse_args()?;
    
    // Load test suite configuration
    let suite_config = if let Some(config_file) = &config.config_file {
        TestSuiteConfig::load_from_file(config_file)
            .unwrap_or_else(|_| {
                eprintln!("Warning: Failed to load config file '{}', using defaults", config_file);
                TestSuiteConfig::default()
            })
    } else {
        TestSuiteConfig::default()
    };
    
    // Validate configuration
    if let Err(e) = suite_config.validate() {
        eprintln!("Configuration validation failed: {}", e);
        process::exit(1);
    }
    
    println!("🚀 Kryon Test Runner");
    println!("Test type: {:?}", config.test_type);
    println!("Parallel execution: {}", config.parallel);
    println!("Timeout: {:?}", config.timeout);
    
    if let Some(filter) = &config.fixture_filter {
        println!("Filter: {}", filter);
    }
    
    println!();
    
    let start_time = std::time::Instant::now();
    let results = run_tests(&config, &suite_config).await?;
    let total_time = start_time.elapsed();
    
    print_final_summary(&results, total_time, &config);
    
    // Exit with non-zero code if any tests failed
    let exit_code = if results.all_passed() { 0 } else { 1 };
    process::exit(exit_code);
}

fn parse_args() -> Result<TestRunnerConfig> {
    let args: Vec<String> = env::args().collect();
    let mut config = TestRunnerConfig {
        test_type: TestType::All,
        fixture_filter: None,
        parallel: true,
        verbose: false,
        timeout: Duration::from_secs(300), // 5 minutes default
        output_format: OutputFormat::Human,
        config_file: None,
    };
    
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--type" | "-t" => {
                i += 1;
                if i < args.len() {
                    config.test_type = match args[i].as_str() {
                        "all" => TestType::All,
                        "unit" => TestType::Unit,
                        "integration" => TestType::Integration,
                        "snapshot" => TestType::Snapshot,
                        "visual" => TestType::Visual,
                        "property" => TestType::Property,
                        "performance" => TestType::Performance,
                        _ => bail!("Invalid test type: {}", args[i]),
                    };
                }
            },
            "--filter" | "-f" => {
                i += 1;
                if i < args.len() {
                    config.fixture_filter = Some(args[i].clone());
                }
            },
            "--sequential" | "-s" => {
                config.parallel = false;
            },
            "--verbose" | "-v" => {
                config.verbose = true;
            },
            "--timeout" => {
                i += 1;
                if i < args.len() {
                    let seconds: u64 = args[i].parse()
                        .context("Invalid timeout value")?;
                    config.timeout = Duration::from_secs(seconds);
                }
            },
            "--output" | "-o" => {
                i += 1;
                if i < args.len() {
                    config.output_format = match args[i].as_str() {
                        "human" => OutputFormat::Human,
                        "json" => OutputFormat::Json,
                        "junit" => OutputFormat::Junit,
                        _ => bail!("Invalid output format: {}", args[i]),
                    };
                }
            },
            "--config" | "-c" => {
                i += 1;
                if i < args.len() {
                    config.config_file = Some(args[i].clone());
                }
            },
            "--help" | "-h" => {
                print_help();
                process::exit(0);
            },
            _ => {
                if args[i].starts_with('-') {
                    bail!("Unknown option: {}", args[i]);
                }
            }
        }
        i += 1;
    }
    
    Ok(config)
}

fn print_help() {
    println!("Kryon Test Runner");
    println!();
    println!("USAGE:");
    println!("    test-runner [OPTIONS]");
    println!();
    println!("OPTIONS:");
    println!("    -t, --type <TYPE>        Test type to run [default: all]");
    println!("                             [possible values: all, unit, integration, snapshot, visual, property, performance]");
    println!("    -f, --filter <FILTER>    Filter tests by name pattern");
    println!("    -s, --sequential         Run tests sequentially instead of in parallel");
    println!("    -v, --verbose           Enable verbose output");
    println!("    --timeout <SECONDS>     Test timeout in seconds [default: 300]");
    println!("    -o, --output <FORMAT>   Output format [default: human]");
    println!("                             [possible values: human, json, junit]");
    println!("    -c, --config <FILE>     Configuration file path");
    println!("    -h, --help              Print help information");
    println!();
    println!("EXAMPLES:");
    println!("    test-runner                           # Run all tests");
    println!("    test-runner -t integration            # Run only integration tests");
    println!("    test-runner -f "layout"               # Run tests matching 'layout'");
    println!("    test-runner -s -v                     # Run sequentially with verbose output");
    println!("    test-runner -o json > results.json    # Output results as JSON");
}

async fn run_tests(config: &TestRunnerConfig, suite_config: &TestSuiteConfig) -> Result<TestResults> {
    let test_config = TestConfig {
        timeout_seconds: config.timeout.as_secs(),
        enable_snapshots: matches!(config.test_type, TestType::All | TestType::Snapshot),
        enable_benchmarks: matches!(config.test_type, TestType::All | TestType::Performance),
        parallel_execution: config.parallel,
        output_directory: PathBuf::from("target/test-output"),
    };
    
    let mut results = TestResults::new();
    
    match config.test_type {
        TestType::All => {
            results.merge(run_unit_tests(&test_config, config).await?);
            results.merge(run_integration_tests(&test_config, config).await?);
            results.merge(run_snapshot_tests(&test_config, config).await?);
            results.merge(run_visual_tests(&test_config, config).await?);
            results.merge(run_property_tests(&test_config, config).await?);
            if test_config.enable_benchmarks {
                results.merge(run_performance_tests(&test_config, config).await?);
            }
        },
        TestType::Unit => {
            results.merge(run_unit_tests(&test_config, config).await?);
        },
        TestType::Integration => {
            results.merge(run_integration_tests(&test_config, config).await?);
        },
        TestType::Snapshot => {
            results.merge(run_snapshot_tests(&test_config, config).await?);
        },
        TestType::Visual => {
            results.merge(run_visual_tests(&test_config, config).await?);
        },
        TestType::Property => {
            results.merge(run_property_tests(&test_config, config).await?);
        },
        TestType::Performance => {
            results.merge(run_performance_tests(&test_config, config).await?);
        },
    }
    
    Ok(results)
}

async fn run_unit_tests(test_config: &TestConfig, config: &TestRunnerConfig) -> Result<TestResults> {
    println!("=== Running Unit Tests ===");
    
    let fixture_manager = std::sync::Arc::new(FixtureManager::new("fixtures"));
    let mut batch_runner = BatchTestRunner::new(test_config.clone(), fixture_manager);
    
    // Filter fixtures if requested
    let test_cases: Vec<TestCase> = if let Some(filter) = &config.fixture_filter {
        batch_runner.fixtures.all_fixtures()
            .filter(|f| f.name.contains(filter))
            .map(|f| test_case_from_fixture(f))
            .collect()
    } else {
        batch_runner.fixtures.all_fixtures()
            .map(|f| test_case_from_fixture(f))
            .collect()
    };
    
    batch_runner.run_test_cases(test_cases).await?;
    
    if config.verbose {
        batch_runner.print_results();
    }
    
    let summary = batch_runner.summary();
    println!("Unit Tests: {} passed, {} failed", summary.passed, summary.failed);
    
    Ok(TestResults {
        unit_tests: summary,
        ..Default::default()
    })
}

async fn run_integration_tests(test_config: &TestConfig, config: &TestRunnerConfig) -> Result<TestResults> {
    println!("\n=== Running Integration Tests ===");
    
    // Stub implementation - create basic integration tests
    let mut results = vec![];
    let start_time = std::time::Instant::now();
    
    // TODO: Implement actual integration test suite
    println!("Integration tests not yet implemented - placeholder");
    
    let total = 0;
    let passed = 0;
    let execution_time = start_time.elapsed();
    
    println!("Integration Tests: {} passed, {} failed", passed, total - passed);
    
    Ok(TestResults {
        integration_tests: TestSummary {
            total,
            passed,
            failed: total - passed,
            total_time: execution_time,
            average_time: Duration::ZERO,
            slowest_test: None,
        },
        ..Default::default()
    })
}

async fn run_snapshot_tests(test_config: &TestConfig, config: &TestRunnerConfig) -> Result<TestResults> {
    println!("\n=== Running Snapshot Tests ===");
    
    // Stub implementation - create basic snapshot tests
    let start_time = std::time::Instant::now();
    
    // TODO: Implement actual snapshot test suite
    println!("Snapshot tests not yet implemented - placeholder");
    
    let total = 0;
    let passed = 0;
    let execution_time = start_time.elapsed();
    
    println!("Snapshot Tests: {} passed, {} failed", passed, total - passed);
    
    Ok(TestResults {
        snapshot_tests: TestSummary {
            total,
            passed,
            failed: total - passed,
            total_time: execution_time,
            average_time: Duration::ZERO,
            slowest_test: None,
        },
        ..Default::default()
    })
}

async fn run_visual_tests(test_config: &TestConfig, config: &TestRunnerConfig) -> Result<TestResults> {
    println!("\n=== Running Visual Tests ===");
    
    // Stub implementation - create basic visual tests  
    let start_time = std::time::Instant::now();
    
    // TODO: Implement actual visual test suite
    println!("Visual tests not yet implemented - placeholder");
    
    let total = 0;
    let passed = 0;
    let execution_time = start_time.elapsed();
    
    println!("Visual Tests: {} passed, {} failed", passed, total - passed);
    
    Ok(TestResults {
        visual_tests: TestSummary {
            total,
            passed,
            failed: total - passed,
            total_time: execution_time,
            average_time: Duration::ZERO,
            slowest_test: None,
        },
        ..Default::default()
    })
}

async fn run_property_tests(test_config: &TestConfig, config: &TestRunnerConfig) -> Result<TestResults> {
    println!("\n=== Running Property Tests ===");
    
    // Stub implementation - create basic property tests
    let start_time = std::time::Instant::now();
    
    // TODO: Implement actual property test suite  
    println!("Property tests not yet implemented - placeholder");
    
    let total = 0;
    let passed = 0;
    let execution_time = start_time.elapsed();
    
    println!("Property Tests: {} passed, {} failed", passed, total - passed);
    
    Ok(TestResults {
        property_tests: TestSummary {
            total,
            passed,
            failed: total - passed,
            total_time: execution_time,
            average_time: Duration::ZERO,
            slowest_test: None,
        },
        ..Default::default()
    })
}

async fn run_performance_tests(test_config: &TestConfig, config: &TestRunnerConfig) -> Result<TestResults> {
    println!("\n=== Running Performance Tests ===");
    
    // Stub implementation - create basic performance tests
    let start_time = std::time::Instant::now();
    
    // TODO: Implement actual performance test suite
    println!("Performance tests not yet implemented - placeholder");
    
    let total = 0;
    let passed = 0;
    let execution_time = start_time.elapsed();
    
    println!("Performance Tests: {} benchmarks completed", total);
    
    Ok(TestResults {
        performance_tests: TestSummary {
            total,
            passed,
            failed: 0,
            total_time: execution_time,
            average_time: Duration::ZERO,
            slowest_test: None,
        },
        ..Default::default()
    })
}

#[derive(Debug, Default)]
struct TestResults {
    unit_tests: TestSummary,
    integration_tests: TestSummary,
    snapshot_tests: TestSummary,
    visual_tests: TestSummary,
    property_tests: TestSummary,
    performance_tests: TestSummary,
}

impl TestResults {
    fn new() -> Self {
        Default::default()
    }
    
    fn merge(&mut self, other: TestResults) {
        // Simple merge - in practice you'd want to properly combine summaries
        if other.unit_tests.total > 0 {
            self.unit_tests = other.unit_tests;
        }
        if other.integration_tests.total > 0 {
            self.integration_tests = other.integration_tests;
        }
        if other.snapshot_tests.total > 0 {
            self.snapshot_tests = other.snapshot_tests;
        }
        if other.visual_tests.total > 0 {
            self.visual_tests = other.visual_tests;
        }
        if other.property_tests.total > 0 {
            self.property_tests = other.property_tests;
        }
        if other.performance_tests.total > 0 {
            self.performance_tests = other.performance_tests;
        }
    }
    
    fn total_tests(&self) -> usize {
        self.unit_tests.total +
        self.integration_tests.total +
        self.snapshot_tests.total +
        self.visual_tests.total +
        self.property_tests.total +
        self.performance_tests.total
    }
    
    fn total_passed(&self) -> usize {
        self.unit_tests.passed +
        self.integration_tests.passed +
        self.snapshot_tests.passed +
        self.visual_tests.passed +
        self.property_tests.passed +
        self.performance_tests.passed
    }
    
    fn total_failed(&self) -> usize {
        self.unit_tests.failed +
        self.integration_tests.failed +
        self.snapshot_tests.failed +
        self.visual_tests.failed +
        self.property_tests.failed +
        self.performance_tests.failed
    }
    
    fn all_passed(&self) -> bool {
        self.total_failed() == 0
    }
}

fn print_final_summary(results: &TestResults, total_time: Duration, config: &TestRunnerConfig) {
    match config.output_format {
        OutputFormat::Human => print_human_summary(results, total_time),
        OutputFormat::Json => print_json_summary(results, total_time),
        OutputFormat::Junit => print_junit_summary(results, total_time),
    }
}

fn print_human_summary(results: &TestResults, total_time: Duration) {
    println!();
    println!("=== 🎯 Final Test Summary ===");
    println!("Total tests: {}", results.total_tests());
    println!("Passed: {}", results.total_passed());
    println!("Failed: {}", results.total_failed());
    println!("Total time: {:.2}s", total_time.as_secs_f64());
    
    if results.all_passed() {
        println!();
        println!("✅ All tests passed! ✅");
    } else {
        println!();
        println!("❌ {} tests failed ❌", results.total_failed());
        
        // Break down failures by type
        if results.unit_tests.failed > 0 {
            println!("  Unit test failures: {}", results.unit_tests.failed);
        }
        if results.integration_tests.failed > 0 {
            println!("  Integration test failures: {}", results.integration_tests.failed);
        }
        if results.snapshot_tests.failed > 0 {
            println!("  Snapshot test failures: {}", results.snapshot_tests.failed);
        }
        if results.visual_tests.failed > 0 {
            println!("  Visual test failures: {}", results.visual_tests.failed);
        }
        if results.property_tests.failed > 0 {
            println!("  Property test failures: {}", results.property_tests.failed);
        }
    }
}

fn print_json_summary(results: &TestResults, total_time: Duration) {
    let json_result = serde_json::json!({
        "total_tests": results.total_tests(),
        "passed": results.total_passed(),
        "failed": results.total_failed(),
        "total_time_seconds": total_time.as_secs_f64(),
        "success": results.all_passed(),
        "breakdown": {
            "unit_tests": {
                "total": results.unit_tests.total,
                "passed": results.unit_tests.passed,
                "failed": results.unit_tests.failed,
            },
            "integration_tests": {
                "total": results.integration_tests.total,
                "passed": results.integration_tests.passed,
                "failed": results.integration_tests.failed,
            },
            "snapshot_tests": {
                "total": results.snapshot_tests.total,
                "passed": results.snapshot_tests.passed,
                "failed": results.snapshot_tests.failed,
            },
            "visual_tests": {
                "total": results.visual_tests.total,
                "passed": results.visual_tests.passed,
                "failed": results.visual_tests.failed,
            },
            "property_tests": {
                "total": results.property_tests.total,
                "passed": results.property_tests.passed,
                "failed": results.property_tests.failed,
            },
            "performance_tests": {
                "total": results.performance_tests.total,
                "passed": results.performance_tests.passed,
                "failed": results.performance_tests.failed,
            },
        }
    });
    
    println!("{}", serde_json::to_string_pretty(&json_result).unwrap());
}

fn print_junit_summary(results: &TestResults, total_time: Duration) {
    // Basic JUnit XML format
    println!("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    println!(
        "<testsuite name=\"Kryon Tests\" tests=\"{}\" failures=\"{}\" time=\"{:.3}\">",
        results.total_tests(),
        results.total_failed(),
        total_time.as_secs_f64()
    );
    
    // Add individual test results (simplified)
    println!("  <testcase classname=\"KryonTests\" name=\"unit_tests\" time=\"{:.3}\"/>", results.unit_tests.total_time.as_secs_f64());
    println!("  <testcase classname=\"KryonTests\" name=\"integration_tests\" time=\"{:.3}\"/>", results.integration_tests.total_time.as_secs_f64());
    println!("  <testcase classname=\"KryonTests\" name=\"snapshot_tests\" time=\"{:.3}\"/>", results.snapshot_tests.total_time.as_secs_f64());
    println!("  <testcase classname=\"KryonTests\" name=\"visual_tests\" time=\"{:.3}\"/>", results.visual_tests.total_time.as_secs_f64());
    println!("  <testcase classname=\"KryonTests\" name=\"property_tests\" time=\"{:.3}\"/>", results.property_tests.total_time.as_secs_f64());
    println!("  <testcase classname=\"KryonTests\" name=\"performance_tests\" time=\"{:.3}\"/>", results.performance_tests.total_time.as_secs_f64());
    
    println!("</testsuite>");
}
