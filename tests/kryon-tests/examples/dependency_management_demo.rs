//! Test Dependency Management Demo
//! 
//! This example demonstrates the comprehensive test dependency management system,
//! showing how to organize tests based on dependencies, resources, and execution order.

use kryon_tests::prelude::*;
use std::time::Duration;

#[tokio::main]
async fn main() -> Result<()> {
    setup_test_environment();
    
    println!("🔗 Kryon Test Dependency Management Demo");
    println!("========================================");
    
    // Demo 1: Basic Dependency Management
    println!("\n1. 🏗️  Basic Dependency Setup");
    demo_basic_dependency_setup().await?;
    
    // Demo 2: Complex Dependency Graph
    println!("\n2. 🕸️  Complex Dependency Graph");
    demo_complex_dependency_graph().await?;
    
    // Demo 3: Resource Conflict Management
    println!("\n3. 🔄 Resource Conflict Management");
    demo_resource_conflict_management().await?;
    
    // Demo 4: Circular Dependency Detection
    println!("\n4. ⚠️  Circular Dependency Detection");
    demo_circular_dependency_detection().await?;
    
    // Demo 5: Optimization Suggestions
    println!("\n5. 💡 Optimization Suggestions");
    demo_optimization_suggestions().await?;
    
    // Demo 6: Real-world Test Scenario
    println!("\n6. 🌍 Real-world Test Scenario");
    demo_real_world_scenario().await?;
    
    println!("\n🎉 Dependency Management Demo Complete!");
    println!("\nKey Benefits Demonstrated:");
    println!("  ✅ Intelligent test ordering based on dependencies");
    println!("  ✅ Parallel execution optimization");
    println!("  ✅ Resource conflict detection and resolution");
    println!("  ✅ Circular dependency detection");
    println!("  ✅ Performance optimization suggestions");
    println!("  ✅ Comprehensive dependency analysis");
    
    Ok(())
}

async fn demo_basic_dependency_setup() -> Result<()> {
    println!("  🔧 Setting up basic dependency manager...");
    
    let config = DependencyConfig {
        enable_dependency_tracking: true,
        enable_parallel_execution: true,
        max_concurrent_tests: 4,
        dependency_timeout_seconds: 120,
        fail_on_circular_dependency: true,
        enable_smart_ordering: true,
        cache_dependency_graph: true,
    };
    
    let mut manager = TestDependencyManager::new(config);
    
    // Create simple test nodes
    let test_a = create_test_node("test_setup", "Setup Tests", "setup", Duration::from_secs(10));
    let mut test_b = create_test_node("test_compilation", "Compilation Tests", "unit", Duration::from_secs(30));
    let mut test_c = create_test_node("test_rendering", "Rendering Tests", "integration", Duration::from_secs(45));
    
    // Add dependencies
    test_b.dependencies.push(create_dependency(
        "test_compilation", 
        "test_setup", 
        DependencyType::Sequential,
        "Compilation requires setup to complete first"
    ));
    
    test_c.dependencies.push(create_dependency(
        "test_rendering", 
        "test_compilation", 
        DependencyType::DataDependency,
        "Rendering tests need compiled output"
    ));
    
    // Add to manager
    manager.add_test_node(test_a)?;
    manager.add_test_node(test_b)?;
    manager.add_test_node(test_c)?;
    
    // Resolve dependencies
    let result = manager.resolve_dependencies()?;
    
    println!("     ✅ Basic dependency resolution completed");
    println!("        Execution batches: {}", result.execution_order.len());
    println!("        Total estimated time: {:.1}s", result.total_estimated_time.as_secs_f64());
    println!("        Parallel efficiency: {:.1}%", result.parallel_efficiency);
    
    // Print execution order
    for (i, batch) in result.execution_order.iter().enumerate() {
        println!("        Batch {}: {}", i + 1, batch.join(", "));
    }
    
    Ok(())
}

