//! Test Dependency Management System
//! 
//! This module provides comprehensive test dependency management, allowing tests to be
//! organized based on their relationships, prerequisites, and execution order requirements.

use crate::prelude::*;
use serde::{Deserialize, Serialize};
use std::collections::{HashMap, HashSet, VecDeque};

/// Test dependency configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DependencyConfig {
    pub enable_dependency_tracking: bool,
    pub enable_parallel_execution: bool,
    pub max_concurrent_tests: usize,
    pub dependency_timeout_seconds: u64,
    pub fail_on_circular_dependency: bool,
    pub enable_smart_ordering: bool,
    pub cache_dependency_graph: bool,
}

/// Types of test dependencies
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum DependencyType {
    /// Test B requires Test A to complete successfully
    Sequential,
    /// Test B requires Test A's output/state
    DataDependency,
    /// Test B requires Test A's setup/environment
    EnvironmentDependency,
    /// Test B conflicts with Test A (cannot run simultaneously)
    Conflict,
    /// Test B should run after Test A if both are selected
    SoftDependency,
    /// Test B requires specific resources that Test A provides
    ResourceDependency,
}

/// Test dependency relationship
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestDependency {
    pub dependent_test: String,
    pub prerequisite_test: String,
    pub dependency_type: DependencyType,
    pub description: String,
    pub optional: bool,
    pub timeout_override: Option<Duration>,
}

/// Test execution node in dependency graph
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestNode {
    pub test_id: String,
    pub test_name: String,
    pub test_category: String,
    pub estimated_duration: Duration,
    pub resource_requirements: Vec<String>,
    pub provides_resources: Vec<String>,
    pub dependencies: Vec<TestDependency>,
    pub status: TestExecutionStatus,
    pub priority: TestPriority,
    pub can_run_in_parallel: bool,
    pub retry_count: usize,
    pub max_retries: usize,
}

/// Test execution status
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum TestExecutionStatus {
    Pending,
    Ready,
    Running,
    Completed,
    Failed,
    Skipped,
    Blocked,
    Cancelled,
}

/// Test priority levels
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
pub enum TestPriority {
    Critical,
    High,
    Medium,
    Low,
}

/// Dependency resolution result
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DependencyResolutionResult {
    pub execution_order: Vec<Vec<String>>, // Batches of tests that can run in parallel
    pub total_estimated_time: Duration,
    pub parallel_efficiency: f64,
    pub circular_dependencies: Vec<CircularDependency>,
    pub unresolved_dependencies: Vec<UnresolvedDependency>,
    pub resource_conflicts: Vec<ResourceConflict>,
    pub optimization_suggestions: Vec<OptimizationSuggestion>,
}

/// Circular dependency detection
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CircularDependency {
    pub test_chain: Vec<String>,
    pub dependency_types: Vec<DependencyType>,
    pub severity: DependencySeverity,
    pub suggested_resolution: String,
}

/// Unresolved dependency
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UnresolvedDependency {
    pub dependent_test: String,
    pub missing_prerequisite: String,
    pub dependency_type: DependencyType,
    pub impact_assessment: String,
}

/// Resource conflict detection
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResourceConflict {
    pub resource_name: String,
    pub conflicting_tests: Vec<String>,
    pub conflict_type: ConflictType,
    pub resolution_strategy: ConflictResolutionStrategy,
}

/// Types of resource conflicts
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ConflictType {
    ExclusiveAccess,
    PortBinding,
    FileSystem,
    DatabaseConnection,
    NetworkResource,
    MemoryResource,
}

/// Strategies for resolving conflicts
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ConflictResolutionStrategy {
    Sequential,
    ResourcePooling,
    Virtualization,
    MockingSubstitution,
    TemporaryAllocation,
}

/// Dependency severity levels
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
pub enum DependencySeverity {
    Critical,
    High,
    Medium,
    Low,
    Info,
}

