//! Test result aggregation and metrics collection system
//! 
//! This module provides comprehensive metrics collection, aggregation, and reporting
//! for all test types in the Kryon testing framework.

use crate::prelude::*;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Comprehensive test metrics aggregator
#[derive(Debug, Clone)]
pub struct TestMetricsCollector {
    pub session_start: std::time::Instant,
    pub metrics: HashMap<String, TestCategoryMetrics>,
    pub system_metrics: SystemMetrics,
    pub config: MetricsConfig,
}

/// Configuration for metrics collection
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MetricsConfig {
    pub collect_system_metrics: bool,
    pub collect_memory_metrics: bool,
    pub collect_timing_metrics: bool,
    pub collect_quality_metrics: bool,
    pub export_format: MetricsExportFormat,
    pub retention_days: u32,
}

/// Supported export formats for metrics
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum MetricsExportFormat {
    Json,
    Csv,
    Prometheus,
    InfluxDB,
}

/// Metrics for a specific test category
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestCategoryMetrics {
    pub category: String,
    pub total_tests: usize,
    pub passed_tests: usize,
    pub failed_tests: usize,
    pub skipped_tests: usize,
    pub execution_time: Duration,
    pub average_test_time: Duration,
    pub slowest_test: Option<TestMetric>,
    pub fastest_test: Option<TestMetric>,
    pub memory_usage: Option<MemoryMetrics>,
    pub quality_metrics: QualityMetrics,
    pub trend_data: Vec<TrendDataPoint>,
}

/// Individual test metric
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestMetric {
    pub name: String,
    pub duration: Duration,
    pub memory_kb: Option<usize>,
    pub success: bool,
    pub error_message: Option<String>,
    pub assertions_count: usize,
    pub timestamp: std::time::SystemTime,
}

/// Memory usage metrics
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MemoryMetrics {
    pub peak_memory_kb: usize,
    pub average_memory_kb: usize,
    pub memory_growth_kb: i64,
    pub gc_collections: usize,
    pub heap_size_kb: usize,
}

/// Quality metrics for code and tests
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct QualityMetrics {
    pub code_coverage_percentage: f64,
    pub assertion_coverage: f64,
    pub cyclomatic_complexity: f64,
    pub test_reliability_score: f64,
    pub maintainability_index: f64,
    pub technical_debt_minutes: f64,
}

/// Trend data point for historical analysis
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TrendDataPoint {
    pub timestamp: std::time::SystemTime,
    pub execution_time_ms: f64,
    pub success_rate: f64,
    pub memory_usage_kb: Option<usize>,
    pub commit_hash: Option<String>,
}

/// System-level metrics
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SystemMetrics {
    pub cpu_usage_percentage: f64,
    pub memory_usage_percentage: f64,
    pub disk_io_mb: f64,
    pub network_io_mb: f64,
    pub load_average: f64,
    pub concurrent_tests: usize,
}

/// Comprehensive test session report
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestSessionReport {
    pub session_id: String,
    pub start_time: std::time::SystemTime,
    pub end_time: std::time::SystemTime,
    pub total_duration: Duration,
    pub environment: TestEnvironmentInfo,
    pub categories: HashMap<String, TestCategoryMetrics>,
    pub system_metrics: SystemMetrics,
    pub summary: TestSessionSummary,
    pub recommendations: Vec<TestRecommendation>,
}

/// Test environment information
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestEnvironmentInfo {
    pub os: String,
    pub rust_version: String,
    pub cpu_cores: usize,
    pub total_memory_mb: usize,
    pub cargo_version: String,
    pub git_commit: Option<String>,
    pub git_branch: Option<String>,
    pub ci_environment: Option<String>,
}

/// Session summary statistics
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestSessionSummary {
    pub total_tests: usize,
    pub total_passed: usize,
    pub total_failed: usize,
    pub total_skipped: usize,
    pub overall_success_rate: f64,
    pub total_execution_time: Duration,
    pub average_test_time: Duration,
    pub fastest_category: String,
    pub slowest_category: String,
    pub memory_efficiency_score: f64,
    pub performance_regression_detected: bool,
}