async fn demo_complex_dependency_graph() -> Result<()> {
    println!("  🕸️  Creating complex dependency graph...");
    
    let config = DependencyConfig::default();
    let mut manager = TestDependencyManager::new(config);
    
    // Create a realistic test scenario for Kryon UI framework
    let tests = vec![
        ("env_setup", "Environment Setup", "setup", 15, vec![]),
        ("compiler_init", "Compiler Initialization", "setup", 20, vec!["env_setup"]),
        ("lexer_tests", "Lexer Tests", "unit", 25, vec!["compiler_init"]),
        ("parser_tests", "Parser Tests", "unit", 30, vec!["lexer_tests"]),
        ("ast_tests", "AST Generation Tests", "unit", 20, vec!["parser_tests"]),
        ("type_checker_tests", "Type Checker Tests", "unit", 35, vec!["ast_tests"]),
        ("codegen_tests", "Code Generation Tests", "unit", 40, vec!["type_checker_tests"]),
        ("runtime_init", "Runtime Initialization", "setup", 10, vec!["env_setup"]),
        ("layout_engine_tests", "Layout Engine Tests", "integration", 45, vec!["runtime_init", "codegen_tests"]),
        ("renderer_tests", "Renderer Tests", "integration", 50, vec!["layout_engine_tests"]),
        ("ui_component_tests", "UI Component Tests", "integration", 30, vec!["renderer_tests"]),
        ("visual_regression_tests", "Visual Regression Tests", "visual", 60, vec!["ui_component_tests"]),
        ("performance_benchmarks", "Performance Benchmarks", "performance", 120, vec!["renderer_tests"]),
        ("e2e_tests", "End-to-End Tests", "e2e", 180, vec!["visual_regression_tests", "performance_benchmarks"]),
    ];
    
    // Create test nodes with dependencies
    for (test_id, test_name, category, duration_secs, deps) in tests {
        let mut node = create_test_node(test_id, test_name, category, Duration::from_secs(duration_secs));
        
        // Add dependencies
        for dep in deps {
            node.dependencies.push(create_dependency(
                test_id,
                dep,
                DependencyType::Sequential,
                &format!("{} requires {} to complete", test_name, dep)
            ));
        }
        
        // Set priority based on category
        node.priority = match category {
            "setup" => TestPriority::Critical,
            "unit" => TestPriority::High,
            "integration" => TestPriority::Medium,
            "e2e" => TestPriority::Low,
            _ => TestPriority::Medium,
        };
        
        manager.add_test_node(node)?;
    }
    
    // Resolve complex dependencies
    let result = manager.resolve_dependencies()?;
    manager.print_dependency_summary(&result);
    
    println!("\n     📊 Complex Dependency Analysis:");
    println!("        Total tests: {}", manager.nodes.len());
    println!("        Execution batches: {}", result.execution_order.len());
    println!("        Sequential time: {:.1}s", 
        manager.nodes.values().map(|n| n.estimated_duration.as_secs()).sum::<u64>() as f64);
    println!("        Parallel time: {:.1}s", result.total_estimated_time.as_secs_f64());
    println!("        Time savings: {:.1}s ({:.1}%)", 
        manager.nodes.values().map(|n| n.estimated_duration.as_secs()).sum::<u64>() as f64 - result.total_estimated_time.as_secs_f64(),
        100.0 - (result.total_estimated_time.as_secs_f64() / manager.nodes.values().map(|n| n.estimated_duration.as_secs()).sum::<u64>() as f64 * 100.0));
    
    Ok(())
}

