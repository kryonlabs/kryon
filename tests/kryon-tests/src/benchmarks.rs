//! Performance benchmarking utilities

use crate::prelude::*;
use std::collections::BTreeMap;

/// Benchmark configuration
#[derive(Debug, Clone)]
pub struct BenchmarkConfig {
    pub name: String,
    pub warmup_iterations: usize,
    pub measurement_iterations: usize,
    pub timeout: Duration,
    pub memory_profiling: bool,
}

/// Benchmark result data
#[derive(Debug, Clone)]
pub struct BenchmarkResult {
    pub config: BenchmarkConfig,
    pub compilation_times: Vec<Duration>,
    pub parse_times: Vec<Duration>,
    pub total_times: Vec<Duration>,
    pub memory_usage: Option<MemoryUsage>,
    pub statistics: BenchmarkStatistics,
}

/// Memory usage statistics
#[derive(Debug, Clone)]
pub struct MemoryUsage {
    pub peak_memory_kb: usize,
    pub average_memory_kb: usize,
    pub allocations: usize,
    pub deallocations: usize,
}

/// Statistical analysis of benchmark results
#[derive(Debug, Clone)]
pub struct BenchmarkStatistics {
    pub compilation_stats: TimeStatistics,
    pub parse_stats: TimeStatistics,
    pub total_stats: TimeStatistics,
}

/// Time-based statistics
#[derive(Debug, Clone)]
pub struct TimeStatistics {
    pub mean: Duration,
    pub median: Duration,
    pub min: Duration,
    pub max: Duration,
    pub std_dev: Duration,
    pub percentile_95: Duration,
    pub percentile_99: Duration,
}

/// Benchmark suite for running multiple benchmarks
#[derive(Debug)]
pub struct BenchmarkSuite {
    pub benchmarks: Vec<BenchmarkCase>,
    pub results: BTreeMap<String, BenchmarkResult>,
    pub config: TestConfig,
}

/// Individual benchmark case
#[derive(Debug, Clone)]
pub struct BenchmarkCase {
    pub config: BenchmarkConfig,
    pub source: String,
    pub category: BenchmarkCategory,
}

/// Benchmark categories
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum BenchmarkCategory {
    Compilation,
    Parsing,
    Layout,
    Rendering,
    Memory,
    EndToEnd,
}

impl Default for BenchmarkConfig {
    fn default() -> Self {
        Self {
            name: "benchmark".to_string(),
            warmup_iterations: 10,
            measurement_iterations: 100,
            timeout: Duration::from_secs(60),
            memory_profiling: false,
        }
    }
}

impl BenchmarkConfig {
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            ..Default::default()
        }
    }

    pub fn with_iterations(mut self, warmup: usize, measurement: usize) -> Self {
        self.warmup_iterations = warmup;
        self.measurement_iterations = measurement;
        self
    }

    pub fn with_memory_profiling(mut self, enabled: bool) -> Self {
        self.memory_profiling = enabled;
        self
    }

    pub fn with_timeout(mut self, timeout: Duration) -> Self {
        self.timeout = timeout;
        self
    }
}

impl BenchmarkCase {
    pub fn new(
        config: BenchmarkConfig,
        source: impl Into<String>,
        category: BenchmarkCategory,
    ) -> Self {
        Self {
            config,
            source: source.into(),
            category,
        }
    }

    /// Run this benchmark case
    pub async fn run(&self) -> Result<BenchmarkResult> {
        println!("Running benchmark: {}", self.config.name);
        
        let mut compilation_times = Vec::new();
        let mut parse_times = Vec::new();
        let mut total_times = Vec::new();
        
        let options = CompilerOptions::default();
        
        // Warmup iterations
        println!("  Warmup ({} iterations)...", self.config.warmup_iterations);
        for _ in 0..self.config.warmup_iterations {
            let _ = self.run_single_iteration(&options)?;
        }
        
        // Measurement iterations
        println!("  Measuring ({} iterations)...", self.config.measurement_iterations);
        for i in 0..self.config.measurement_iterations {
            if i % 20 == 0 {
                print!(".");
            }
            
            let (comp_time, parse_time, total_time) = self.run_single_iteration(&options)?;
            compilation_times.push(comp_time);
            parse_times.push(parse_time);
            total_times.push(total_time);
        }
        println!();
        
        // Calculate statistics
        let statistics = BenchmarkStatistics {
            compilation_stats: Self::calculate_time_statistics(&compilation_times),
            parse_stats: Self::calculate_time_statistics(&parse_times),
            total_stats: Self::calculate_time_statistics(&total_times),
        };
        
        Ok(BenchmarkResult {
            config: self.config.clone(),
            compilation_times,
            parse_times,
            total_times,
            memory_usage: None, // TODO: Implement memory profiling
            statistics,
        })
    }

