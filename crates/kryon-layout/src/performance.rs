//! Performance monitoring and benchmarking for layout systems
//!
//! This module provides tools to measure and optimize layout performance.

use std::time::{Duration, Instant};
use std::collections::HashMap;
use serde::{Serialize, Deserialize};

/// Performance metrics for layout operations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LayoutPerformanceMetrics {
    /// Total time spent in layout computation
    pub total_layout_time: Duration,
    /// Time spent building the layout tree
    pub tree_building_time: Duration,
    /// Time spent in style conversion
    pub style_conversion_time: Duration,
    /// Time spent in Taffy layout computation
    pub taffy_computation_time: Duration,
    /// Time spent in result caching
    pub caching_time: Duration,
    /// Number of elements processed
    pub element_count: usize,
    /// Number of layout invalidations
    pub invalidation_count: usize,
    /// Cache hit rate (0.0 to 1.0)
    pub cache_hit_rate: f64,
    /// Memory usage in bytes
    pub memory_usage: usize,
}

/// Performance profiler for layout operations
pub struct LayoutProfiler {
    /// Whether profiling is enabled
    enabled: bool,
    /// Start time of current operation
    operation_start: Option<Instant>,
    /// Accumulated metrics
    metrics: LayoutPerformanceMetrics,
    /// Individual operation timings
    operation_times: HashMap<String, Vec<Duration>>,
    /// Cache hit/miss counters
    cache_hits: u64,
    cache_misses: u64,
}

impl LayoutProfiler {
    /// Create a new layout profiler
    pub fn new(enabled: bool) -> Self {
        Self {
            enabled,
            operation_start: None,
            metrics: LayoutPerformanceMetrics::default(),
            operation_times: HashMap::new(),
            cache_hits: 0,
            cache_misses: 0,
        }
    }

    /// Start timing an operation
    pub fn start_operation(&mut self, operation: &str) {
        if !self.enabled {
            return;
        }
        
        self.operation_start = Some(Instant::now());
        tracing::debug!("Started profiling operation: {}", operation);
    }

    /// End timing an operation and record the duration
    pub fn end_operation(&mut self, operation: &str) {
        if !self.enabled {
            return;
        }
        
        if let Some(start) = self.operation_start.take() {
            let duration = start.elapsed();
            
            self.operation_times
                .entry(operation.to_string())
                .or_insert_with(Vec::new)
                .push(duration);
            
            // Update specific metrics
            match operation {
                "tree_building" => self.metrics.tree_building_time += duration,
                "style_conversion" => self.metrics.style_conversion_time += duration,
                "taffy_computation" => self.metrics.taffy_computation_time += duration,
                "caching" => self.metrics.caching_time += duration,
                "total_layout" => self.metrics.total_layout_time += duration,
                _ => {}
            }
            
            tracing::debug!("Completed operation {} in {:?}", operation, duration);
        }
    }

    /// Record a cache hit
    pub fn record_cache_hit(&mut self) {
        if self.enabled {
            self.cache_hits += 1;
        }
    }

    /// Record a cache miss
    pub fn record_cache_miss(&mut self) {
        if self.enabled {
            self.cache_misses += 1;
        }
    }

    /// Record element count
    pub fn record_element_count(&mut self, count: usize) {
        if self.enabled {
            self.metrics.element_count = count;
        }
    }

    /// Record invalidation count
    pub fn record_invalidation(&mut self) {
        if self.enabled {
            self.metrics.invalidation_count += 1;
        }
    }

    /// Record memory usage
    pub fn record_memory_usage(&mut self, bytes: usize) {
        if self.enabled {
            self.metrics.memory_usage = bytes;
        }
    }

    /// Get current metrics
    pub fn get_metrics(&mut self) -> LayoutPerformanceMetrics {
        if self.enabled {
            // Calculate cache hit rate
            let total_cache_operations = self.cache_hits + self.cache_misses;
            self.metrics.cache_hit_rate = if total_cache_operations > 0 {
                self.cache_hits as f64 / total_cache_operations as f64
            } else {
                0.0
            };
        }
        
        self.metrics.clone()
    }