/// Test recommendations based on metrics analysis
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestRecommendation {
    pub category: RecommendationCategory,
    pub priority: RecommendationPriority,
    pub title: String,
    pub description: String,
    pub suggested_action: String,
    pub impact_estimate: String,
}

/// Recommendation categories
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum RecommendationCategory {
    Performance,
    Memory,
    Quality,
    Reliability,
    Maintainability,
}

/// Recommendation priorities
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum RecommendationPriority {
    Critical,
    High,
    Medium,
    Low,
    Info,
}

impl Default for MetricsConfig {
    fn default() -> Self {
        Self {
            collect_system_metrics: true,
            collect_memory_metrics: true,
            collect_timing_metrics: true,
            collect_quality_metrics: true,
            export_format: MetricsExportFormat::Json,
            retention_days: 30,
        }
    }
}

impl TestMetricsCollector {
    pub fn new(config: MetricsConfig) -> Self {
        Self {
            session_start: std::time::Instant::now(),
            metrics: HashMap::new(),
            system_metrics: SystemMetrics::collect(),
            config,
        }
    }

    /// Record a test result
    pub fn record_test_result(&mut self, category: &str, test_result: TestMetric) {
        let category_metrics = self.metrics.entry(category.to_string())
            .or_insert_with(|| TestCategoryMetrics::new(category));

        category_metrics.add_test_result(test_result);
    }

    /// Record multiple test results for a category
    pub fn record_category_results(&mut self, category: &str, results: Vec<TestMetric>) {
        for result in results {
            self.record_test_result(category, result);
        }
    }

    /// Generate comprehensive session report
    pub fn generate_session_report(&self) -> TestSessionReport {
        let session_id = uuid::Uuid::new_v4().to_string();
        let end_time = std::time::SystemTime::now();
        let start_time = end_time - self.session_start.elapsed();
        
        let summary = self.calculate_session_summary();
        let recommendations = self.generate_recommendations(&summary);

        TestSessionReport {
            session_id,
            start_time,
            end_time,
            total_duration: self.session_start.elapsed(),
            environment: TestEnvironmentInfo::collect(),
            categories: self.metrics.clone(),
            system_metrics: self.system_metrics.clone(),
            summary,
            recommendations,
        }
    }

    /// Calculate session summary statistics
    fn calculate_session_summary(&self) -> TestSessionSummary {
        let total_tests: usize = self.metrics.values().map(|m| m.total_tests).sum();
        let total_passed: usize = self.metrics.values().map(|m| m.passed_tests).sum();
        let total_failed: usize = self.metrics.values().map(|m| m.failed_tests).sum();
        let total_skipped: usize = self.metrics.values().map(|m| m.skipped_tests).sum();
        
        let overall_success_rate = if total_tests > 0 {
            (total_passed as f64 / total_tests as f64) * 100.0
        } else {
            0.0
        };

        let total_execution_time: Duration = self.metrics.values()
            .map(|m| m.execution_time)
            .sum();

        let average_test_time = if total_tests > 0 {
            total_execution_time / total_tests as u32
        } else {
            Duration::ZERO
        };

        // Find fastest and slowest categories
        let (fastest_category, slowest_category) = self.find_fastest_slowest_categories();

        // Calculate memory efficiency score
        let memory_efficiency_score = self.calculate_memory_efficiency_score();

        // Detect performance regressions (simplified)
        let performance_regression_detected = self.detect_performance_regression();

        TestSessionSummary {
            total_tests,
            total_passed,
            total_failed,
            total_skipped,
            overall_success_rate,
            total_execution_time,
            average_test_time,
            fastest_category,
            slowest_category,
            memory_efficiency_score,
            performance_regression_detected,
        }
    }