async fn demo_resource_conflict_management() -> Result<()> {
    println!("  🔄 Demonstrating resource conflict management...");
    
    let config = DependencyConfig::default();
    let mut manager = TestDependencyManager::new(config);
    
    // Register shared resources
    manager.register_resource(create_resource("test_database", "database", 1));
    manager.register_resource(create_resource("port_8080", "network_port", 1));
    manager.register_resource(create_resource("temp_directory", "filesystem", 1));
    manager.register_resource(create_resource("gpu_device", "hardware", 1));
    
    // Create tests that compete for resources
    let mut test_db_1 = create_test_node("db_integration_test_1", "Database Integration Test 1", "integration", Duration::from_secs(60));
    test_db_1.resource_requirements.push("test_database".to_string());
    
    let mut test_db_2 = create_test_node("db_integration_test_2", "Database Integration Test 2", "integration", Duration::from_secs(45));
    test_db_2.resource_requirements.push("test_database".to_string());
    
    let mut test_server_1 = create_test_node("server_test_1", "Server Test 1", "integration", Duration::from_secs(30));
    test_server_1.resource_requirements.push("port_8080".to_string());
    
    let mut test_server_2 = create_test_node("server_test_2", "Server Test 2", "integration", Duration::from_secs(35));
    test_server_2.resource_requirements.push("port_8080".to_string());
    
    let mut test_gpu = create_test_node("gpu_rendering_test", "GPU Rendering Test", "performance", Duration::from_secs(90));
    test_gpu.resource_requirements.push("gpu_device".to_string());
    
    // Add all tests
    manager.add_test_node(test_db_1)?;
    manager.add_test_node(test_db_2)?;
    manager.add_test_node(test_server_1)?;
    manager.add_test_node(test_server_2)?;
    manager.add_test_node(test_gpu)?;
    
    // Resolve dependencies and detect conflicts
    let result = manager.resolve_dependencies()?;
    
    println!("     🔍 Resource Conflict Analysis:");
    if result.resource_conflicts.is_empty() {
        println!("        ✅ No resource conflicts detected");
    } else {
        for (i, conflict) in result.resource_conflicts.iter().enumerate() {
            println!("        {}. Resource: {}", i + 1, conflict.resource_name);
            println!("           Conflicting tests: {}", conflict.conflicting_tests.join(", "));
            println!("           Resolution: {:?}", conflict.resolution_strategy);
        }
    }
    
    println!("     📈 Execution Plan with Resource Management:");
    for (i, batch) in result.execution_order.iter().enumerate() {
        println!("        Batch {}: {} (parallel: {})", 
            i + 1, 
            batch.join(", "),
            batch.len() > 1
        );
    }
    
    Ok(())
}

async fn demo_circular_dependency_detection() -> Result<()> {
    println!("  ⚠️  Demonstrating circular dependency detection...");
    
    let config = DependencyConfig {
        fail_on_circular_dependency: false, // Allow for demonstration
        ..Default::default()
    };
    let mut manager = TestDependencyManager::new(config);
    
    // Create tests with circular dependencies (intentionally problematic)
    let mut test_a = create_test_node("circular_test_a", "Circular Test A", "unit", Duration::from_secs(20));
    let mut test_b = create_test_node("circular_test_b", "Circular Test B", "unit", Duration::from_secs(25));
    let mut test_c = create_test_node("circular_test_c", "Circular Test C", "unit", Duration::from_secs(30));
    
    // Create circular dependency: A → B → C → A
    test_a.dependencies.push(create_dependency(
        "circular_test_a", "circular_test_c", DependencyType::Sequential,
        "A depends on C"
    ));
    
    test_b.dependencies.push(create_dependency(
        "circular_test_b", "circular_test_a", DependencyType::Sequential,
        "B depends on A"
    ));
    
    test_c.dependencies.push(create_dependency(
        "circular_test_c", "circular_test_b", DependencyType::Sequential,
        "C depends on B"
    ));
    
    manager.add_test_node(test_a)?;
    manager.add_test_node(test_b)?;
    manager.add_test_node(test_c)?;
    
    // Try to resolve dependencies
    match manager.resolve_dependencies() {
        Ok(result) => {
            println!("     ⚠️  Circular Dependencies Detected:");
            for (i, circular) in result.circular_dependencies.iter().enumerate() {
                println!("        {}. Cycle: {}", i + 1, circular.test_chain.join(" → "));
                println!("           Severity: {:?}", circular.severity);
                println!("           Resolution: {}", circular.suggested_resolution);
            }
        }
        Err(e) => {
            println!("     ❌ Dependency resolution failed (as expected): {}", e);
        }
    }
    
    // Show how to fix circular dependencies
    println!("     💡 Resolution Strategy:");
    println!("        - Break circular chain by removing unnecessary dependencies");
    println!("        - Use test fixtures or setup/teardown hooks");
    println!("        - Refactor tests to be more independent");
    println!("        - Consider using soft dependencies instead of hard dependencies");
    
    Ok(())
}