/// Optimization suggestions
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OptimizationSuggestion {
    pub suggestion_type: OptimizationType,
    pub title: String,
    pub description: String,
    pub estimated_time_savings: Duration,
    pub implementation_effort: String,
    pub affected_tests: Vec<String>,
}

/// Types of optimizations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum OptimizationType {
    ParallelizationOpportunity,
    DependencyReduction,
    ResourceOptimization,
    TestGrouping,
    CachingOpportunity,
    SetupConsolidation,
}

/// Test dependency manager
#[derive(Debug)]
pub struct TestDependencyManager {
    pub config: DependencyConfig,
    pub nodes: HashMap<String, TestNode>,
    pub global_dependencies: Vec<TestDependency>,
    pub resource_pool: HashMap<String, ResourceInfo>,
    pub execution_history: Vec<ExecutionHistoryEntry>,
}

/// Resource information
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResourceInfo {
    pub resource_id: String,
    pub resource_type: String,
    pub max_concurrent_users: usize,
    pub current_users: usize,
    pub allocation_timeout: Duration,
    pub cleanup_required: bool,
}

/// Execution history entry
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExecutionHistoryEntry {
    pub test_id: String,
    pub execution_time: Duration,
    pub success: bool,
    pub dependencies_met: bool,
    pub resource_utilization: HashMap<String, f64>,
    pub timestamp: std::time::SystemTime,
}

impl Default for DependencyConfig {
    fn default() -> Self {
        Self {
            enable_dependency_tracking: true,
            enable_parallel_execution: true,
            max_concurrent_tests: num_cpus::get(),
            dependency_timeout_seconds: 300,
            fail_on_circular_dependency: true,
            enable_smart_ordering: true,
            cache_dependency_graph: true,
        }
    }
}

impl TestDependencyManager {
    pub fn new(config: DependencyConfig) -> Self {
        Self {
            config,
            nodes: HashMap::new(),
            global_dependencies: Vec::new(),
            resource_pool: HashMap::new(),
            execution_history: Vec::new(),
        }
    }

    /// Add a test node to the dependency graph
    pub fn add_test_node(&mut self, test_node: TestNode) -> Result<()> {
        if self.nodes.contains_key(&test_node.test_id) {
            return Err(Error::Config(format!("Test node '{}' already exists", test_node.test_id)));
        }

        // Validate dependencies exist
        for dep in &test_node.dependencies {
            if !self.nodes.contains_key(&dep.prerequisite_test) && dep.prerequisite_test != "global" {
                println!("⚠️  Warning: Prerequisite test '{}' not found for '{}'", 
                    dep.prerequisite_test, test_node.test_id);
            }
        }

        self.nodes.insert(test_node.test_id.clone(), test_node);
        println!("✅ Added test node: {}", self.nodes.len());
        Ok(())
    }

    /// Add a global dependency that applies to multiple tests
    pub fn add_global_dependency(&mut self, dependency: TestDependency) {
        self.global_dependencies.push(dependency);
    }

    /// Register a resource in the resource pool
    pub fn register_resource(&mut self, resource: ResourceInfo) {
        self.resource_pool.insert(resource.resource_id.clone(), resource);
    }

    /// Resolve test dependencies and create execution plan
    pub fn resolve_dependencies(&self) -> Result<DependencyResolutionResult> {
        println!("🔍 Resolving test dependencies...");

        // Detect circular dependencies
        let circular_deps = self.detect_circular_dependencies()?;
        if !circular_deps.is_empty() && self.config.fail_on_circular_dependency {
            return Err(Error::Config(format!("Circular dependencies detected: {}", 
                circular_deps.len())));
        }

        // Find unresolved dependencies
        let unresolved_deps = self.find_unresolved_dependencies();

        // Detect resource conflicts
        let resource_conflicts = self.detect_resource_conflicts();

        // Create execution order using topological sort
        let execution_order = self.create_execution_order()?;

        // Calculate timing estimates
        let total_estimated_time = self.calculate_total_time(&execution_order);
        let parallel_efficiency = self.calculate_parallel_efficiency(&execution_order);

        // Generate optimization suggestions
        let optimization_suggestions = self.generate_optimization_suggestions(&execution_order);

        Ok(DependencyResolutionResult {
            execution_order,
            total_estimated_time,
            parallel_efficiency,
            circular_dependencies: circular_deps,
            unresolved_dependencies: unresolved_deps,
            resource_conflicts,
            optimization_suggestions,
        })
    }