    /// Generate recommendations based on metrics analysis
    fn generate_recommendations(&self, summary: &TestSessionSummary) -> Vec<TestRecommendation> {
        let mut recommendations = Vec::new();

        // Performance recommendations
        if summary.average_test_time > Duration::from_millis(500) {
            recommendations.push(TestRecommendation {
                category: RecommendationCategory::Performance,
                priority: RecommendationPriority::Medium,
                title: "Test execution time is high".to_string(),
                description: "Average test execution time exceeds recommended threshold".to_string(),
                suggested_action: "Consider optimizing slow tests or running them in parallel".to_string(),
                impact_estimate: "Could reduce CI time by 20-30%".to_string(),
            });
        }

        // Quality recommendations
        if summary.overall_success_rate < 95.0 {
            let priority = if summary.overall_success_rate < 80.0 {
                RecommendationPriority::Critical
            } else {
                RecommendationPriority::High
            };

            recommendations.push(TestRecommendation {
                category: RecommendationCategory::Quality,
                priority,
                title: "Test success rate is below target".to_string(),
                description: format!("Success rate is {:.1}%, target is 95%+", summary.overall_success_rate),
                suggested_action: "Investigate and fix failing tests to improve reliability".to_string(),
                impact_estimate: "Critical for code quality and deployment confidence".to_string(),
            });
        }

        // Memory recommendations
        if summary.memory_efficiency_score < 70.0 {
            recommendations.push(TestRecommendation {
                category: RecommendationCategory::Memory,
                priority: RecommendationPriority::Medium,
                title: "Memory usage efficiency is low".to_string(),
                description: "Tests are using more memory than expected".to_string(),
                suggested_action: "Profile memory usage and optimize resource-intensive tests".to_string(),
                impact_estimate: "Could reduce peak memory usage by 15-25%".to_string(),
            });
        }

        // Reliability recommendations
        let flaky_tests = self.detect_flaky_tests();
        if !flaky_tests.is_empty() {
            recommendations.push(TestRecommendation {
                category: RecommendationCategory::Reliability,
                priority: RecommendationPriority::High,
                title: format!("Detected {} potentially flaky tests", flaky_tests.len()),
                description: "Some tests show inconsistent results across runs".to_string(),
                suggested_action: "Review and stabilize flaky tests to improve reliability".to_string(),
                impact_estimate: "Improves CI stability and developer confidence".to_string(),
            });
        }

        recommendations
    }

    /// Find fastest and slowest test categories
    fn find_fastest_slowest_categories(&self) -> (String, String) {
        let mut fastest = ("unknown".to_string(), Duration::MAX);
        let mut slowest = ("unknown".to_string(), Duration::ZERO);

        for (name, metrics) in &self.metrics {
            if metrics.average_test_time < fastest.1 {
                fastest = (name.clone(), metrics.average_test_time);
            }
            if metrics.average_test_time > slowest.1 {
                slowest = (name.clone(), metrics.average_test_time);
            }
        }

        (fastest.0, slowest.0)
    }

    /// Calculate memory efficiency score (0-100)
    fn calculate_memory_efficiency_score(&self) -> f64 {
        // Simplified calculation - would need actual memory profiling
        let base_score = 80.0;
        
        // Penalize if any category uses excessive memory
        let memory_penalty = self.metrics.values()
            .filter_map(|m| m.memory_usage.as_ref())
            .map(|mem| {
                if mem.peak_memory_kb > 100_000 { // > 100MB
                    10.0
                } else if mem.peak_memory_kb > 50_000 { // > 50MB
                    5.0
                } else {
                    0.0
                }
            })
            .sum::<f64>();

        (base_score - memory_penalty).max(0.0).min(100.0)
    }

    /// Detect performance regressions (simplified)
    fn detect_performance_regression(&self) -> bool {
        // In a real implementation, this would compare against historical data
        // For now, just check if any category has unusually slow tests
        self.metrics.values().any(|metrics| {
            metrics.average_test_time > Duration::from_secs(1)
        })
    }

    /// Detect potentially flaky tests
    fn detect_flaky_tests(&self) -> Vec<String> {
        // In a real implementation, this would analyze historical test results
        // For now, return empty vector
        Vec::new()
    }

