//! Coverage Integration Demo
//! 
//! This example demonstrates how to integrate the coverage analysis system
//! with the Kryon testing framework for comprehensive code coverage reporting.

use kryon_tests::prelude::*;
use std::path::Path;

#[tokio::main]
async fn main() -> Result<()> {
    setup_test_environment();
    
    println!("🔍 Kryon Coverage Integration Demo");
    println!("==================================");
    
    // Create output directory for coverage reports
    let coverage_output = Path::new("target/coverage-demo");
    std::fs::create_dir_all(coverage_output)?;
    
    // Demo 1: Basic Coverage Collection
    println!("\n1. 📊 Basic Coverage Collection");
    demo_basic_coverage().await?;
    
    // Demo 2: Coverage with Test Orchestrator
    println!("\n2. 🎛️  Coverage with Test Orchestrator");
    demo_coverage_orchestrator(coverage_output).await?;
    
    // Demo 3: Coverage Analysis and Reporting
    println!("\n3. 📈 Coverage Analysis and Reporting");
    demo_coverage_analysis(coverage_output).await?;
    
    // Demo 4: Coverage Dashboard Generation
    println!("\n4. 📊 Coverage Dashboard Generation");
    demo_coverage_dashboard(coverage_output).await?;
    
    // Demo 5: Integration with Test Metrics
    println!("\n5. 🔗 Integration with Test Metrics");
    demo_coverage_metrics_integration(coverage_output).await?;
    
    println!("\n🎉 Coverage Integration Demo Complete!");
    println!("\nGenerated Coverage Reports:");
    println!("  📁 Output Directory: {}", coverage_output.display());
    println!("  📊 HTML Report: {}", coverage_output.join("coverage.html").display());
    println!("  📈 Dashboard: {}", coverage_output.join("coverage_dashboard.html").display());
    println!("  📋 LCOV Report: {}", coverage_output.join("coverage.info").display());
    println!("  📄 Text Summary: {}", coverage_output.join("coverage_summary.txt").display());
    
    Ok(())
}

async fn demo_basic_coverage() -> Result<()> {
    println!("  🔬 Creating coverage analyzer...");
    
    // Create coverage configuration
    let coverage_config = CoverageConfig {
        enable_line_coverage: true,
        enable_branch_coverage: true,
        enable_function_coverage: true,
        enable_condition_coverage: false,
        minimum_coverage_threshold: 75.0,
        exclude_patterns: vec!["*/tests/*".to_string()],
        include_patterns: vec!["src/**/*.rs".to_string()],
        output_formats: vec![
            CoverageOutputFormat::Html,
            CoverageOutputFormat::Lcov,
            CoverageOutputFormat::Json,
        ],
    };
    
    let mut analyzer = CoverageAnalyzer::new(coverage_config);
    
    // Start coverage collection for a test
    analyzer.start_test_coverage("demo_test", "unit");
    
    // Simulate recording coverage data
    analyzer.record_line_execution("src/lib.rs", 42, "demo_test");
    analyzer.record_line_execution("src/lib.rs", 43, "demo_test");
    analyzer.record_branch_execution("src/lib.rs", "branch_main", true, "demo_test");
    analyzer.record_branch_execution("src/lib.rs", "branch_main", false, "demo_test");
    
    println!("     ✅ Coverage data recorded for demo_test");
    
    // Generate coverage report
    let report = analyzer.generate_coverage_report()?;
    println!("     📊 Coverage report generated:");
    println!("        Line Coverage: {:.1}%", report.summary.line_coverage_percentage);
    println!("        Branch Coverage: {:.1}%", report.summary.branch_coverage_percentage);
    println!("        Function Coverage: {:.1}%", report.summary.function_coverage_percentage);
    
    Ok(())
}