async fn demo_optimization_suggestions() -> Result<()> {
    println!("  💡 Generating optimization suggestions...");
    
    let config = DependencyConfig::default();
    let mut manager = TestDependencyManager::new(config);
    
    // Create a scenario with optimization opportunities
    let mut slow_setup = create_test_node("slow_setup", "Slow Setup Test", "setup", Duration::from_secs(120));
    slow_setup.can_run_in_parallel = false; // Unnecessarily sequential
    
    let mut over_dependent = create_test_node("over_dependent_test", "Over-dependent Test", "unit", Duration::from_secs(30));
    // Add many dependencies (some unnecessary)
    for i in 1..=5 {
        over_dependent.dependencies.push(create_dependency(
            "over_dependent_test",
            &format!("dependency_{}", i),
            DependencyType::Sequential,
            &format!("Dependency {}", i)
        ));
    }
    
    let mut fast_parallel = create_test_node("fast_parallel_test", "Fast Parallel Test", "unit", Duration::from_secs(10));
    fast_parallel.can_run_in_parallel = true;
    
    let mut resource_heavy = create_test_node("resource_heavy_test", "Resource Heavy Test", "integration", Duration::from_secs(60));
    resource_heavy.resource_requirements.push("exclusive_resource".to_string());
    
    manager.add_test_node(slow_setup)?;
    manager.add_test_node(over_dependent)?;
    manager.add_test_node(fast_parallel)?;
    manager.add_test_node(resource_heavy)?;
    
    // Register a constrained resource
    manager.register_resource(create_resource("exclusive_resource", "hardware", 1));
    
    let result = manager.resolve_dependencies()?;
    
    println!("     🎯 Optimization Opportunities Identified:");
    for (i, suggestion) in result.optimization_suggestions.iter().enumerate() {
        println!("        {}. {} (Type: {:?})", i + 1, suggestion.title, suggestion.suggestion_type);
        println!("           Description: {}", suggestion.description);
        println!("           Time Savings: {:.1}s", suggestion.estimated_time_savings.as_secs_f64());
        println!("           Effort: {}", suggestion.implementation_effort);
        println!("           Affected Tests: {}", suggestion.affected_tests.join(", "));
        println!();
    }
    
    // Calculate potential improvements
    let total_savings: Duration = result.optimization_suggestions.iter()
        .map(|s| s.estimated_time_savings)
        .sum();
    
    println!("     📈 Optimization Impact:");
    println!("        Potential time savings: {:.1}s", total_savings.as_secs_f64());
    println!("        Current execution time: {:.1}s", result.total_estimated_time.as_secs_f64());
    println!("        Optimized execution time: {:.1}s", 
        (result.total_estimated_time - total_savings).as_secs_f64());
    println!("        Performance improvement: {:.1}%", 
        (total_savings.as_secs_f64() / result.total_estimated_time.as_secs_f64()) * 100.0);
    
    Ok(())
}

