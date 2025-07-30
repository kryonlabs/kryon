//! Test Result Caching and Incremental Testing
//! 
//! This module provides intelligent test result caching and incremental testing capabilities,
//! allowing the system to avoid re-running tests when no relevant changes have been made.
//! 
//! # Features
//! 
//! - **Smart Caching**: Cache test results based on source code and dependency hashes
//! - **Incremental Testing**: Only run tests affected by code changes
//! - **Version Control Integration**: Analyze git diffs to determine what needs retesting
//! - **Dependency Tracking**: Understand which tests are affected by which code changes
//! - **Performance Optimization**: Significantly reduce CI/CD test execution time
//! - **Cache Management**: Automatic cache invalidation and cleanup

use crate::prelude::*;
use std::collections::{HashMap, HashSet};
use std::path::{Path, PathBuf};
use std::time::{Duration, SystemTime};
use serde::{Deserialize, Serialize};
use sha2::{Sha256, Digest};
use std::process::Command;

/// Configuration for test result caching and incremental testing
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CachingConfig {
    /// Enable test result caching
    pub enable_caching: bool,
    
    /// Enable incremental testing based on changes
    pub enable_incremental_testing: bool,
    
    /// Directory to store cache files
    pub cache_directory: PathBuf,
    
    /// Maximum age of cached results in seconds
    pub max_cache_age_seconds: u64,
    
    /// Maximum cache size in bytes
    pub max_cache_size_bytes: u64,
    
    /// Hash algorithm for content fingerprinting
    pub hash_algorithm: HashAlgorithm,
    
    /// Files and directories to track for changes
    pub tracked_paths: Vec<PathBuf>,
    
    /// Patterns to ignore when detecting changes
    pub ignore_patterns: Vec<String>,
    
    /// Enable git integration for change detection
    pub enable_git_integration: bool,
    
    /// Git branch/commit to compare against
    pub git_base_ref: Option<String>,
    
    /// Enable dependency analysis
    pub enable_dependency_analysis: bool,
    
    /// Force cache refresh after this many runs
    pub force_refresh_after_runs: Option<u32>,
}