    /// Export metrics in specified format
    pub fn export_metrics(&self, path: &Path) -> Result<()> {
        let report = self.generate_session_report();

        match self.config.export_format {
            MetricsExportFormat::Json => {
                let json = serde_json::to_string_pretty(&report)?;
                fs::write(path, json)?;
            }
            MetricsExportFormat::Csv => {
                self.export_csv(&report, path)?;
            }
            MetricsExportFormat::Prometheus => {
                self.export_prometheus(&report, path)?;
            }
            MetricsExportFormat::InfluxDB => {
                self.export_influxdb(&report, path)?;
            }
        }

        println!("Metrics exported to: {}", path.display());
        Ok(())
    }

    /// Export metrics as CSV
    fn export_csv(&self, report: &TestSessionReport, path: &Path) -> Result<()> {
        let mut csv_content = String::new();
        csv_content.push_str("category,total_tests,passed_tests,failed_tests,success_rate,avg_time_ms\n");

        for (category, metrics) in &report.categories {
            let success_rate = if metrics.total_tests > 0 {
                (metrics.passed_tests as f64 / metrics.total_tests as f64) * 100.0
            } else {
                0.0
            };

            csv_content.push_str(&format!(
                "{},{},{},{},{:.2},{:.2}\n",
                category,
                metrics.total_tests,
                metrics.passed_tests,
                metrics.failed_tests,
                success_rate,
                metrics.average_test_time.as_secs_f64() * 1000.0
            ));
        }

        fs::write(path, csv_content)?;
        Ok(())
    }

    /// Export metrics in Prometheus format
    fn export_prometheus(&self, report: &TestSessionReport, path: &Path) -> Result<()> {
        let mut prometheus_content = String::new();
        
        prometheus_content.push_str("# HELP kryon_tests_total Total number of tests\n");
        prometheus_content.push_str("# TYPE kryon_tests_total counter\n");

        for (category, metrics) in &report.categories {
            prometheus_content.push_str(&format!(
                "kryon_tests_total{{category=\"{}\"}} {}\n",
                category, metrics.total_tests
            ));
            
            prometheus_content.push_str(&format!(
                "kryon_tests_passed{{category=\"{}\"}} {}\n",
                category, metrics.passed_tests
            ));
            
            prometheus_content.push_str(&format!(
                "kryon_tests_failed{{category=\"{}\"}} {}\n",
                category, metrics.failed_tests
            ));
            
            prometheus_content.push_str(&format!(
                "kryon_test_duration_seconds{{category=\"{}\"}} {:.6}\n",
                category, metrics.execution_time.as_secs_f64()
            ));
        }

        fs::write(path, prometheus_content)?;
        Ok(())
    }

    /// Export metrics in InfluxDB line protocol format
    fn export_influxdb(&self, report: &TestSessionReport, path: &Path) -> Result<()> {
        let mut influx_content = String::new();
        let timestamp = report.start_time.duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_nanos();

        for (category, metrics) in &report.categories {
            influx_content.push_str(&format!(
                "kryon_tests,category={} total={}i,passed={}i,failed={}i,duration_ms={:.2} {}\n",
                category,
                metrics.total_tests,
                metrics.passed_tests,
                metrics.failed_tests,
                metrics.execution_time.as_secs_f64() * 1000.0,
                timestamp
            ));
        }

        fs::write(path, influx_content)?;
        Ok(())
    }