    /// Detect circular dependencies in the test graph
    fn detect_circular_dependencies(&self) -> Result<Vec<CircularDependency>> {
        let mut circular_deps = Vec::new();
        let mut visited = HashSet::new();
        let mut rec_stack = HashSet::new();

        for node_id in self.nodes.keys() {
            if !visited.contains(node_id) {
                if let Some(cycle) = self.detect_cycle_dfs(node_id, &mut visited, &mut rec_stack, &mut Vec::new())? {
                    // Analyze the circular dependency
                    let dependency_types = self.analyze_cycle_types(&cycle);
                    let severity = self.assess_cycle_severity(&cycle, &dependency_types);
                    let suggested_resolution = self.suggest_cycle_resolution(&cycle, &dependency_types);

                    circular_deps.push(CircularDependency {
                        test_chain: cycle,
                        dependency_types,
                        severity,
                        suggested_resolution,
                    });
                }
            }
        }

        Ok(circular_deps)
    }

    /// DFS-based cycle detection
    fn detect_cycle_dfs(
        &self,
        node_id: &str,
        visited: &mut HashSet<String>,
        rec_stack: &mut HashSet<String>,
        path: &mut Vec<String>,
    ) -> Result<Option<Vec<String>>> {
        visited.insert(node_id.to_string());
        rec_stack.insert(node_id.to_string());
        path.push(node_id.to_string());

        if let Some(node) = self.nodes.get(node_id) {
            for dep in &node.dependencies {
                let prereq = &dep.prerequisite_test;
                
                if rec_stack.contains(prereq) {
                    // Found a cycle
                    let cycle_start = path.iter().position(|x| x == prereq).unwrap_or(0);
                    return Ok(Some(path[cycle_start..].to_vec()));
                }

                if !visited.contains(prereq) {
                    if let Some(cycle) = self.detect_cycle_dfs(prereq, visited, rec_stack, path)? {
                        return Ok(Some(cycle));
                    }
                }
            }
        }

        rec_stack.remove(node_id);
        path.pop();
        Ok(None)
    }

    /// Find tests with unresolved dependencies
    fn find_unresolved_dependencies(&self) -> Vec<UnresolvedDependency> {
        let mut unresolved = Vec::new();

        for node in self.nodes.values() {
            for dep in &node.dependencies {
                if !self.nodes.contains_key(&dep.prerequisite_test) && dep.prerequisite_test != "global" {
                    let impact = if dep.optional {
                        "Optional dependency - test can proceed with reduced functionality".to_string()
                    } else {
                        "Critical dependency - test will be blocked".to_string()
                    };

                    unresolved.push(UnresolvedDependency {
                        dependent_test: node.test_id.clone(),
                        missing_prerequisite: dep.prerequisite_test.clone(),
                        dependency_type: dep.dependency_type.clone(),
                        impact_assessment: impact,
                    });
                }
            }
        }

        unresolved
    }

    /// Detect resource conflicts between tests
    fn detect_resource_conflicts(&self) -> Vec<ResourceConflict> {
        let mut conflicts = Vec::new();
        let mut resource_usage: HashMap<String, Vec<String>> = HashMap::new();

        // Map resource usage
        for node in self.nodes.values() {
            for resource in &node.resource_requirements {
                resource_usage.entry(resource.clone())
                    .or_insert_with(Vec::new)
                    .push(node.test_id.clone());
            }
        }

        // Detect conflicts
        for (resource_name, users) in resource_usage {
            if users.len() > 1 {
                if let Some(resource_info) = self.resource_pool.get(&resource_name) {
                    if resource_info.max_concurrent_users < users.len() {
                        let conflict_type = self.classify_conflict_type(&resource_name);
                        let resolution_strategy = self.suggest_conflict_resolution(&conflict_type, &users);

                        conflicts.push(ResourceConflict {
                            resource_name,
                            conflicting_tests: users,
                            conflict_type,
                            resolution_strategy,
                        });
                    }
                }
            }
        }

        conflicts
    }