    /// Reset all metrics
    pub fn reset(&mut self) {
        self.metrics = LayoutPerformanceMetrics::default();
        self.operation_times.clear();
        self.cache_hits = 0;
        self.cache_misses = 0;
    }

    /// Get detailed operation statistics
    pub fn get_operation_stats(&self) -> HashMap<String, OperationStats> {
        let mut stats = HashMap::new();
        
        for (operation, times) in &self.operation_times {
            if !times.is_empty() {
                let total: Duration = times.iter().sum();
                let avg = total / times.len() as u32;
                let min = *times.iter().min().unwrap();
                let max = *times.iter().max().unwrap();
                
                stats.insert(operation.clone(), OperationStats {
                    count: times.len(),
                    total,
                    average: avg,
                    min,
                    max,
                    percentile_95: Self::calculate_percentile(times, 0.95),
                });
            }
        }
        
        stats
    }

    /// Calculate percentile for a list of durations
    fn calculate_percentile(times: &[Duration], percentile: f64) -> Duration {
        if times.is_empty() {
            return Duration::ZERO;
        }
        
        let mut sorted = times.to_vec();
        sorted.sort();
        
        let index = ((sorted.len() as f64 - 1.0) * percentile) as usize;
        sorted[index]
    }

    /// Generate a performance report
    pub fn generate_report(&mut self) -> PerformanceReport {
        let metrics = self.get_metrics();
        let operation_stats = self.get_operation_stats();
        
        PerformanceReport {
            metrics,
            operation_stats,
            recommendations: self.generate_recommendations(),
        }
    }

    /// Generate performance recommendations
    fn generate_recommendations(&self) -> Vec<String> {
        let mut recommendations = Vec::new();
        
        // Analyze cache hit rate
        let total_cache_operations = self.cache_hits + self.cache_misses;
        if total_cache_operations > 100 {
            let hit_rate = self.cache_hits as f64 / total_cache_operations as f64;
            if hit_rate < 0.8 {
                recommendations.push(format!(
                    "Cache hit rate is {:.1}%. Consider increasing cache size or improving cache invalidation strategy.",
                    hit_rate * 100.0
                ));
            }
        }
        
        // Analyze layout frequency
        if self.metrics.invalidation_count > 100 {
            recommendations.push(
                "High number of layout invalidations detected. Consider batching layout updates.".to_string()
            );
        }
        
        // Analyze memory usage
        if self.metrics.memory_usage > 50 * 1024 * 1024 { // 50MB
            recommendations.push(
                "High memory usage detected. Consider optimizing element storage or implementing memory pooling.".to_string()
            );
        }
        
        // Analyze operation times
        if let Some(layout_times) = self.operation_times.get("total_layout") {
            if !layout_times.is_empty() {
                let avg_time = layout_times.iter().sum::<Duration>() / layout_times.len() as u32;
                if avg_time > Duration::from_millis(16) { // 60fps threshold
                    recommendations.push(format!(
                        "Average layout time is {:?}, which may cause frame drops. Consider optimizing layout algorithms.",
                        avg_time
                    ));
                }
            }
        }
        
        recommendations
    }
}

/// Statistics for a specific operation
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OperationStats {
    pub count: usize,
    pub total: Duration,
    pub average: Duration,
    pub min: Duration,
    pub max: Duration,
    pub percentile_95: Duration,
}

/// Complete performance report
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PerformanceReport {
    pub metrics: LayoutPerformanceMetrics,
    pub operation_stats: HashMap<String, OperationStats>,
    pub recommendations: Vec<String>,
}

impl Default for LayoutPerformanceMetrics {
    fn default() -> Self {
        Self {
            total_layout_time: Duration::ZERO,
            tree_building_time: Duration::ZERO,
            style_conversion_time: Duration::ZERO,
            taffy_computation_time: Duration::ZERO,
            caching_time: Duration::ZERO,
            element_count: 0,
            invalidation_count: 0,
            cache_hit_rate: 0.0,
            memory_usage: 0,
        }
    }
}