async fn demo_coverage_orchestrator(output_dir: &Path) -> Result<()> {
    println!("  🎛️  Setting up coverage orchestrator...");
    
    // Create coverage configuration for orchestrator
    let coverage_config = CoverageConfig {
        enable_line_coverage: true,
        enable_branch_coverage: true,
        enable_function_coverage: true,
        enable_condition_coverage: false,
        minimum_coverage_threshold: 80.0,
        exclude_patterns: vec!["*/tests/*".to_string(), "*/examples/*".to_string()],
        include_patterns: vec!["src/**/*.rs".to_string()],
        output_formats: vec![
            CoverageOutputFormat::Html,
            CoverageOutputFormat::Lcov,
            CoverageOutputFormat::Json,
            CoverageOutputFormat::Text,
        ],
    };
    
    let mut orchestrator = CoverageTestOrchestrator::new(coverage_config, output_dir);
    
    // Create a test case
    let test_case = TestCase::new("orchestrator_demo_test")
        .with_source(r#"
            App {
                window_width: 800
                window_height: 600
                Text { 
                    text: "Coverage Demo"
                    color: "#00ff00"
                }
            }
        "#)
        .expect_compilation_success()
        .expect_elements(2);
    
    // Run test with coverage collection
    let coverage_report = orchestrator
        .run_test_with_coverage(test_case, "orchestrator_demo_test")
        .await?;
    
    println!("     ✅ Test executed with coverage collection");
    println!("     📊 Coverage Summary:");
    println!("        Total Lines: {}", coverage_report.summary.total_lines);
    println!("        Covered Lines: {}", coverage_report.summary.covered_lines);
    println!("        Line Coverage: {:.1}%", coverage_report.summary.line_coverage_percentage);
    
    Ok(())
}

async fn demo_coverage_analysis(output_dir: &Path) -> Result<()> {
    println!("  🔍 Performing detailed coverage analysis...");
    
    // Create a more comprehensive coverage scenario
    let coverage_config = CoverageConfig::default();
    let mut analyzer = CoverageAnalyzer::new(coverage_config);
    
    // Simulate multiple test cases with varying coverage
    let test_scenarios = vec![
        ("basic_ui_test", vec![(10, 15), (20, 25), (30, 35)]),
        ("layout_test", vec![(40, 50), (60, 70)]),
        ("error_handling_test", vec![(100, 110), (120, 125)]),
    ];
    
    for (test_name, line_ranges) in test_scenarios {
        analyzer.start_test_coverage(test_name, "integration");
        
        for (start, end) in line_ranges {
            for line in start..=end {
                analyzer.record_line_execution("src/renderer.rs", line, test_name);
            }
        }
        
        // Add some branch coverage
        analyzer.record_branch_execution("src/renderer.rs", &format!("branch_{}", test_name), true, test_name);
        analyzer.record_branch_execution("src/renderer.rs", &format!("branch_{}", test_name), false, test_name);
        
        println!("     📈 Recorded coverage for {}", test_name);
    }
    
    // Generate comprehensive report
    let report = analyzer.generate_coverage_report()?;
    
    // Export reports
    analyzer.export_coverage_report(&report, output_dir)?;
    
    // Print detailed analysis
    analyzer.print_coverage_summary(&report);
    
    // Show uncovered regions
    if !report.uncovered_regions.is_empty() {
        println!("\n     🎯 Priority Uncovered Regions:");
        for (i, region) in report.uncovered_regions.iter().take(3).enumerate() {
            let priority_icon = match region.priority {
                CoveragePriority::Critical => "🔴",
                CoveragePriority::High => "🟠",
                CoveragePriority::Medium => "🟡",
                CoveragePriority::Low => "🟢",
            };
            println!("        {} {}. {}:{} - {:?} ({:?})", 
                priority_icon, i + 1, 
                region.file_path.split('/').last().unwrap_or(&region.file_path),
                region.start_line, region.region_type, region.priority);
        }
    }
    
    // Show recommendations
    if !report.recommendations.is_empty() {
        println!("\n     💡 Coverage Recommendations:");
        for (i, rec) in report.recommendations.iter().take(2).enumerate() {
            println!("        {}. {}", i + 1, rec.title);
            println!("           Approach: {}", rec.suggested_approach);
            println!("           Impact: {}", rec.impact_assessment);
        }
    }
    
    Ok(())
}

async fn demo_coverage_dashboard(output_dir: &Path) -> Result<()> {
    println!("  📊 Generating interactive coverage dashboard...");
    
    // Create orchestrator with dashboard capabilities
    let coverage_config = CoverageConfig {
        enable_line_coverage: true,
        enable_branch_coverage: true,
        enable_function_coverage: true,
        minimum_coverage_threshold: 85.0,
        output_formats: vec![
            CoverageOutputFormat::Html,
            CoverageOutputFormat::Json,
        ],
        ..Default::default()
    };
    
    let orchestrator = CoverageTestOrchestrator::new(coverage_config, output_dir);
    
    // Create a sample coverage report
    let demo_report = coverage_utils::generate_demo_coverage_report();
    
    // Generate dashboard
    orchestrator.generate_coverage_dashboard(&demo_report)?;
    
    // Print status
    orchestrator.print_coverage_status(&demo_report);
    
    println!("     ✅ Interactive dashboard generated");
    println!("     🌐 Open {} in your browser to view the dashboard", 
        output_dir.join("coverage_dashboard.html").display());
    
    Ok(())
}

async fn demo_coverage_metrics_integration(output_dir: &Path) -> Result<()> {
    println!("  📊 Integrating coverage with test metrics...");
    
    // Create metrics collector
    let metrics_config = MetricsConfig {
        collect_system_metrics: true,
        collect_memory_metrics: true,
        collect_timing_metrics: true,
        collect_quality_metrics: true,
        export_format: MetricsExportFormat::Json,
        retention_days: 30,
    };
    
    let mut metrics_collector = TestMetricsCollector::new(metrics_config);
    
    // Create coverage orchestrator
    let coverage_config = CoverageConfig::default();
    let mut coverage_orchestrator = CoverageTestOrchestrator::new(coverage_config, output_dir);
    
    // Simulate running tests with both metrics and coverage
    let test_cases = vec![
        ("coverage_integration_test_1", Duration::from_millis(150)),
        ("coverage_integration_test_2", Duration::from_millis(200)),
        ("coverage_integration_test_3", Duration::from_millis(120)),
    ];
    
    for (test_name, duration) in test_cases {
        // Create test case
        let test_case = TestCase::new(test_name)
            .with_source(&format!(r#"
                App {{
                    Text {{ text: "{}" }}
                }}
            "#, test_name))
            .expect_compilation_success();
        
        // Run with coverage
        let coverage_report = coverage_orchestrator
            .run_test_with_coverage(test_case, test_name)
            .await?;
        
        // Record metrics
        let test_metric = TestMetric {
            name: test_name.to_string(),
            duration,
            memory_kb: Some(1024),
            success: true,
            error_message: None,
            assertions_count: 1,
            timestamp: std::time::SystemTime::now(),
        };
        
        metrics_collector.record_test_result("coverage_integration", test_metric);
        
        println!("     ✅ Completed {} with coverage: {:.1}% line coverage", 
            test_name, coverage_report.summary.line_coverage_percentage);
    }
    
    // Generate comprehensive report combining metrics and coverage
    let session_report = metrics_collector.generate_session_report();
    
    println!("     📈 Combined Metrics & Coverage Report:");
    println!("        Total Tests: {}", session_report.summary.total_tests);
    println!("        Success Rate: {:.1}%", session_report.summary.overall_success_rate);
    println!("        Average Time: {:.2}ms", 
        session_report.summary.average_test_time.as_secs_f64() * 1000.0);
    
    // Export combined report
    let combined_report_path = output_dir.join("combined_metrics_coverage.json");
    metrics_collector.export_metrics(&combined_report_path)?;
    
    println!("     💾 Combined report exported to: {}", combined_report_path.display());
    
    Ok(())
}

#[cfg(test)]
mod coverage_integration_tests {
    use super::*;
    
    #[tokio::test]
    async fn test_coverage_analyzer_creation() {
        let config = CoverageConfig::default();
        let analyzer = CoverageAnalyzer::new(config);
        
        assert!(analyzer.collected_data.is_empty());
        assert!(analyzer.test_mappings.is_empty());
    }
    
    #[tokio::test]
    async fn test_coverage_orchestrator_setup() {
        let temp_dir = tempfile::tempdir().unwrap();
        let config = CoverageConfig::default();
        let orchestrator = CoverageTestOrchestrator::new(config, temp_dir.path());
        
        assert!(orchestrator.enabled);
        assert_eq!(orchestrator.output_directory, temp_dir.path());
    }
    
    #[tokio::test]
    async fn test_coverage_data_recording() {
        let config = CoverageConfig::default();
        let mut analyzer = CoverageAnalyzer::new(config);
        
        analyzer.start_test_coverage("test_recording", "unit");
        analyzer.record_line_execution("test_file.rs", 10, "test_recording");
        analyzer.record_branch_execution("test_file.rs", "branch_1", true, "test_recording");
        
        assert!(!analyzer.collected_data.is_empty());
        assert!(!analyzer.test_mappings.is_empty());
        
        let test_info = analyzer.test_mappings.get("test_recording").unwrap();
        assert_eq!(test_info.test_name, "test_recording");
        assert_eq!(test_info.test_category, "unit");
        assert!(test_info.lines_covered.contains("test_file.rs:10"));
        assert!(test_info.branches_covered.contains("test_file.rs:branch_1"));
    }
    
    #[tokio::test]
    async fn test_coverage_report_generation() {
        let config = CoverageConfig::default();
        let mut analyzer = CoverageAnalyzer::new(config);
        
        // Add some test data
        analyzer.start_test_coverage("report_test", "unit");
        analyzer.record_line_execution("src/lib.rs", 1, "report_test");
        analyzer.record_line_execution("src/lib.rs", 2, "report_test");
        
        let report = analyzer.generate_coverage_report().unwrap();
        
        assert!(report.summary.total_lines > 0);
        assert!(!report.file_coverage.is_empty());
        assert!(!report.test_coverage_mapping.is_empty());
    }
}