    /// Create execution order using topological sort
    fn create_execution_order(&self) -> Result<Vec<Vec<String>>> {
        let mut in_degree: HashMap<String, usize> = HashMap::new();
        let mut graph: HashMap<String, Vec<String>> = HashMap::new();

        // Initialize graph
        for node_id in self.nodes.keys() {
            in_degree.insert(node_id.clone(), 0);
            graph.insert(node_id.clone(), Vec::new());
        }

        // Build graph and calculate in-degrees
        for node in self.nodes.values() {
            for dep in &node.dependencies {
                if self.nodes.contains_key(&dep.prerequisite_test) {
                    graph.get_mut(&dep.prerequisite_test)
                        .unwrap()
                        .push(node.test_id.clone());
                    *in_degree.get_mut(&node.test_id).unwrap() += 1;
                }
            }
        }

        // Topological sort with parallel batching
        let mut execution_order = Vec::new();
        let mut queue: VecDeque<String> = VecDeque::new();

        // Start with nodes that have no dependencies
        for (node_id, &degree) in &in_degree {
            if degree == 0 {
                queue.push_back(node_id.clone());
            }
        }

        while !queue.is_empty() {
            let mut current_batch = Vec::new();
            let current_batch_size = queue.len();

            // Process all nodes at current level
            for _ in 0..current_batch_size {
                if let Some(node_id) = queue.pop_front() {
                    current_batch.push(node_id.clone());

                    // Update dependencies
                    if let Some(neighbors) = graph.get(&node_id) {
                        for neighbor in neighbors {
                            if let Some(degree) = in_degree.get_mut(neighbor) {
                                *degree -= 1;
                                if *degree == 0 {
                                    queue.push_back(neighbor.clone());
                                }
                            }
                        }
                    }
                }
            }

            if !current_batch.is_empty() {
                // Sort batch by priority and estimated duration
                current_batch.sort_by(|a, b| {
                    let node_a = self.nodes.get(a).unwrap();
                    let node_b = self.nodes.get(b).unwrap();
                    
                    match node_a.priority.cmp(&node_b.priority) {
                        std::cmp::Ordering::Equal => node_a.estimated_duration.cmp(&node_b.estimated_duration),
                        other => other,
                    }
                });

                execution_order.push(current_batch);
            }
        }

        // Check for remaining nodes (indicates circular dependency)
        let processed_count: usize = execution_order.iter().map(|batch| batch.len()).sum();
        if processed_count != self.nodes.len() {
            return Err(Error::Config("Circular dependency detected in topological sort".to_string()));
        }

        Ok(execution_order)
    }

    /// Calculate total estimated execution time
    fn calculate_total_time(&self, execution_order: &[Vec<String>]) -> Duration {
        let mut total_time = Duration::ZERO;

        for batch in execution_order {
            // For parallel execution, take the maximum duration in the batch
            let batch_time = batch.iter()
                .filter_map(|test_id| self.nodes.get(test_id))
                .map(|node| node.estimated_duration)
                .max()
                .unwrap_or(Duration::ZERO);
            
            total_time += batch_time;
        }

        total_time
    }

    /// Calculate parallel execution efficiency
    fn calculate_parallel_efficiency(&self, execution_order: &[Vec<String>]) -> f64 {
        if execution_order.is_empty() {
            return 0.0;
        }

        let total_sequential_time: Duration = self.nodes.values()
            .map(|node| node.estimated_duration)
            .sum();

        let parallel_time = self.calculate_total_time(execution_order);

        if parallel_time.is_zero() {
            return 0.0;
        }

        let efficiency = total_sequential_time.as_secs_f64() / parallel_time.as_secs_f64();
        (efficiency * 100.0).min(100.0)
    }