    fn run_single_iteration(&self, options: &CompilerOptions) -> Result<(Duration, Duration, Duration)> {
        let total_start = std::time::Instant::now();
        
        // Compilation timing
        let comp_start = std::time::Instant::now();
        let krb_data = compile_string(&self.source, options.clone())
            .map_err(|e| anyhow::anyhow!("Compilation failed: {}", e))?;
        let comp_time = comp_start.elapsed();
        
        // Parsing timing
        let parse_start = std::time::Instant::now();
        let _krb_file = KrbFile::parse(&krb_data)
            .map_err(|e| anyhow::anyhow!("Parsing failed: {}", e))?;
        let parse_time = parse_start.elapsed();
        
        let total_time = total_start.elapsed();
        
        Ok((comp_time, parse_time, total_time))
    }

    fn calculate_time_statistics(times: &[Duration]) -> TimeStatistics {
        if times.is_empty() {
            return TimeStatistics {
                mean: Duration::ZERO,
                median: Duration::ZERO,
                min: Duration::ZERO,
                max: Duration::ZERO,
                std_dev: Duration::ZERO,
                percentile_95: Duration::ZERO,
                percentile_99: Duration::ZERO,
            };
        }
        
        let mut sorted_times = times.to_vec();
        sorted_times.sort();
        
        let min = sorted_times[0];
        let max = sorted_times[sorted_times.len() - 1];
        
        // Calculate mean
        let total_nanos: u128 = times.iter().map(|d| d.as_nanos()).sum();
        let mean_nanos = total_nanos / times.len() as u128;
        let mean = Duration::from_nanos(mean_nanos as u64);
        
        // Calculate median
        let median = if sorted_times.len() % 2 == 0 {
            let mid1 = sorted_times[sorted_times.len() / 2 - 1].as_nanos();
            let mid2 = sorted_times[sorted_times.len() / 2].as_nanos();
            Duration::from_nanos(((mid1 + mid2) / 2) as u64)
        } else {
            sorted_times[sorted_times.len() / 2]
        };
        
        // Calculate standard deviation
        let variance: f64 = times.iter()
            .map(|d| {
                let diff = d.as_nanos() as f64 - mean_nanos as f64;
                diff * diff
            })
            .sum::<f64>() / times.len() as f64;
        let std_dev = Duration::from_nanos(variance.sqrt() as u64);
        
        // Calculate percentiles
        let p95_idx = (sorted_times.len() as f64 * 0.95) as usize;
        let p99_idx = (sorted_times.len() as f64 * 0.99) as usize;
        let percentile_95 = sorted_times[p95_idx.min(sorted_times.len() - 1)];
        let percentile_99 = sorted_times[p99_idx.min(sorted_times.len() - 1)];
        
        TimeStatistics {
            mean,
            median,
            min,
            max,
            std_dev,
            percentile_95,
            percentile_99,
        }
    }
}

impl BenchmarkSuite {
    pub fn new(config: TestConfig) -> Self {
        Self {
            benchmarks: Vec::new(),
            results: BTreeMap::new(),
            config,
        }
    }

    pub fn add_benchmark(&mut self, benchmark: BenchmarkCase) {
        self.benchmarks.push(benchmark);
    }

    pub fn add_compilation_benchmark(&mut self, name: &str, source: &str) {
        let config = BenchmarkConfig::new(format!("{}_compilation", name))
            .with_iterations(5, 50);
        
        let benchmark = BenchmarkCase::new(
            config,
            source,
            BenchmarkCategory::Compilation,
        );
        
        self.add_benchmark(benchmark);
    }

    pub fn add_parsing_benchmark(&mut self, name: &str, source: &str) {
        let config = BenchmarkConfig::new(format!("{}_parsing", name))
            .with_iterations(10, 100);
        
        let benchmark = BenchmarkCase::new(
            config,
            source,
            BenchmarkCategory::Parsing,
        );
        
        self.add_benchmark(benchmark);
    }

    /// Run all benchmarks in the suite
    pub async fn run_all(&mut self) -> Result<()> {
        println!("\n=== Running Benchmark Suite ===");
        println!("Total benchmarks: {}", self.benchmarks.len());
        
        for benchmark in &self.benchmarks {
            match benchmark.run().await {
                Ok(result) => {
                    self.results.insert(result.config.name.clone(), result);
                }
                Err(e) => {
                    eprintln!("Benchmark '{}' failed: {}", benchmark.config.name, e);
                }
            }
        }
        
        self.print_summary();
        Ok(())
    }