async fn demo_real_world_scenario() -> Result<()> {
    println!("  🌍 Real-world Kryon UI framework test scenario...");
    
    let config = DependencyConfig {
        max_concurrent_tests: 6,
        enable_smart_ordering: true,
        ..Default::default()
    };
    let mut manager = TestDependencyManager::new(config);
    
    // Register realistic resources
    manager.register_resource(create_resource("test_database", "postgresql", 2));
    manager.register_resource(create_resource("redis_cache", "redis", 1));
    manager.register_resource(create_resource("test_server_port", "network", 4));
    manager.register_resource(create_resource("gpu_device", "cuda", 1));
    manager.register_resource(create_resource("file_system", "tmpfs", 8));
    
    // Create comprehensive test suite
    let test_definitions = vec![
        // Setup phase
        ("env_setup", "Environment Setup", "setup", 10, vec![], vec![], TestPriority::Critical),
        ("db_setup", "Database Setup", "setup", 15, vec!["env_setup"], vec!["test_database"], TestPriority::Critical),
        ("cache_setup", "Cache Setup", "setup", 8, vec!["env_setup"], vec!["redis_cache"], TestPriority::High),
        
        // Core unit tests
        ("lexer_unit", "Lexer Unit Tests", "unit", 20, vec!["env_setup"], vec!["file_system"], TestPriority::High),
        ("parser_unit", "Parser Unit Tests", "unit", 25, vec!["lexer_unit"], vec!["file_system"], TestPriority::High),
        ("compiler_unit", "Compiler Unit Tests", "unit", 30, vec!["parser_unit"], vec!["file_system"], TestPriority::High),
        ("runtime_unit", "Runtime Unit Tests", "unit", 18, vec!["env_setup"], vec!["file_system"], TestPriority::High),
        
        // Integration tests
        ("db_integration", "Database Integration", "integration", 45, vec!["db_setup", "runtime_unit"], vec!["test_database"], TestPriority::Medium),
        ("cache_integration", "Cache Integration", "integration", 35, vec!["cache_setup", "runtime_unit"], vec!["redis_cache"], TestPriority::Medium),
        ("api_integration", "API Integration", "integration", 40, vec!["db_integration"], vec!["test_server_port", "test_database"], TestPriority::Medium),
        
        // Rendering tests
        ("layout_tests", "Layout Engine Tests", "rendering", 50, vec!["compiler_unit"], vec!["file_system"], TestPriority::Medium),
        ("gpu_rendering", "GPU Rendering Tests", "rendering", 60, vec!["layout_tests"], vec!["gpu_device"], TestPriority::Low),
        ("cpu_rendering", "CPU Rendering Tests", "rendering", 45, vec!["layout_tests"], vec!["file_system"], TestPriority::Medium),
        
        // Visual tests
        ("visual_regression", "Visual Regression", "visual", 90, vec!["gpu_rendering", "cpu_rendering"], vec!["file_system"], TestPriority::Low),
        ("screenshot_tests", "Screenshot Tests", "visual", 70, vec!["visual_regression"], vec!["file_system"], TestPriority::Low),
        
        // Performance tests
        ("performance_cpu", "CPU Performance", "performance", 120, vec!["cpu_rendering"], vec!["file_system"], TestPriority::Low),
        ("performance_gpu", "GPU Performance", "performance", 180, vec!["gpu_rendering"], vec!["gpu_device"], TestPriority::Low),
        ("memory_profiling", "Memory Profiling", "performance", 90, vec!["runtime_unit"], vec!["file_system"], TestPriority::Low),
        
        // End-to-end tests
        ("e2e_basic", "Basic E2E Tests", "e2e", 200, vec!["api_integration", "screenshot_tests"], vec!["test_server_port", "test_database", "file_system"], TestPriority::Low),
        ("e2e_advanced", "Advanced E2E Tests", "e2e", 300, vec!["e2e_basic", "performance_cpu"], vec!["test_server_port", "test_database", "file_system"], TestPriority::Low),
    ];
    
    // Create and add test nodes
    for (test_id, test_name, category, duration, deps, resources, priority) in test_definitions {
        let mut node = create_test_node(test_id, test_name, category, Duration::from_secs(duration));
        
        // Add dependencies
        for dep in deps {
            node.dependencies.push(create_dependency(
                test_id, dep, DependencyType::Sequential,
                &format!("{} requires {}", test_name, dep)
            ));
        }
        
        // Add resource requirements
        node.resource_requirements = resources.into_iter().map(String::from).collect();
        node.priority = priority;
        
        // Set parallel capability
        node.can_run_in_parallel = match category {
            "setup" => false,
            "e2e" => false,
            _ => true,
        };
        
        manager.add_test_node(node)?;
    }
    
    // Resolve the complex real-world scenario
    let result = manager.resolve_dependencies()?;
    
    println!("     🎯 Real-world Scenario Analysis:");
    manager.print_dependency_summary(&result);
    
    println!("\n     📊 Detailed Execution Plan:");
    let mut total_batch_time = Duration::ZERO;
    for (i, batch) in result.execution_order.iter().enumerate() {
        let batch_max_time = batch.iter()
            .filter_map(|test_id| manager.nodes.get(test_id))
            .map(|node| node.estimated_duration)
            .max()
            .unwrap_or(Duration::ZERO);
        
        total_batch_time += batch_max_time;
        
        println!("        Batch {} ({:.1}s): {} tests", 
            i + 1, 
            batch_max_time.as_secs_f64(),
            batch.len()
        );
        
        for test_id in batch {
            if let Some(node) = manager.nodes.get(test_id) {
                let resources = if node.resource_requirements.is_empty() {
                    "none".to_string()
                } else {
                    node.resource_requirements.join(", ")
                };
                
                println!("          - {} ({:.1}s, resources: {})", 
                    node.test_name, 
                    node.estimated_duration.as_secs_f64(),
                    resources
                );
            }
        }
        println!();
    }
    
    // Calculate efficiency metrics
    let sequential_time: Duration = manager.nodes.values()
        .map(|node| node.estimated_duration)
        .sum();
    
    println!("     📈 Performance Metrics:");
    println!("        Total tests: {}", manager.nodes.len());
    println!("        Sequential execution time: {:.1}s ({:.1} minutes)", 
        sequential_time.as_secs_f64(),
        sequential_time.as_secs_f64() / 60.0
    );
    println!("        Parallel execution time: {:.1}s ({:.1} minutes)", 
        result.total_estimated_time.as_secs_f64(),
        result.total_estimated_time.as_secs_f64() / 60.0
    );
    println!("        Time savings: {:.1}s ({:.1} minutes)", 
        sequential_time.as_secs_f64() - result.total_estimated_time.as_secs_f64(),
        (sequential_time.as_secs_f64() - result.total_estimated_time.as_secs_f64()) / 60.0
    );
    println!("        Efficiency improvement: {:.1}%", result.parallel_efficiency);
    println!("        Average batch size: {:.1} tests", 
        manager.nodes.len() as f64 / result.execution_order.len() as f64
    );
    
    Ok(())
}