    /// Generate optimization suggestions
    fn generate_optimization_suggestions(&self, execution_order: &[Vec<String>]) -> Vec<OptimizationSuggestion> {
        let mut suggestions = Vec::new();

        // Suggest parallelization opportunities
        for (i, batch) in execution_order.iter().enumerate() {
            if batch.len() == 1 {
                let test_id = &batch[0];
                if let Some(node) = self.nodes.get(test_id) {
                    if node.can_run_in_parallel && node.dependencies.iter().all(|d| d.dependency_type != DependencyType::Sequential) {
                        suggestions.push(OptimizationSuggestion {
                            suggestion_type: OptimizationType::ParallelizationOpportunity,
                            title: format!("Parallelize test '{}'", test_id),
                            description: "This test could potentially run in parallel with others".to_string(),
                            estimated_time_savings: node.estimated_duration / 2,
                            implementation_effort: "Low - Review dependencies and enable parallel execution".to_string(),
                            affected_tests: vec![test_id.clone()],
                        });
                    }
                }
            }
        }

        // Suggest dependency reduction
        let high_dependency_tests: Vec<_> = self.nodes.values()
            .filter(|node| node.dependencies.len() > 3)
            .collect();

        for node in high_dependency_tests {
            suggestions.push(OptimizationSuggestion {
                suggestion_type: OptimizationType::DependencyReduction,
                title: format!("Reduce dependencies for '{}'", node.test_id),
                description: format!("Test has {} dependencies which may slow execution", node.dependencies.len()),
                estimated_time_savings: Duration::from_secs(30),
                implementation_effort: "Medium - Analyze and remove unnecessary dependencies".to_string(),
                affected_tests: vec![node.test_id.clone()],
            });
        }

        // Suggest resource optimization
        let resource_conflicts = self.detect_resource_conflicts();
        for conflict in resource_conflicts {
            suggestions.push(OptimizationSuggestion {
                suggestion_type: OptimizationType::ResourceOptimization,
                title: format!("Optimize resource usage for '{}'", conflict.resource_name),
                description: format!("Resource conflict affects {} tests", conflict.conflicting_tests.len()),
                estimated_time_savings: Duration::from_secs(60),
                implementation_effort: "High - Implement resource pooling or mocking".to_string(),
                affected_tests: conflict.conflicting_tests,
            });
        }

        suggestions
    }

    /// Print dependency analysis summary
    pub fn print_dependency_summary(&self, result: &DependencyResolutionResult) {
        println!("\n╔═══════════════════════════════════════════════════════════════╗");
        println!("║                🔗 TEST DEPENDENCY ANALYSIS                    ║");
        println!("╚═══════════════════════════════════════════════════════════════╝");

        println!("\n📊 Execution Plan:");
        println!("  Total Tests: {}", self.nodes.len());
        println!("  Execution Batches: {}", result.execution_order.len());
        println!("  Estimated Time: {:.2}s", result.total_estimated_time.as_secs_f64());
        println!("  Parallel Efficiency: {:.1}%", result.parallel_efficiency);

        if !result.circular_dependencies.is_empty() {
            println!("\n⚠️  Circular Dependencies: {}", result.circular_dependencies.len());
            for (i, circular) in result.circular_dependencies.iter().take(3).enumerate() {
                println!("  {}. {} ({})", i + 1, circular.test_chain.join(" → "), 
                    format!("{:?}", circular.severity).to_lowercase());
            }
        }

        if !result.unresolved_dependencies.is_empty() {
            println!("\n❌ Unresolved Dependencies: {}", result.unresolved_dependencies.len());
            for (i, unresolved) in result.unresolved_dependencies.iter().take(3).enumerate() {
                println!("  {}. {} requires {}", i + 1, 
                    unresolved.dependent_test, unresolved.missing_prerequisite);
            }
        }

        if !result.resource_conflicts.is_empty() {
            println!("\n🔄 Resource Conflicts: {}", result.resource_conflicts.len());
            for (i, conflict) in result.resource_conflicts.iter().take(3).enumerate() {
                println!("  {}. {} ({} tests affected)", i + 1, 
                    conflict.resource_name, conflict.conflicting_tests.len());
            }
        }

        if !result.optimization_suggestions.is_empty() {
            println!("\n💡 Optimization Opportunities: {}", result.optimization_suggestions.len());
            for (i, suggestion) in result.optimization_suggestions.iter().take(3).enumerate() {
                println!("  {}. {} (saves {:.1}s)", i + 1, 
                    suggestion.title, suggestion.estimated_time_savings.as_secs_f64());
            }
        }

        println!("\n═══════════════════════════════════════════════════════════════");
    }