    /// Print comprehensive metrics summary to console
    pub fn print_metrics_summary(&self) {
        let report = self.generate_session_report();
        
        println!("\n╔═══════════════════════════════════════════════════════════════╗");
        println!("║                    🎯 TEST METRICS SUMMARY                    ║");
        println!("╚═══════════════════════════════════════════════════════════════╝");
        
        println!("\n📊 Session Overview:");
        println!("  Session ID: {}", report.session_id);
        println!("  Duration: {:.2}s", report.total_duration.as_secs_f64());
        println!("  Environment: {} (Rust {})", report.environment.os, report.environment.rust_version);
        
        println!("\n📈 Test Results:");
        println!("  Total Tests: {}", report.summary.total_tests);
        println!("  Passed: {} ({:.1}%)", 
            report.summary.total_passed,
            report.summary.overall_success_rate);
        println!("  Failed: {}", report.summary.total_failed);
        println!("  Skipped: {}", report.summary.total_skipped);
        
        println!("\n⏱️  Performance:");
        println!("  Average Test Time: {:.2}ms", 
            report.summary.average_test_time.as_secs_f64() * 1000.0);
        println!("  Fastest Category: {}", report.summary.fastest_category);
        println!("  Slowest Category: {}", report.summary.slowest_category);
        
        println!("\n💾 Memory:");
        println!("  Memory Efficiency Score: {:.1}/100", report.summary.memory_efficiency_score);
        
        if !report.recommendations.is_empty() {
            println!("\n💡 Recommendations:");
            for rec in report.recommendations.iter().take(3) {
                let priority_icon = match rec.priority {
                    RecommendationPriority::Critical => "🔴",
                    RecommendationPriority::High => "🟠",
                    RecommendationPriority::Medium => "🟡",
                    RecommendationPriority::Low => "🟢",
                    RecommendationPriority::Info => "ℹ️",
                };
                println!("  {} {}", priority_icon, rec.title);
                println!("     {}", rec.suggested_action);
            }
            
            if report.recommendations.len() > 3 {
                println!("     ... and {} more recommendations", report.recommendations.len() - 3);
            }
        }
        
        println!("\n═══════════════════════════════════════════════════════════════");
    }
}

impl TestCategoryMetrics {
    pub fn new(category: &str) -> Self {
        Self {
            category: category.to_string(),
            total_tests: 0,
            passed_tests: 0,
            failed_tests: 0,
            skipped_tests: 0,
            execution_time: Duration::ZERO,
            average_test_time: Duration::ZERO,
            slowest_test: None,
            fastest_test: None,
            memory_usage: None,
            quality_metrics: QualityMetrics::default(),
            trend_data: Vec::new(),
        }
    }

    pub fn add_test_result(&mut self, test_result: TestMetric) {
        self.total_tests += 1;
        
        if test_result.success {
            self.passed_tests += 1;
        } else {
            self.failed_tests += 1;
        }
        
        self.execution_time += test_result.duration;
        self.average_test_time = self.execution_time / self.total_tests as u32;
        
        // Update fastest/slowest tests
        if self.fastest_test.is_none() || test_result.duration < self.fastest_test.as_ref().unwrap().duration {
            self.fastest_test = Some(test_result.clone());
        }
        
        if self.slowest_test.is_none() || test_result.duration > self.slowest_test.as_ref().unwrap().duration {
            self.slowest_test = Some(test_result.clone());
        }
    }
}

impl SystemMetrics {
    pub fn collect() -> Self {
        // In a real implementation, this would collect actual system metrics
        Self {
            cpu_usage_percentage: 0.0,
            memory_usage_percentage: 0.0,
            disk_io_mb: 0.0,
            network_io_mb: 0.0,
            load_average: 0.0,
            concurrent_tests: 1,
        }
    }
}

impl TestEnvironmentInfo {
    pub fn collect() -> Self {
        Self {
            os: std::env::consts::OS.to_string(),
            rust_version: "1.70.0".to_string(), // Would get from rustc --version
            cpu_cores: num_cpus::get(),
            total_memory_mb: 8192, // Would get from system info
            cargo_version: "1.70.0".to_string(), // Would get from cargo --version
            git_commit: std::env::var("GIT_COMMIT").ok(),
            git_branch: std::env::var("GIT_BRANCH").ok(),
            ci_environment: std::env::var("CI").ok(),
        }
    }
}

impl Default for QualityMetrics {
    fn default() -> Self {
        Self {
            code_coverage_percentage: 0.0,
            assertion_coverage: 0.0,
            cyclomatic_complexity: 0.0,
            test_reliability_score: 100.0,
            maintainability_index: 100.0,
            technical_debt_minutes: 0.0,
        }
    }
}

/// Utility function to create test metric from test result
pub fn create_test_metric(name: &str, result: &TestResult, duration: Duration) -> TestMetric {
    TestMetric {
        name: name.to_string(),
        duration,
        memory_kb: None, // Would be populated if memory profiling is enabled
        success: result.success,
        error_message: if result.success { None } else { Some("Test failed".to_string()) },
        assertions_count: 1, // Would count actual assertions
        timestamp: std::time::SystemTime::now(),
    }
}