#[cfg(test)]
mod dependency_demo_tests {
    use super::*;
    
    #[tokio::test]
    async fn test_dependency_manager_creation() {
        let config = DependencyConfig::default();
        let manager = TestDependencyManager::new(config);
        
        assert!(manager.nodes.is_empty());
        assert!(manager.global_dependencies.is_empty());
        assert!(manager.resource_pool.is_empty());
    }
    
    #[tokio::test]
    async fn test_basic_dependency_resolution() {
        let config = DependencyConfig::default();
        let mut manager = TestDependencyManager::new(config);
        
        // Create simple dependency chain: A → B → C
        let test_a = create_test_node("test_a", "Test A", "unit", Duration::from_secs(10));
        let mut test_b = create_test_node("test_b", "Test B", "unit", Duration::from_secs(20));
        let mut test_c = create_test_node("test_c", "Test C", "unit", Duration::from_secs(15));
        
        test_b.dependencies.push(create_dependency("test_b", "test_a", DependencyType::Sequential, "B needs A"));
        test_c.dependencies.push(create_dependency("test_c", "test_b", DependencyType::Sequential, "C needs B"));
        
        manager.add_test_node(test_a).unwrap();
        manager.add_test_node(test_b).unwrap();
        manager.add_test_node(test_c).unwrap();
        
        let result = manager.resolve_dependencies().unwrap();
        
        assert_eq!(result.execution_order.len(), 3); // Should be 3 sequential batches
        assert_eq!(result.execution_order[0], vec!["test_a"]);
        assert_eq!(result.execution_order[1], vec!["test_b"]);
        assert_eq!(result.execution_order[2], vec!["test_c"]);
    }
    
    #[tokio::test]
    async fn test_parallel_execution_detection() {
        let config = DependencyConfig::default();
        let mut manager = TestDependencyManager::new(config);
        
        // Create tests that can run in parallel
        let test_a = create_test_node("test_a", "Test A", "unit", Duration::from_secs(10));
        let test_b = create_test_node("test_b", "Test B", "unit", Duration::from_secs(20));
        let test_c = create_test_node("test_c", "Test C", "unit", Duration::from_secs(15));
        
        manager.add_test_node(test_a).unwrap();
        manager.add_test_node(test_b).unwrap();
        manager.add_test_node(test_c).unwrap();
        
        let result = manager.resolve_dependencies().unwrap();
        
        assert_eq!(result.execution_order.len(), 1); // All can run in parallel
        assert_eq!(result.execution_order[0].len(), 3);
    }
    
    #[tokio::test]
    async fn test_resource_conflict_detection() {
        let config = DependencyConfig::default();
        let mut manager = TestDependencyManager::new(config);
        
        // Register a constrained resource
        manager.register_resource(create_resource("database", "postgres", 1));
        
        let mut test_a = create_test_node("test_a", "Test A", "integration", Duration::from_secs(30));
        test_a.resource_requirements.push("database".to_string());
        
        let mut test_b = create_test_node("test_b", "Test B", "integration", Duration::from_secs(40));
        test_b.resource_requirements.push("database".to_string());
        
        manager.add_test_node(test_a).unwrap();
        manager.add_test_node(test_b).unwrap();
        
        let result = manager.resolve_dependencies().unwrap();
        
        assert!(!result.resource_conflicts.is_empty());
        assert_eq!(result.resource_conflicts[0].conflicting_tests.len(), 2);
    }
}