    // Helper methods
    fn analyze_cycle_types(&self, _cycle: &[String]) -> Vec<DependencyType> {
        // Simplified implementation
        vec![DependencyType::Sequential]
    }

    fn assess_cycle_severity(&self, _cycle: &[String], _types: &[DependencyType]) -> DependencySeverity {
        DependencySeverity::High
    }

    fn suggest_cycle_resolution(&self, _cycle: &[String], _types: &[DependencyType]) -> String {
        "Consider breaking the dependency chain by introducing test fixtures or mocking".to_string()
    }

    fn classify_conflict_type(&self, resource_name: &str) -> ConflictType {
        if resource_name.contains("port") {
            ConflictType::PortBinding
        } else if resource_name.contains("file") {
            ConflictType::FileSystem
        } else if resource_name.contains("db") {
            ConflictType::DatabaseConnection
        } else {
            ConflictType::ExclusiveAccess
        }
    }

    fn suggest_conflict_resolution(&self, conflict_type: &ConflictType, _users: &[String]) -> ConflictResolutionStrategy {
        match conflict_type {
            ConflictType::PortBinding => ConflictResolutionStrategy::TemporaryAllocation,
            ConflictType::DatabaseConnection => ConflictResolutionStrategy::ResourcePooling,
            ConflictType::FileSystem => ConflictResolutionStrategy::Virtualization,
            _ => ConflictResolutionStrategy::Sequential,
        }
    }
}

/// Utility functions for dependency management
pub mod dependency_utils {
    use super::*;

    /// Create a simple test node
    pub fn create_test_node(
        test_id: &str,
        test_name: &str,
        category: &str,
        duration: Duration,
    ) -> TestNode {
        TestNode {
            test_id: test_id.to_string(),
            test_name: test_name.to_string(),
            test_category: category.to_string(),
            estimated_duration: duration,
            resource_requirements: Vec::new(),
            provides_resources: Vec::new(),
            dependencies: Vec::new(),
            status: TestExecutionStatus::Pending,
            priority: TestPriority::Medium,
            can_run_in_parallel: true,
            retry_count: 0,
            max_retries: 3,
        }
    }

    /// Create a test dependency
    pub fn create_dependency(
        dependent: &str,
        prerequisite: &str,
        dep_type: DependencyType,
        description: &str,
    ) -> TestDependency {
        TestDependency {
            dependent_test: dependent.to_string(),
            prerequisite_test: prerequisite.to_string(),
            dependency_type: dep_type,
            description: description.to_string(),
            optional: false,
            timeout_override: None,
        }
    }

    /// Create a resource info entry
    pub fn create_resource(
        resource_id: &str,
        resource_type: &str,
        max_users: usize,
    ) -> ResourceInfo {
        ResourceInfo {
            resource_id: resource_id.to_string(),
            resource_type: resource_type.to_string(),
            max_concurrent_users: max_users,
            current_users: 0,
            allocation_timeout: Duration::from_secs(30),
            cleanup_required: false,
        }
    }
}

pub use dependency_utils::*;