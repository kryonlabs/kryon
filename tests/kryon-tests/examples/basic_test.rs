//! Example demonstrating the Kryon testing framework
//! 
//! This example shows how to use the comprehensive testing infrastructure
//! to test Kryon UI applications.

use kryon_tests::prelude::*;

#[tokio::main]
async fn main() -> Result<()> {
    // Initialize the test environment
    setup_test_environment();
    
    println!("🧪 Kryon Testing Framework Example");
    println!("==================================");
    
    // Example 1: Basic unit test using the TestCase builder
    println!("\n1. Basic Unit Test");
    let basic_test = TestCase::new("example_basic_app")
        .with_source(r#"
            App {
                window_width: 800
                window_height: 600
                Text { text: "Hello World" }
            }
        "#)
        .expect_compilation_success()
        .expect_elements(2)
        .expect_text_content("Hello World");
    
    match basic_test.run().await {
        Ok(result) => {
            if result.success {
                println!("✅ Basic test passed");
            } else {
                println!("❌ Basic test failed: {:?}", result.errors);
            }
        }
        Err(e) => println!("❌ Basic test error: {}", e),
    }
    
    // Example 2: Using fixtures for multiple tests
    println!("\n2. Fixture-based Testing");
    let fixture_manager = FixtureManager::new("fixtures");
    println!("Available fixtures:");
    for fixture in fixture_manager.all_fixtures() {
        println!("  - {} ({})", fixture.name, fixture.description);
    }
    
    // Example 3: Batch test runner
    println!("\n3. Batch Test Runner");
    let config = TestConfig::default();
    let mut batch_runner = BatchTestRunner::new(
        config.clone(),
        std::sync::Arc::new(fixture_manager)
    );
    
    // Run all fixtures
    match batch_runner.run_all_fixtures().await {
        Ok(_) => {
            let summary = batch_runner.summary();
            println!("Batch tests completed:");
            println!("  Total: {}", summary.total);
            println!("  Passed: {}", summary.passed);
            println!("  Failed: {}", summary.failed);
            println!("  Success rate: {:.1}%", summary.success_rate());
        }
        Err(e) => println!("❌ Batch tests failed: {}", e),
    }
    
    // Example 4: Configuration system
    println!("\n4. Configuration System");
    let test_suite_config = TestSuiteConfig::default();
    println!("Default test configuration:");
    println!("  Name: {}", test_suite_config.general.name);
    println!("  Default renderer: {}", test_suite_config.general.default_renderer);
    println!("  Enabled renderers: {:?}", test_suite_config.enabled_renderers());
    
    // Validate configuration
    match test_suite_config.validate() {
        Ok(_) => println!("✅ Configuration is valid"),
        Err(e) => println!("❌ Configuration validation failed: {}", e),
    }
    
    // Example 5: Test context and environment
    println!("\n5. Test Context and Environment");
    let mut context = TestContext::new(test_suite_config)?;
    context.with_suite("basic_tests".to_string())
           .with_renderer("ratatui".to_string())
           .with_environment("testing".to_string());
    
    println!("Test context created:");
    println!("  Suite: {:?}", context.current_suite);
    println!("  Renderer: {:?}", context.current_renderer);
    println!("  Environment: {:?}", context.current_environment);
    
    // Apply environment configuration
    if let Err(e) = context.apply_environment() {
        println!("❌ Failed to apply environment: {}", e);
    } else {
        println!("✅ Environment applied successfully");
    }
    
    println!("\n🎉 Testing framework example completed!");
    println!("\nTo run the full test suite, use:");
    println!("  cargo run --bin test_runner");
    println!("  cargo run --bin test_runner --type unit --verbose");
    println!("  cargo run --bin test_runner --config test-config.toml");
    
    Ok(())
}

// Example of how to write custom test functions
#[cfg(test)]
mod tests {
    use super::*;
    
    #[tokio::test]
    async fn test_basic_compilation() {
        let test_case = TestCase::new("basic_test")
            .with_source(r#"
                App {
                    Text { text: "Test" }
                }
            "#)
            .expect_compilation_success()
            .expect_elements(2);
        
        let result = test_case.run().await.unwrap();
        assert!(result.success);
    }
    
    #[tokio::test]
    async fn test_error_handling() {
        let test_case = TestCase::new("error_test")
            .with_source(r#"
                App {
                    Text { text: "Missing brace"
                // Missing closing brace
            "#)
            .expect_compilation_failure();
        
        let result = test_case.run().await.unwrap();
        assert!(result.success); // Success means it correctly failed as expected
    }
    
    #[tokio::test]
    async fn test_fixture_manager() {
        let fixture_manager = FixtureManager::new("fixtures");
        
        // Test that we have some default fixtures
        let fixtures: Vec<_> = fixture_manager.all_fixtures().collect();
        assert!(!fixtures.is_empty());
        
        // Test that we can get specific fixtures
        let basic_fixture = fixture_manager.get_fixture("basic_app");
        assert!(basic_fixture.is_some());
        
        if let Some(fixture) = basic_fixture {
            assert_eq!(fixture.name, "basic_app");
            assert!(!fixture.source.is_empty());
        }
    }
}