    /// Print benchmark results summary
    pub fn print_summary(&self) {
        println!("\n=== Benchmark Results ===");
        
        for (name, result) in &self.results {
            println!("\n{}", name.bold());
            
            // Compilation stats
            let comp_stats = &result.statistics.compilation_stats;
            println!("  Compilation:");
            println!("    Mean: {:.2}ms", comp_stats.mean.as_secs_f64() * 1000.0);
            println!("    Median: {:.2}ms", comp_stats.median.as_secs_f64() * 1000.0);
            println!("    Min: {:.2}ms, Max: {:.2}ms", 
                comp_stats.min.as_secs_f64() * 1000.0,
                comp_stats.max.as_secs_f64() * 1000.0);
            println!("    95th percentile: {:.2}ms", comp_stats.percentile_95.as_secs_f64() * 1000.0);
            
            // Parsing stats
            let parse_stats = &result.statistics.parse_stats;
            println!("  Parsing:");
            println!("    Mean: {:.2}ms", parse_stats.mean.as_secs_f64() * 1000.0);
            println!("    Median: {:.2}ms", parse_stats.median.as_secs_f64() * 1000.0);
            println!("    95th percentile: {:.2}ms", parse_stats.percentile_95.as_secs_f64() * 1000.0);
            
            // Total stats
            let total_stats = &result.statistics.total_stats;
            println!("  Total:");
            println!("    Mean: {:.2}ms", total_stats.mean.as_secs_f64() * 1000.0);
            println!("    95th percentile: {:.2}ms", total_stats.percentile_95.as_secs_f64() * 1000.0);
        }
        
        // Performance comparison
        if self.results.len() > 1 {
            println!("\n=== Performance Comparison ===");
            let mut benchmarks: Vec<_> = self.results.iter().collect();
            benchmarks.sort_by_key(|(_, result)| result.statistics.total_stats.mean);
            
            let fastest = benchmarks[0].1.statistics.total_stats.mean;
            
            for (name, result) in benchmarks {
                let ratio = result.statistics.total_stats.mean.as_secs_f64() / fastest.as_secs_f64();
                if ratio > 1.0 {
                    println!("  {} is {:.2}x slower than fastest", name, ratio);
                } else {
                    println!("  {} (fastest)", name);
                }
            }
        }
    }

    /// Export results to JSON for analysis
    pub fn export_json(&self, path: &Path) -> Result<()> {
        let json = serde_json::to_string_pretty(&self.results)?;
        fs::write(path, json)?;
        println!("Benchmark results exported to: {}", path.display());
        Ok(())
    }
}

/// Create default benchmark suite with common test cases
pub fn create_default_benchmark_suite() -> BenchmarkSuite {
    let mut suite = BenchmarkSuite::new(TestConfig::default());
    
    // Basic app benchmark
    suite.add_compilation_benchmark(
        "basic_app",
        r#"
App {
    window_width: 800
    window_height: 600
    Text { text: "Hello World" }
}
"#,
    );
    
    // Complex layout benchmark
    suite.add_compilation_benchmark(
        "complex_layout",
        r#"
App {
    Container {
        layout: "column"
        padding: 20
        gap: 10
        
        Container {
            layout: "row"
            justify_content: "space-between"
            align_items: "center"
            
            Button { text: "Start" }
            Button { text: "Middle" }
            Button { text: "End" }
        }
        
        Container {
            layout: "column"
            align_items: "center"
            
            Text { text: "Title", font_size: 24, font_weight: "bold" }
            Text { text: "Subtitle", text_color: "#666666" }
        }
    }
}
"#,
    );
    
    // Themed app benchmark
    suite.add_compilation_benchmark(
        "themed_app",
        r#"
Theme default {
    primary_color: "#3b82f6"
    background_color: "#ffffff"
    text_color: "#1e293b"
    spacing: 16
}

App {
    background_color: ${theme.background_color}
    padding: ${theme.spacing}
    
    Container {
        layout: "column"
        gap: ${theme.spacing}
        
        Text {
            text: "Themed Application"
            text_color: ${theme.primary_color}
            font_size: 20
        }
        
        Button {
            text: "Primary Button"
            background_color: ${theme.primary_color}
            text_color: ${theme.background_color}
        }
    }
}
"#,
    );
    
    suite
}

/// Utility macro for creating benchmarks
#[macro_export]
macro_rules! kryon_benchmark {
    ($name:expr, $source:expr) => {
        #[tokio::test]
        async fn benchmark_test() -> anyhow::Result<()> {
            let config = crate::benchmarks::BenchmarkConfig::new($name)
                .with_iterations(5, 25); // Reduced for tests
            
            let benchmark = crate::benchmarks::BenchmarkCase::new(
                config,
                $source,
                crate::benchmarks::BenchmarkCategory::Compilation,
            );
            
            let result = benchmark.run().await?;
            
            // Assert reasonable performance (adjust thresholds as needed)
            if result.statistics.compilation_stats.mean > std::time::Duration::from_millis(100) {
                anyhow::bail!("Compilation too slow: {:.2}ms", 
                    result.statistics.compilation_stats.mean.as_secs_f64() * 1000.0);
            }
            
            Ok(())
        }
    };
}

pub use kryon_benchmark;