/// Benchmark suite for layout operations
pub struct LayoutBenchmark {
    profiler: LayoutProfiler,
    test_cases: Vec<BenchmarkCase>,
}

/// A single benchmark test case
#[derive(Debug, Clone)]
pub struct BenchmarkCase {
    pub name: String,
    pub element_count: usize,
    pub complexity: BenchmarkComplexity,
}

/// Complexity levels for benchmark cases
#[derive(Debug, Clone)]
pub enum BenchmarkComplexity {
    Simple,
    Medium,
    Complex,
    Extreme,
}

impl LayoutBenchmark {
    /// Create a new benchmark suite
    pub fn new() -> Self {
        Self {
            profiler: LayoutProfiler::new(true),
            test_cases: Vec::new(),
        }
    }

    /// Add a benchmark case
    pub fn add_case(&mut self, name: String, element_count: usize, complexity: BenchmarkComplexity) {
        self.test_cases.push(BenchmarkCase {
            name,
            element_count,
            complexity,
        });
    }

    /// Run all benchmark cases
    pub fn run_all(&mut self) -> Vec<BenchmarkResult> {
        let mut results = Vec::new();
        
        // Clone test cases to avoid borrowing issues
        let cases = self.test_cases.clone();
        for case in &cases {
            let result = self.run_case(case);
            results.push(result);
        }
        
        results
    }

    /// Run a single benchmark case
    fn run_case(&mut self, case: &BenchmarkCase) -> BenchmarkResult {
        self.profiler.reset();
        
        // Simulate layout operations
        let start = Instant::now();
        
        // Simulate different complexity levels
        let iterations = match case.complexity {
            BenchmarkComplexity::Simple => 10,
            BenchmarkComplexity::Medium => 100,
            BenchmarkComplexity::Complex => 1000,
            BenchmarkComplexity::Extreme => 10000,
        };
        
        for _ in 0..iterations {
            self.profiler.start_operation("total_layout");
            
            // Simulate work
            std::thread::sleep(Duration::from_micros(case.element_count as u64));
            
            self.profiler.end_operation("total_layout");
        }
        
        let total_time = start.elapsed();
        let metrics = self.profiler.get_metrics();
        
        BenchmarkResult {
            case_name: case.name.clone(),
            total_time,
            metrics,
            operations_per_second: iterations as f64 / total_time.as_secs_f64(),
        }
    }
}

/// Result of a benchmark run
#[derive(Debug, Clone)]
pub struct BenchmarkResult {
    pub case_name: String,
    pub total_time: Duration,
    pub metrics: LayoutPerformanceMetrics,
    pub operations_per_second: f64,
}

impl Default for LayoutBenchmark {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_profiler_basic() {
        let mut profiler = LayoutProfiler::new(true);
        
        profiler.start_operation("test");
        std::thread::sleep(Duration::from_millis(1));
        profiler.end_operation("test");
        
        let stats = profiler.get_operation_stats();
        assert_eq!(stats.len(), 1);
        assert!(stats.contains_key("test"));
    }

    #[test]
    fn test_cache_metrics() {
        let mut profiler = LayoutProfiler::new(true);
        
        profiler.record_cache_hit();
        profiler.record_cache_hit();
        profiler.record_cache_miss();
        
        let metrics = profiler.get_metrics();
        assert_eq!(metrics.cache_hit_rate, 2.0 / 3.0);
    }

    #[test]
    fn test_benchmark_case() {
        let mut benchmark = LayoutBenchmark::new();
        benchmark.add_case("test".to_string(), 10, BenchmarkComplexity::Simple);
        
        let results = benchmark.run_all();
        assert_eq!(results.len(), 1);
        assert_eq!(results[0].case_name, "test");
    }

    #[test]
    fn test_percentile_calculation() {
        let times = vec![
            Duration::from_millis(1),
            Duration::from_millis(2),
            Duration::from_millis(3),
            Duration::from_millis(4),
            Duration::from_millis(5),
        ];
        
        let p95 = LayoutProfiler::calculate_percentile(&times, 0.95);
        assert_eq!(p95, Duration::from_millis(5));
    }
}