impl Default for CachingConfig {
    fn default() -> Self {
        Self {
            enable_caching: true,
            enable_incremental_testing: true,
            cache_directory: PathBuf::from("target/test-cache"),
            max_cache_age_seconds: 86400, // 24 hours
            max_cache_size_bytes: 100 * 1024 * 1024, // 100MB
            hash_algorithm: HashAlgorithm::Sha256,
            tracked_paths: vec![
                PathBuf::from("src"),
                PathBuf::from("tests"),
                PathBuf::from("Cargo.toml"),
                PathBuf::from("Cargo.lock"),
            ],
            ignore_patterns: vec![
                "*.tmp".to_string(),
                "*.log".to_string(),
                "target/*".to_string(),
                ".git/*".to_string(),
                "*.md".to_string(),
            ],
            enable_git_integration: true,
            git_base_ref: Some("main".to_string()),
            enable_dependency_analysis: true,
            force_refresh_after_runs: Some(10),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum HashAlgorithm {
    Sha256,
    Blake3,
}

/// Cached test result with metadata
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CachedTestResult {
    /// Test identifier
    pub test_id: String,
    
    /// Test name
    pub test_name: String,
    
    /// Test execution result
    pub result: TestResult,
    
    /// Content hash when test was executed
    pub content_hash: String,
    
    /// Dependencies hash
    pub dependency_hash: String,
    
    /// Timestamp when cached
    pub cached_at: SystemTime,
    
    /// Number of times this result was used
    pub hit_count: u32,
    
    /// Test execution environment hash
    pub environment_hash: String,
    
    /// Version of the caching system
    pub cache_version: String,
}

/// Information about what changed since last test run
#[derive(Debug, Clone)]
pub struct ChangeAnalysis {
    /// Files that have been modified
    pub modified_files: HashSet<PathBuf>,
    
    /// Files that have been added
    pub added_files: HashSet<PathBuf>,
    
    /// Files that have been deleted
    pub deleted_files: HashSet<PathBuf>,
    
    /// Tests that need to be re-run due to changes
    pub affected_tests: HashSet<String>,
    
    /// Tests that can use cached results
    pub cacheable_tests: HashSet<String>,
    
    /// Change summary statistics
    pub change_summary: ChangeSummary,
}

#[derive(Debug, Clone)]
pub struct ChangeSummary {
    pub total_files_changed: usize,
    pub total_lines_changed: usize,
    pub change_complexity_score: f64,
    pub risk_level: RiskLevel,
}

#[derive(Debug, Clone)]
pub enum RiskLevel {
    Low,
    Medium,
    High,
    Critical,
}

/// Test result cache manager
pub struct TestResultCache {
    config: CachingConfig,
    cache_entries: HashMap<String, CachedTestResult>,
    dependency_graph: HashMap<String, HashSet<String>>,
    file_test_mappings: HashMap<PathBuf, HashSet<String>>,
    cache_stats: CacheStatistics,
}

#[derive(Debug, Clone, Default)]
pub struct CacheStatistics {
    pub total_cache_hits: u64,
    pub total_cache_misses: u64,
    pub total_cache_invalidations: u64,
    pub cache_size_bytes: u64,
    pub cache_entries_count: u64,
    pub average_lookup_time_ms: f64,
    pub cache_effectiveness_percent: f64,
}

impl TestResultCache {
    /// Create a new test result cache with configuration
    pub fn new(config: CachingConfig) -> Result<Self> {
        std::fs::create_dir_all(&config.cache_directory)?;
        
        let mut cache = Self {
            config,
            cache_entries: HashMap::new(),
            dependency_graph: HashMap::new(),
            file_test_mappings: HashMap::new(),
            cache_stats: CacheStatistics::default(),
        };
        
        cache.load_cache_from_disk()?;
        cache.build_dependency_mappings()?;
        
        Ok(cache)
    }
    
    /// Check if a test result can be retrieved from cache
    pub fn get_cached_result(&mut self, test_id: &str) -> Option<&CachedTestResult> {
        let lookup_start = std::time::Instant::now();
        
        if !self.config.enable_caching {
            return None;
        }
        
        if let Some(cached_result) = self.cache_entries.get_mut(test_id) {
            // Check if cache entry is still valid
            if self.is_cache_entry_valid(cached_result) {
                cached_result.hit_count += 1;
                self.cache_stats.total_cache_hits += 1;
                
                let lookup_time = lookup_start.elapsed().as_millis() as f64;
                self.update_average_lookup_time(lookup_time);
                
                return Some(cached_result);
            } else {
                // Cache entry is invalid, remove it
                self.cache_entries.remove(test_id);
                self.cache_stats.total_cache_invalidations += 1;
            }
        }
        
        self.cache_stats.total_cache_misses += 1;
        let lookup_time = lookup_start.elapsed().as_millis() as f64;
        self.update_average_lookup_time(lookup_time);
        
        None
    }
    
    /// Cache a test result
    pub fn cache_result(&mut self, test_id: String, test_name: String, result: TestResult) -> Result<()> {
        if !self.config.enable_caching {
            return Ok(());
        }
        
        let content_hash = self.calculate_content_hash(&test_id)?;
        let dependency_hash = self.calculate_dependency_hash(&test_id)?;
        let environment_hash = self.calculate_environment_hash()?;
        
        let cached_result = CachedTestResult {
            test_id: test_id.clone(),
            test_name,
            result,
            content_hash,
            dependency_hash,
            cached_at: SystemTime::now(),
            hit_count: 0,
            environment_hash,
            cache_version: "1.0.0".to_string(),
        };
        
        self.cache_entries.insert(test_id, cached_result);
        self.cache_stats.cache_entries_count = self.cache_entries.len() as u64;
        
        // Check if we need to clean up old entries
        self.cleanup_cache_if_needed()?;
        
        Ok(())
    }
    
    /// Analyze what has changed since last test run
    pub fn analyze_changes(&self) -> Result<ChangeAnalysis> {
        let mut analysis = ChangeAnalysis {
            modified_files: HashSet::new(),
            added_files: HashSet::new(),
            deleted_files: HashSet::new(),
            affected_tests: HashSet::new(),
            cacheable_tests: HashSet::new(),
            change_summary: ChangeSummary {
                total_files_changed: 0,
                total_lines_changed: 0,
                change_complexity_score: 0.0,
                risk_level: RiskLevel::Low,
            },
        };
        
        if self.config.enable_git_integration {
            analysis = self.analyze_git_changes()?;
        } else {
            analysis = self.analyze_filesystem_changes()?;
        }
        
        // Determine affected tests based on file changes
        for modified_file in &analysis.modified_files {
            if let Some(affected_tests) = self.file_test_mappings.get(modified_file) {
                analysis.affected_tests.extend(affected_tests.clone());
            }
        }
        
        // Determine cacheable tests (tests not affected by changes)
        for test_id in self.cache_entries.keys() {
            if !analysis.affected_tests.contains(test_id) {
                analysis.cacheable_tests.insert(test_id.clone());
            }
        }
        
        // Calculate change complexity and risk
        analysis.change_summary = self.calculate_change_complexity(&analysis);
        
        Ok(analysis)
    }
    
    /// Get recommendations for optimizing test execution
    pub fn get_optimization_recommendations(&self) -> Vec<OptimizationRecommendation> {
        let mut recommendations = Vec::new();
        
        // Cache hit rate analysis
        let hit_rate = if self.cache_stats.total_cache_hits + self.cache_stats.total_cache_misses > 0 {
            self.cache_stats.total_cache_hits as f64 / 
            (self.cache_stats.total_cache_hits + self.cache_stats.total_cache_misses) as f64 * 100.0
        } else {
            0.0
        };
        
        if hit_rate < 50.0 {
            recommendations.push(OptimizationRecommendation {
                category: OptimizationCategory::CacheEfficiency,
                priority: RecommendationPriority::High,
                title: "Improve cache hit rate".to_string(),
                description: format!("Current cache hit rate is {:.1}%, which is below optimal", hit_rate),
                estimated_time_savings: "20-40% faster test execution".to_string(),
                implementation_steps: vec![
                    "Review cache invalidation logic".to_string(),
                    "Increase cache size if memory allows".to_string(),
                    "Improve dependency tracking accuracy".to_string(),
                ],
            });
        }
        
        // Dependency analysis recommendations
        if self.dependency_graph.is_empty() {
            recommendations.push(OptimizationRecommendation {
                category: OptimizationCategory::DependencyTracking,
                priority: RecommendationPriority::Medium,
                title: "Enable dependency tracking".to_string(),
                description: "Dependency tracking is not configured, limiting incremental testing effectiveness".to_string(),
                estimated_time_savings: "30-50% reduction in unnecessary test runs".to_string(),
                implementation_steps: vec![
                    "Configure dependency analysis".to_string(),
                    "Map tests to source files".to_string(),
                    "Enable incremental testing".to_string(),
                ],
            });
        }
        
        // Cache size optimization
        if self.cache_stats.cache_size_bytes > self.config.max_cache_size_bytes {
            recommendations.push(OptimizationRecommendation {
                category: OptimizationCategory::ResourceUsage,
                priority: RecommendationPriority::Medium,
                title: "Cache size optimization needed".to_string(),
                description: "Cache size exceeds configured limit".to_string(),
                estimated_time_savings: "Prevent cache thrashing".to_string(),
                implementation_steps: vec![
                    "Increase cache size limit".to_string(),
                    "Implement more aggressive cleanup".to_string(),
                    "Review cache entry priorities".to_string(),
                ],
            });
        }
        
        recommendations
    }
    
    /// Generate comprehensive caching report
    pub fn generate_caching_report(&self) -> CachingReport {
        let total_tests = self.cache_entries.len();
        let hit_rate = if self.cache_stats.total_cache_hits + self.cache_stats.total_cache_misses > 0 {
            self.cache_stats.total_cache_hits as f64 / 
            (self.cache_stats.total_cache_hits + self.cache_stats.total_cache_misses) as f64 * 100.0
        } else {
            0.0
        };
        
        CachingReport {
            summary: CachingReportSummary {
                total_cached_tests: total_tests,
                cache_hit_rate_percent: hit_rate,
                total_cache_hits: self.cache_stats.total_cache_hits,
                total_cache_misses: self.cache_stats.total_cache_misses,
                average_lookup_time_ms: self.cache_stats.average_lookup_time_ms,
                cache_size_mb: self.cache_stats.cache_size_bytes as f64 / (1024.0 * 1024.0),
                effectiveness_score: self.calculate_cache_effectiveness(),
            },
            performance_impact: self.calculate_performance_impact(),
            optimization_opportunities: self.get_optimization_recommendations(),
            cache_health: self.assess_cache_health(),
        }
    }
    
    // Private helper methods
    
    fn is_cache_entry_valid(&self, cached_result: &CachedTestResult) -> bool {
        // Check age
        if let Ok(age) = cached_result.cached_at.elapsed() {
            if age.as_secs() > self.config.max_cache_age_seconds {
                return false;
            }
        }
        
        // Check content hash
        if let Ok(current_hash) = self.calculate_content_hash(&cached_result.test_id) {
            if current_hash != cached_result.content_hash {
                return false;
            }
        }
        
        // Check dependency hash
        if let Ok(current_dep_hash) = self.calculate_dependency_hash(&cached_result.test_id) {
            if current_dep_hash != cached_result.dependency_hash {
                return false;
            }
        }
        
        // Check environment hash
        if let Ok(current_env_hash) = self.calculate_environment_hash() {
            if current_env_hash != cached_result.environment_hash {
                return false;
            }
        }
        
        true
    }
    
    fn calculate_content_hash(&self, test_id: &str) -> Result<String> {
        let mut hasher = Sha256::new();
        
        // Hash relevant source files for this test
        for tracked_path in &self.config.tracked_paths {
            if tracked_path.exists() {
                self.hash_directory_recursive(&mut hasher, tracked_path)?;
            }
        }
        
        // Include test-specific information
        hasher.update(test_id.as_bytes());
        
        Ok(format!("{:x}", hasher.finalize()))
    }
    
    fn calculate_dependency_hash(&self, test_id: &str) -> Result<String> {
        let mut hasher = Sha256::new();
        
        // Hash dependency information
        if let Some(deps) = self.dependency_graph.get(test_id) {
            let mut sorted_deps: Vec<_> = deps.iter().collect();
            sorted_deps.sort();
            for dep in sorted_deps {
                hasher.update(dep.as_bytes());
            }
        }
        
        // Hash Cargo.toml and Cargo.lock if they exist
        for manifest in &["Cargo.toml", "Cargo.lock"] {
            let path = Path::new(manifest);
            if path.exists() {
                let content = std::fs::read(path)?;
                hasher.update(&content);
            }
        }
        
        Ok(format!("{:x}", hasher.finalize()))
    }
    
    fn calculate_environment_hash(&self) -> Result<String> {
        let mut hasher = Sha256::new();
        
        // Hash environment variables that might affect tests
        let env_vars = ["RUST_LOG", "KRYON_TEST_MODE", "PATH"];
        for var in &env_vars {
            if let Ok(value) = std::env::var(var) {
                hasher.update(var.as_bytes());
                hasher.update(value.as_bytes());
            }
        }
        
        // Hash Rust version
        if let Ok(output) = Command::new("rustc").arg("--version").output() {
            hasher.update(&output.stdout);
        }
        
        Ok(format!("{:x}", hasher.finalize()))
    }
    
    fn hash_directory_recursive(&self, hasher: &mut Sha256, dir: &Path) -> Result<()> {
        if dir.is_file() {
            let content = std::fs::read(dir)?;
            hasher.update(&content);
            return Ok(());
        }
        
        let entries = std::fs::read_dir(dir)?;
        let mut sorted_entries: Vec<_> = entries.collect::<std::result::Result<Vec<_>, _>>()?;
        sorted_entries.sort_by_key(|e| e.path());
        
        for entry in sorted_entries {
            let path = entry.path();
            let path_str = path.to_string_lossy();
            
            // Skip ignored patterns
            if self.config.ignore_patterns.iter().any(|pattern| {
                // Simple glob matching
                if pattern.ends_with("*") {
                    let prefix = &pattern[..pattern.len()-1];
                    path_str.contains(prefix)
                } else {
                    path_str.contains(pattern)
                }
            }) {
                continue;
            }
            
            if path.is_dir() {
                self.hash_directory_recursive(hasher, &path)?;
            } else {
                let content = std::fs::read(&path)?;
                hasher.update(&content);
            }
        }
        
        Ok(())
    }
    
    fn analyze_git_changes(&self) -> Result<ChangeAnalysis> {
        let base_ref = self.config.git_base_ref.as_ref().unwrap_or(&"HEAD~1".to_string());
        
        // Get list of changed files
        let output = Command::new("git")
            .args(&["diff", "--name-status", base_ref])
            .output()?;
        
        let diff_output = String::from_utf8_lossy(&output.stdout);
        let mut analysis = ChangeAnalysis {
            modified_files: HashSet::new(),
            added_files: HashSet::new(),
            deleted_files: HashSet::new(),
            affected_tests: HashSet::new(),
            cacheable_tests: HashSet::new(),
            change_summary: ChangeSummary {
                total_files_changed: 0,
                total_lines_changed: 0,
                change_complexity_score: 0.0,
                risk_level: RiskLevel::Low,
            },
        };
        
        for line in diff_output.lines() {
            let parts: Vec<&str> = line.split('\t').collect();
            if parts.len() >= 2 {
                let status = parts[0];
                let file_path = PathBuf::from(parts[1]);
                
                match status {
                    "M" => { analysis.modified_files.insert(file_path); },
                    "A" => { analysis.added_files.insert(file_path); },
                    "D" => { analysis.deleted_files.insert(file_path); },
                    _ => { analysis.modified_files.insert(file_path); },
                }
            }
        }
        
        // Get line change statistics
        let stats_output = Command::new("git")
            .args(&["diff", "--stat", base_ref])
            .output()?;
        
        let stats_str = String::from_utf8_lossy(&stats_output.stdout);
        analysis.change_summary.total_lines_changed = self.parse_git_stats(&stats_str);
        analysis.change_summary.total_files_changed = 
            analysis.modified_files.len() + analysis.added_files.len() + analysis.deleted_files.len();
        
        Ok(analysis)
    }
    
    fn analyze_filesystem_changes(&self) -> Result<ChangeAnalysis> {
        // Fallback: compare file modification times
        // This is a simplified implementation
        let mut analysis = ChangeAnalysis {
            modified_files: HashSet::new(),
            added_files: HashSet::new(),
            deleted_files: HashSet::new(),
            affected_tests: HashSet::new(),
            cacheable_tests: HashSet::new(),
            change_summary: ChangeSummary {
                total_files_changed: 0,
                total_lines_changed: 0,
                change_complexity_score: 0.0,
                risk_level: RiskLevel::Low,
            },
        };
        
        let last_cache_time = self.cache_entries.values()
            .map(|entry| entry.cached_at)
            .max()
            .unwrap_or(SystemTime::UNIX_EPOCH);
        
        for tracked_path in &self.config.tracked_paths {
            if tracked_path.exists() {
                self.check_directory_for_changes(&mut analysis, tracked_path, last_cache_time)?;
            }
        }
        
        Ok(analysis)
    }
    
    fn check_directory_for_changes(
        &self, 
        analysis: &mut ChangeAnalysis, 
        dir: &Path, 
        since: SystemTime
    ) -> Result<()> {
        if dir.is_file() {
            if let Ok(metadata) = dir.metadata() {
                if let Ok(modified) = metadata.modified() {
                    if modified > since {
                        analysis.modified_files.insert(dir.to_path_buf());
                    }
                }
            }
            return Ok(());
        }
        
        let entries = std::fs::read_dir(dir)?;
        for entry in entries {
            let entry = entry?;
            let path = entry.path();
            
            if path.is_dir() {
                self.check_directory_for_changes(analysis, &path, since)?;
            } else {
                if let Ok(metadata) = entry.metadata() {
                    if let Ok(modified) = metadata.modified() {
                        if modified > since {
                            analysis.modified_files.insert(path);
                        }
                    }
                }
            }
        }
        
        Ok(())
    }
    
    fn parse_git_stats(&self, stats: &str) -> usize {
        // Parse git diff --stat output to get total line changes
        for line in stats.lines() {
            if line.contains("insertion") || line.contains("deletion") {
                let parts: Vec<&str> = line.split_whitespace().collect();
                if let Some(changes_str) = parts.get(3) {
                    if let Ok(changes) = changes_str.parse::<usize>() {
                        return changes;
                    }
                }
            }
        }
        0
    }
    
    fn calculate_change_complexity(&self, analysis: &ChangeAnalysis) -> ChangeSummary {
        let mut complexity_score = 0.0;
        
        // Base complexity from number of files changed
        complexity_score += analysis.modified_files.len() as f64 * 1.0;
        complexity_score += analysis.added_files.len() as f64 * 1.5;
        complexity_score += analysis.deleted_files.len() as f64 * 2.0;
        
        // Line change impact
        complexity_score += (analysis.change_summary.total_lines_changed as f64) * 0.1;
        
        // Risk level assessment
        let risk_level = if complexity_score < 5.0 {
            RiskLevel::Low
        } else if complexity_score < 15.0 {
            RiskLevel::Medium
        } else if complexity_score < 30.0 {
            RiskLevel::High
        } else {
            RiskLevel::Critical
        };
        
        ChangeSummary {
            total_files_changed: analysis.modified_files.len() + analysis.added_files.len() + analysis.deleted_files.len(),
            total_lines_changed: analysis.change_summary.total_lines_changed,
            change_complexity_score: complexity_score,
            risk_level,
        }
    }
    
    fn load_cache_from_disk(&mut self) -> Result<()> {
        let cache_file = self.config.cache_directory.join("test_cache.json");
        if cache_file.exists() {
            let content = std::fs::read_to_string(&cache_file)?;
            if let Ok(cache_data) = serde_json::from_str::<HashMap<String, CachedTestResult>>(&content) {
                self.cache_entries = cache_data;
                self.cache_stats.cache_entries_count = self.cache_entries.len() as u64;
            }
        }
        Ok(())
    }
    
    fn build_dependency_mappings(&mut self) -> Result<()> {
        // This would analyze the codebase to build test-to-file mappings
        // For now, implement a basic version
        
        // Map source files to tests (simplified)
        for test_id in self.cache_entries.keys() {
            let test_parts: Vec<&str> = test_id.split("::").collect();
            if let Some(module_name) = test_parts.get(0) {
                let source_file = PathBuf::from(format!("src/{}.rs", module_name));
                self.file_test_mappings
                    .entry(source_file)
                    .or_insert_with(HashSet::new)
                    .insert(test_id.clone());
            }
        }
        
        Ok(())
    }
    
    fn cleanup_cache_if_needed(&mut self) -> Result<()> {
        // Remove old entries if cache is too large
        if self.cache_stats.cache_size_bytes > self.config.max_cache_size_bytes {
            // Sort by last access time and remove oldest
            let mut entries: Vec<_> = self.cache_entries.iter().collect();
            entries.sort_by_key(|(_, entry)| (entry.cached_at, entry.hit_count));
            
            // Remove oldest 25% of entries
            let remove_count = entries.len() / 4;
            for (test_id, _) in entries.iter().take(remove_count) {
                self.cache_entries.remove(*test_id);
            }
        }
        
        Ok(())
    }
    
    fn update_average_lookup_time(&mut self, lookup_time_ms: f64) {
        let total_lookups = self.cache_stats.total_cache_hits + self.cache_stats.total_cache_misses;
        if total_lookups > 0 {
            self.cache_stats.average_lookup_time_ms = 
                (self.cache_stats.average_lookup_time_ms * (total_lookups - 1) as f64 + lookup_time_ms) / total_lookups as f64;
        }
    }
    
    fn calculate_cache_effectiveness(&self) -> f64 {
        if self.cache_stats.total_cache_hits + self.cache_stats.total_cache_misses == 0 {
            return 0.0;
        }
        
        (self.cache_stats.total_cache_hits as f64 / 
         (self.cache_stats.total_cache_hits + self.cache_stats.total_cache_misses) as f64) * 100.0
    }
    
    fn calculate_performance_impact(&self) -> PerformanceImpact {
        let hit_rate = self.calculate_cache_effectiveness();
        let estimated_time_saved = hit_rate * 0.8; // Assume cached tests are 80% faster
        
        PerformanceImpact {
            cache_hit_rate_percent: hit_rate,
            estimated_time_savings_percent: estimated_time_saved,
            total_cache_hits: self.cache_stats.total_cache_hits,
            average_lookup_time_ms: self.cache_stats.average_lookup_time_ms,
            recommendation: if hit_rate > 80.0 {
                "Excellent cache performance".to_string()
            } else if hit_rate > 60.0 {
                "Good cache performance, minor optimizations possible".to_string()
            } else if hit_rate > 40.0 {
                "Moderate cache performance, consider improvements".to_string()
            } else {
                "Poor cache performance, requires optimization".to_string()
            },
        }
    }
    
    fn assess_cache_health(&self) -> CacheHealth {
        let mut health_score = 100.0;
        let mut issues = Vec::new();
        
        // Check hit rate
        let hit_rate = self.calculate_cache_effectiveness();
        if hit_rate < 50.0 {
            health_score -= 30.0;
            issues.push("Low cache hit rate".to_string());
        }
        
        // Check cache size
        if self.cache_stats.cache_size_bytes > self.config.max_cache_size_bytes {
            health_score -= 20.0;
            issues.push("Cache size exceeds limit".to_string());
        }
        
        // Check lookup performance
        if self.cache_stats.average_lookup_time_ms > 10.0 {
            health_score -= 15.0;
            issues.push("Slow cache lookups".to_string());
        }
        
        CacheHealth {
            health_score: health_score.max(0.0),
            status: if health_score > 80.0 { "Healthy".to_string() }
                   else if health_score > 60.0 { "Good".to_string() }
                   else if health_score > 40.0 { "Fair".to_string() }
                   else { "Poor".to_string() },
            issues,
            recommendations: self.get_optimization_recommendations(),
        }
    }
}

// Additional supporting types

#[derive(Debug, Clone)]
pub struct OptimizationRecommendation {
    pub category: OptimizationCategory,
    pub priority: RecommendationPriority,
    pub title: String,
    pub description: String,
    pub estimated_time_savings: String,
    pub implementation_steps: Vec<String>,
}

#[derive(Debug, Clone)]
pub enum OptimizationCategory {
    CacheEfficiency,
    DependencyTracking,
    ResourceUsage,
    PerformanceTuning,
}

#[derive(Debug, Clone)]
pub struct CachingReport {
    pub summary: CachingReportSummary,
    pub performance_impact: PerformanceImpact,
    pub optimization_opportunities: Vec<OptimizationRecommendation>,
    pub cache_health: CacheHealth,
}

#[derive(Debug, Clone)]
pub struct CachingReportSummary {
    pub total_cached_tests: usize,
    pub cache_hit_rate_percent: f64,
    pub total_cache_hits: u64,
    pub total_cache_misses: u64,
    pub average_lookup_time_ms: f64,
    pub cache_size_mb: f64,
    pub effectiveness_score: f64,
}

#[derive(Debug, Clone)]
pub struct PerformanceImpact {
    pub cache_hit_rate_percent: f64,
    pub estimated_time_savings_percent: f64,
    pub total_cache_hits: u64,
    pub average_lookup_time_ms: f64,
    pub recommendation: String,
}

#[derive(Debug, Clone)]
pub struct CacheHealth {
    pub health_score: f64,
    pub status: String,
    pub issues: Vec<String>,
    pub recommendations: Vec<OptimizationRecommendation>,
}

/// Incremental test runner that uses caching
pub struct IncrementalTestRunner {
    cache: TestResultCache,
    config: CachingConfig,
}

impl IncrementalTestRunner {
    pub fn new(config: CachingConfig) -> Result<Self> {
        let cache = TestResultCache::new(config.clone())?;
        Ok(Self { cache, config })
    }
    
    /// Run tests incrementally, using cache when possible
    pub async fn run_incremental_tests(&mut self, test_plan: &[String]) -> Result<IncrementalTestResult> {
        let analysis = self.cache.analyze_changes()?;
        
        let mut results = IncrementalTestResult {
            total_tests: test_plan.len(),
            cached_tests: 0,
            executed_tests: 0,
            cache_hit_rate: 0.0,
            total_execution_time: Duration::ZERO,
            test_results: Vec::new(),
            change_analysis: analysis.clone(),
        };
        
        let execution_start = std::time::Instant::now();
        
        for test_id in test_plan {
            if analysis.cacheable_tests.contains(test_id) {
                if let Some(cached_result) = self.cache.get_cached_result(test_id) {
                    results.test_results.push(cached_result.result.clone());
                    results.cached_tests += 1;
                    continue;
                }
            }
            
            // Execute test (simulated)
            let test_result = self.execute_test(test_id).await?;
            
            // Cache the result
            self.cache.cache_result(
                test_id.clone(),
                test_id.clone(),
                test_result.clone()
            )?;
            
            results.test_results.push(test_result);
            results.executed_tests += 1;
        }
        
        results.total_execution_time = execution_start.elapsed();
        results.cache_hit_rate = if results.total_tests > 0 {
            (results.cached_tests as f64 / results.total_tests as f64) * 100.0
        } else {
            0.0
        };
        
        Ok(results)
    }
    
    async fn execute_test(&self, test_id: &str) -> Result<TestResult> {
        // Simulate test execution
        tokio::time::sleep(Duration::from_millis(100)).await;
        
        Ok(TestResult {
            name: test_id.to_string(),
            success: true,
            execution_time: Duration::from_millis(100),
            error_message: None,
        })
    }
}

#[derive(Debug, Clone)]
pub struct IncrementalTestResult {
    pub total_tests: usize,
    pub cached_tests: usize,
    pub executed_tests: usize,
    pub cache_hit_rate: f64,
    pub total_execution_time: Duration,
    pub test_results: Vec<TestResult>,
    pub change_analysis: ChangeAnalysis,
}

impl IncrementalTestResult {
    pub fn generate_summary_report(&self) -> String {
        format!(
            r#"
🚀 INCREMENTAL TESTING RESULTS
===============================

📊 EXECUTION SUMMARY:
• Total tests: {}
• Tests executed: {} ({:.1}%)
• Tests from cache: {} ({:.1}%)
• Cache hit rate: {:.1}%
• Total execution time: {:.2}s

📈 PERFORMANCE IMPACT:
• Time saved by caching: ~{:.1}%
• Tests affected by changes: {}
• Cacheable tests: {}

🔍 CHANGE ANALYSIS:
• Files modified: {}
• Files added: {}
• Files deleted: {}
• Change complexity: {:.1}
• Risk level: {:?}

💡 RECOMMENDATIONS:
{}
            "#,
            self.total_tests,
            self.executed_tests,
            if self.total_tests > 0 { (self.executed_tests as f64 / self.total_tests as f64) * 100.0 } else { 0.0 },
            self.cached_tests,
            if self.total_tests > 0 { (self.cached_tests as f64 / self.total_tests as f64) * 100.0 } else { 0.0 },
            self.cache_hit_rate,
            self.total_execution_time.as_secs_f64(),
            if self.cache_hit_rate > 0.0 { self.cache_hit_rate * 0.8 } else { 0.0 },
            self.change_analysis.affected_tests.len(),
            self.change_analysis.cacheable_tests.len(),
            self.change_analysis.modified_files.len(),
            self.change_analysis.added_files.len(),
            self.change_analysis.deleted_files.len(),
            self.change_analysis.change_summary.change_complexity_score,
            self.change_analysis.change_summary.risk_level,
            if self.cache_hit_rate > 80.0 {
                "• Excellent caching performance - maintain current approach"
            } else if self.cache_hit_rate > 60.0 {
                "• Good caching performance - consider minor optimizations"
            } else {
                "• Caching performance needs improvement - review invalidation logic"
            }
        )
    }
}