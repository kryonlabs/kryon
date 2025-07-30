//! Complete Test Orchestration Demo
//! 
//! This example demonstrates the full power of the Kryon testing infrastructure,
//! combining dependency management, coverage analysis, metrics collection, and
//! intelligent test orchestration in a comprehensive demonstration.

use kryon_tests::prelude::*;
use std::time::Duration;

#[tokio::main]
async fn main() -> Result<()> {
    setup_test_environment();
    
    println!("🎭 Kryon Complete Test Orchestration Demo");
    println!("==========================================");
    
    // Demo 1: Basic Orchestration Setup
    println!("\n1. 🏗️  Setting up Test Orchestration");
    demo_orchestration_setup().await?;
    
    // Demo 2: Small-Scale Orchestration
    println!("\n2. 🔬 Small-Scale Orchestration");
    demo_small_scale_orchestration().await?;
    
    // Demo 3: Enterprise-Scale Orchestration
    println!("\n3. 🏢 Enterprise-Scale Orchestration");
    demo_enterprise_scale_orchestration().await?;
    
    // Demo 4: Real-World Kryon Framework Test Suite
    println!("\n4. 🌍 Real-World Kryon Framework Test Suite");
    demo_real_world_kryon_suite().await?;
    
    println!("\n🎉 Complete Orchestration Demo Finished!");
    println!("\nDemonstrated Capabilities:");
    println!("  ✅ Intelligent test dependency resolution");
    println!("  ✅ Parallel execution optimization");
    println!("  ✅ Real-time coverage analysis");
    println!("  ✅ Comprehensive metrics collection");
    println!("  ✅ Resource conflict management");
    println!("  ✅ Performance insights and recommendations");
    println!("  ✅ Executive reporting and dashboards");
    println!("  ✅ Enterprise-grade test orchestration");
    
    Ok(())
}

async fn demo_orchestration_setup() -> Result<()> {
    println!("  🔧 Configuring orchestration system...");
    
    let config = OrchestratorConfig {
        enable_dependency_management: true,
        enable_coverage_collection: true,
        enable_metrics_collection: true,
        enable_parallel_execution: true,
        max_concurrent_tests: 4,
        test_timeout_seconds: 120,
        retry_failed_tests: true,
        max_retries: 2,
        output_directory: "target/orchestration-demo".to_string(),
        generate_reports: true,
        fail_fast: false,
    };
    
    let orchestrator = TestOrchestrator::new(config);
    
    println!("     ✅ Orchestration system configured:");
    println!("        Session ID: {}", orchestrator.session_id);
    println!("        Max concurrent tests: {}", orchestrator.config.max_concurrent_tests);
    println!("        Dependency management: {}", orchestrator.config.enable_dependency_management);
    println!("        Coverage collection: {}", orchestrator.config.enable_coverage_collection);
    println!("        Metrics collection: {}", orchestrator.config.enable_metrics_collection);
    println!("        Output directory: {}", orchestrator.config.output_directory);
    
    Ok(())
}

async fn demo_small_scale_orchestration() -> Result<()> {
    println!("  🔬 Running small-scale orchestration demo...");
    
    let config = OrchestratorConfig::default();
    let mut orchestrator = TestOrchestrator::new(config);
    
    // Create a simple test suite
    let tests = vec![
        ("setup_test", "Setup Test", "setup", 5, vec![], TestPriority::Critical),
        ("unit_test_1", "Unit Test 1", "unit", 10, vec!["setup_test"], TestPriority::High),
        ("unit_test_2", "Unit Test 2", "unit", 12, vec!["setup_test"], TestPriority::High),
        ("integration_test", "Integration Test", "integration", 20, vec!["unit_test_1", "unit_test_2"], TestPriority::Medium),
        ("cleanup_test", "Cleanup Test", "cleanup", 3, vec!["integration_test"], TestPriority::Low),
    ];
    
    // Add tests to orchestrator
    for (test_id, test_name, category, duration, deps, priority) in tests {
        let mut node = create_test_node(test_id, test_name, category, Duration::from_secs(duration));
        
        for dep in deps {
            node.dependencies.push(create_dependency(
                test_id, dep, DependencyType::Sequential,
                &format!("{} requires {}", test_name, dep)
            ));
        }
        
        node.priority = priority;
        node.can_run_in_parallel = !matches!(category, "setup" | "cleanup");
        
        orchestrator.add_test(node)?;
    }
    
    // Execute orchestration
    let result = orchestrator.execute_orchestration().await?;
    
    println!("     📊 Small-Scale Results:");
    println!("        Tests executed: {}", result.total_tests);
    println!("        Success rate: {:.1}%", 
        (result.successful_tests as f64 / result.total_tests as f64) * 100.0);
    println!("        Total time: {:.1}s", result.total_execution_time.as_secs_f64());
    println!("        Parallel efficiency: {:.1}%", result.parallel_efficiency);
    println!("        Insights generated: {}", result.performance_insights.len());
    println!("        Recommendations: {}", result.recommendations.len());
    
    Ok(())
}

async fn demo_enterprise_scale_orchestration() -> Result<()> {
    println!("  🏢 Running enterprise-scale orchestration demo...");
    
    let config = OrchestratorConfig {
        max_concurrent_tests: 8,
        test_timeout_seconds: 600,
        output_directory: "target/enterprise-orchestration".to_string(),
        ..Default::default()
    };
    
    let mut orchestrator = TestOrchestrator::new(config);
    
    // Create an enterprise-scale test suite
    let enterprise_tests = vec![
        // Infrastructure setup
        ("infra_setup", "Infrastructure Setup", "infrastructure", 30, vec![], TestPriority::Critical),
        ("db_setup", "Database Setup", "infrastructure", 45, vec!["infra_setup"], TestPriority::Critical),
        ("cache_setup", "Cache Setup", "infrastructure", 25, vec!["infra_setup"], TestPriority::High),
        ("message_queue_setup", "Message Queue Setup", "infrastructure", 20, vec!["infra_setup"], TestPriority::High),
        
        // Core service tests
        ("auth_service_tests", "Authentication Service Tests", "service", 60, vec!["db_setup"], TestPriority::Critical),
        ("user_service_tests", "User Service Tests", "service", 50, vec!["auth_service_tests"], TestPriority::High),
        ("billing_service_tests", "Billing Service Tests", "service", 40, vec!["db_setup"], TestPriority::High),
        ("notification_service_tests", "Notification Service Tests", "service", 35, vec!["message_queue_setup"], TestPriority::Medium),
        
        // API layer tests
        ("rest_api_tests", "REST API Tests", "api", 80, vec!["user_service_tests", "billing_service_tests"], TestPriority::High),
        ("graphql_api_tests", "GraphQL API Tests", "api", 70, vec!["user_service_tests"], TestPriority::Medium),
        ("websocket_api_tests", "WebSocket API Tests", "api", 45, vec!["notification_service_tests"], TestPriority::Medium),
        
        // Frontend component tests
        ("ui_component_tests", "UI Component Tests", "frontend", 90, vec!["rest_api_tests"], TestPriority::Medium),
        ("state_management_tests", "State Management Tests", "frontend", 40, vec!["graphql_api_tests"], TestPriority::Medium),
        ("routing_tests", "Routing Tests", "frontend", 30, vec!["ui_component_tests"], TestPriority::Low),
        
        // Integration tests
        ("api_integration_tests", "API Integration Tests", "integration", 120, vec!["rest_api_tests", "graphql_api_tests"], TestPriority::High),
        ("frontend_integration_tests", "Frontend Integration Tests", "integration", 100, vec!["ui_component_tests", "state_management_tests"], TestPriority::Medium),
        ("cross_service_integration", "Cross-Service Integration", "integration", 150, vec!["api_integration_tests"], TestPriority::High),
        
        // Performance tests
        ("load_testing", "Load Testing", "performance", 300, vec!["cross_service_integration"], TestPriority::Medium),
        ("stress_testing", "Stress Testing", "performance", 400, vec!["load_testing"], TestPriority::Low),
        ("endurance_testing", "Endurance Testing", "performance", 600, vec!["stress_testing"], TestPriority::Low),
        
        // Security tests
        ("auth_security_tests", "Authentication Security Tests", "security", 80, vec!["auth_service_tests"], TestPriority::Critical),
        ("api_security_tests", "API Security Tests", "security", 90, vec!["rest_api_tests"], TestPriority::High),
        ("data_security_tests", "Data Security Tests", "security", 70, vec!["db_setup"], TestPriority::High),
        
        // End-to-end tests
        ("user_journey_tests", "User Journey Tests", "e2e", 200, vec!["frontend_integration_tests", "api_security_tests"], TestPriority::Medium),
        ("admin_workflow_tests", "Admin Workflow Tests", "e2e", 180, vec!["user_journey_tests"], TestPriority::Medium),
        ("business_process_tests", "Business Process Tests", "e2e", 250, vec!["admin_workflow_tests"], TestPriority::Low),
        
        // Cleanup and reporting
        ("performance_analysis", "Performance Analysis", "analysis", 40, vec!["endurance_testing"], TestPriority::Low),
        ("security_analysis", "Security Analysis", "analysis", 30, vec!["data_security_tests"], TestPriority::Medium),
        ("final_cleanup", "Final Cleanup", "cleanup", 15, vec!["business_process_tests", "performance_analysis", "security_analysis"], TestPriority::Low),
    ];
    
    // Add all enterprise tests
    for (test_id, test_name, category, duration, deps, priority) in enterprise_tests {
        let mut node = create_test_node(test_id, test_name, category, Duration::from_secs(duration));
        
        for dep in deps {
            node.dependencies.push(create_dependency(
                test_id, dep, DependencyType::Sequential,
                &format!("{} requires {}", test_name, dep)
            ));
        }
        
        node.priority = priority;
        
        // Configure parallelization based on category
        node.can_run_in_parallel = match category {
            "infrastructure" | "cleanup" | "analysis" => false,
            "e2e" => false,
            _ => true,
        };
        
        // Add resource requirements
        match category {
            "infrastructure" => {
                node.resource_requirements.push("infrastructure_resources".to_string());
            }
            "service" | "api" => {
                node.resource_requirements.push("service_resources".to_string());
            }
            "performance" => {
                node.resource_requirements.push("performance_resources".to_string());
            }
            "security" => {
                node.resource_requirements.push("security_tools".to_string());
            }
            _ => {}
        }
        
        orchestrator.add_test(node)?;
    }
    
    // Execute enterprise orchestration
    println!("     🚀 Executing enterprise-scale test orchestration...");
    let result = orchestrator.execute_orchestration().await?;
    
    println!("     📊 Enterprise-Scale Results:");
    println!("        Total tests: {}", result.total_tests);
    println!("        Successful: {} ({:.1}%)", 
        result.successful_tests,
        (result.successful_tests as f64 / result.total_tests as f64) * 100.0
    );
    println!("        Failed: {}", result.failed_tests);
    println!("        Total execution time: {:.1}s ({:.1}m)", 
        result.total_execution_time.as_secs_f64(),
        result.total_execution_time.as_secs_f64() / 60.0
    );
    println!("        Parallel efficiency: {:.1}%", result.parallel_efficiency);
    
    // Show dependency analysis
    if let Some(dep_result) = &result.dependency_resolution {
        println!("        Execution batches: {}", dep_result.execution_order.len());
        println!("        Optimization opportunities: {}", dep_result.optimization_suggestions.len());
        
        if !dep_result.circular_dependencies.is_empty() {
            println!("        ⚠️  Circular dependencies: {}", dep_result.circular_dependencies.len());
        }
        if !dep_result.resource_conflicts.is_empty() {
            println!("        🔄 Resource conflicts: {}", dep_result.resource_conflicts.len());
        }
    }
    
    // Show performance insights
    if !result.performance_insights.is_empty() {
        println!("        💡 Performance insights: {}", result.performance_insights.len());
        let critical_insights = result.performance_insights.iter()
            .filter(|i| matches!(i.impact_level, InsightImpact::Critical))
            .count();
        if critical_insights > 0 {
            println!("           Critical issues: {}", critical_insights);
        }
    }
    
    // Show recommendations
    if !result.recommendations.is_empty() {
        println!("        🎯 Recommendations generated: {}", result.recommendations.len());
        let high_priority = result.recommendations.iter()
            .filter(|r| matches!(r.priority, RecommendationPriority::Critical | RecommendationPriority::High))
            .count();
        if high_priority > 0 {
            println!("           High priority actions: {}", high_priority);
        }
    }
    
    Ok(())
}

async fn demo_real_world_kryon_suite() -> Result<()> {
    println!("  🌍 Simulating real-world Kryon UI framework test suite...");
    
    let config = OrchestratorConfig {
        max_concurrent_tests: 6,
        test_timeout_seconds: 300,
        output_directory: "target/kryon-real-world".to_string(),
        retry_failed_tests: true,
        max_retries: 1,
        ..Default::default()
    };
    
    let mut orchestrator = TestOrchestrator::new(config);
    
    // Real-world Kryon framework test suite
    let kryon_tests = vec![
        // Compiler pipeline tests
        ("lexer_tests", "Lexer Tests", "compiler", 25, vec![], TestPriority::Critical),
        ("parser_tests", "Parser Tests", "compiler", 35, vec!["lexer_tests"], TestPriority::Critical),
        ("ast_builder_tests", "AST Builder Tests", "compiler", 30, vec!["parser_tests"], TestPriority::Critical),
        ("semantic_analyzer_tests", "Semantic Analyzer Tests", "compiler", 40, vec!["ast_builder_tests"], TestPriority::Critical),
        ("type_checker_tests", "Type Checker Tests", "compiler", 45, vec!["semantic_analyzer_tests"], TestPriority::High),
        ("code_generator_tests", "Code Generator Tests", "compiler", 50, vec!["type_checker_tests"], TestPriority::High),
        ("optimizer_tests", "Optimizer Tests", "compiler", 35, vec!["code_generator_tests"], TestPriority::Medium),
        
        // Runtime system tests
        ("runtime_core_tests", "Runtime Core Tests", "runtime", 40, vec![], TestPriority::Critical),
        ("memory_manager_tests", "Memory Manager Tests", "runtime", 35, vec!["runtime_core_tests"], TestPriority::High),
        ("event_system_tests", "Event System Tests", "runtime", 30, vec!["runtime_core_tests"], TestPriority::High),
        ("component_lifecycle_tests", "Component Lifecycle Tests", "runtime", 25, vec!["event_system_tests"], TestPriority::Medium),
        
        // Layout engine tests
        ("flexbox_tests", "Flexbox Layout Tests", "layout", 45, vec!["runtime_core_tests"], TestPriority::High),
        ("grid_tests", "Grid Layout Tests", "layout", 40, vec!["flexbox_tests"], TestPriority::Medium),
        ("constraint_solver_tests", "Constraint Solver Tests", "layout", 35, vec!["grid_tests"], TestPriority::Medium),
        ("responsive_layout_tests", "Responsive Layout Tests", "layout", 50, vec!["constraint_solver_tests"], TestPriority::Medium),
        
        // Rendering engine tests
        ("ratatui_renderer_tests", "Ratatui Renderer Tests", "rendering", 60, vec!["flexbox_tests", "code_generator_tests"], TestPriority::High),
        ("web_renderer_tests", "Web Renderer Tests", "rendering", 70, vec!["grid_tests", "code_generator_tests"], TestPriority::Medium),
        ("desktop_renderer_tests", "Desktop Renderer Tests", "rendering", 80, vec!["responsive_layout_tests", "code_generator_tests"], TestPriority::Medium),
        ("mobile_renderer_tests", "Mobile Renderer Tests", "rendering", 75, vec!["responsive_layout_tests", "code_generator_tests"], TestPriority::Low),
        
        // Component library tests
        ("basic_components_tests", "Basic Components Tests", "components", 40, vec!["component_lifecycle_tests"], TestPriority::High),
        ("form_components_tests", "Form Components Tests", "components", 50, vec!["basic_components_tests"], TestPriority::Medium),
        ("navigation_components_tests", "Navigation Components Tests", "components", 35, vec!["basic_components_tests"], TestPriority::Medium),
        ("data_display_tests", "Data Display Tests", "components", 45, vec!["form_components_tests"], TestPriority::Medium),
        
        // Integration tests
        ("compiler_runtime_integration", "Compiler-Runtime Integration", "integration", 80, vec!["optimizer_tests", "memory_manager_tests"], TestPriority::High),
        ("layout_rendering_integration", "Layout-Rendering Integration", "integration", 90, vec!["ratatui_renderer_tests", "web_renderer_tests"], TestPriority::High),
        ("components_rendering_integration", "Components-Rendering Integration", "integration", 70, vec!["data_display_tests", "desktop_renderer_tests"], TestPriority::Medium),
        ("cross_platform_integration", "Cross-Platform Integration", "integration", 120, vec!["layout_rendering_integration", "mobile_renderer_tests"], TestPriority::Medium),
        
        // Performance and stress tests
        ("compiler_performance", "Compiler Performance Tests", "performance", 150, vec!["compiler_runtime_integration"], TestPriority::Medium),
        ("rendering_performance", "Rendering Performance Tests", "performance", 180, vec!["components_rendering_integration"], TestPriority::Medium),
        ("memory_stress_tests", "Memory Stress Tests", "performance", 200, vec!["cross_platform_integration"], TestPriority::Low),
        ("concurrent_rendering_tests", "Concurrent Rendering Tests", "performance", 160, vec!["rendering_performance"], TestPriority::Low),
        
        // End-to-end application tests
        ("todo_app_e2e", "Todo App E2E Tests", "e2e", 120, vec!["cross_platform_integration"], TestPriority::Medium),
        ("dashboard_app_e2e", "Dashboard App E2E Tests", "e2e", 140, vec!["todo_app_e2e"], TestPriority::Medium),
        ("game_ui_e2e", "Game UI E2E Tests", "e2e", 100, vec!["concurrent_rendering_tests"], TestPriority::Low),
        ("editor_e2e", "Code Editor E2E Tests", "e2e", 180, vec!["dashboard_app_e2e"], TestPriority::Low),
        
        // Documentation and example tests
        ("documentation_tests", "Documentation Tests", "docs", 30, vec!["compiler_performance"], TestPriority::Low),
        ("example_apps_tests", "Example Apps Tests", "docs", 60, vec!["game_ui_e2e"], TestPriority::Low),
        ("tutorial_tests", "Tutorial Tests", "docs", 40, vec!["editor_e2e"], TestPriority::Low),
        
        // Final validation
        ("regression_test_suite", "Regression Test Suite", "validation", 90, vec!["documentation_tests", "example_apps_tests", "tutorial_tests"], TestPriority::High),
    ];
    
    // Add all Kryon tests
    for (test_id, test_name, category, duration, deps, priority) in kryon_tests {
        let mut node = create_test_node(test_id, test_name, category, Duration::from_secs(duration));
        
        for dep in deps {
            node.dependencies.push(create_dependency(
                test_id, dep, DependencyType::Sequential,
                &format!("{} requires {}", test_name, dep)
            ));
        }
        
        node.priority = priority;
        
        // Configure parallelization and resources based on category
        match category {
            "compiler" => {
                node.can_run_in_parallel = true;
                node.resource_requirements.push("cpu_intensive".to_string());
            }
            "runtime" => {
                node.can_run_in_parallel = true;
                node.resource_requirements.push("memory_testing".to_string());
            }
            "layout" | "rendering" => {
                node.can_run_in_parallel = true;
                node.resource_requirements.push("graphics_resources".to_string());
            }
            "components" => {
                node.can_run_in_parallel = true;
            }
            "integration" => {
                node.can_run_in_parallel = false; // Integration tests need isolation
                node.resource_requirements.push("full_system".to_string());
            }
            "performance" => {
                node.can_run_in_parallel = false; // Performance tests need dedicated resources
                node.resource_requirements.push("performance_testing".to_string());
            }
            "e2e" => {
                node.can_run_in_parallel = false; // E2E tests need full system
                node.resource_requirements.push("full_system".to_string());
            }
            "docs" | "validation" => {
                node.can_run_in_parallel = true;
            }
            _ => {
                node.can_run_in_parallel = true;
            }
        }
        
        orchestrator.add_test(node)?;
    }
    
    // Execute the comprehensive Kryon test suite
    println!("     🚀 Executing comprehensive Kryon UI framework test suite...");
    let start_time = std::time::Instant::now();
    let result = orchestrator.execute_orchestration().await?;
    let total_orchestration_time = start_time.elapsed();
    
    println!("     🎯 Kryon Real-World Test Suite Results:");
    println!("        ═══════════════════════════════════════");
    println!("        Total tests: {}", result.total_tests);
    println!("        Successful: {} ({:.1}%)", 
        result.successful_tests,
        (result.successful_tests as f64 / result.total_tests as f64) * 100.0
    );
    println!("        Failed: {}", result.failed_tests);
    println!("        Test execution time: {:.1}s ({:.1}m)", 
        result.total_execution_time.as_secs_f64(),
        result.total_execution_time.as_secs_f64() / 60.0
    );
    println!("        Total orchestration time: {:.1}s ({:.1}m)", 
        total_orchestration_time.as_secs_f64(),
        total_orchestration_time.as_secs_f64() / 60.0
    );
    println!("        Parallel efficiency: {:.1}%", result.parallel_efficiency);
    
    // Detailed analysis
    if let Some(dep_result) = &result.dependency_resolution {
        println!("\n     📊 Dependency Analysis:");
        println!("        Execution batches: {}", dep_result.execution_order.len());
        println!("        Average batch size: {:.1} tests", 
            result.total_tests as f64 / dep_result.execution_order.len() as f64);
        
        // Calculate sequential vs parallel time savings
        let sequential_time = dep_result.execution_order.iter()
            .flat_map(|batch| batch.iter())
            .count() as f64 * 60.0; // Assume average 60s per test if sequential
        
        let time_saved = sequential_time - result.total_execution_time.as_secs_f64();
        println!("        Time saved by orchestration: {:.1}s ({:.1}m)", time_saved, time_saved / 60.0);
        
        if !dep_result.optimization_suggestions.is_empty() {
            println!("        Optimization opportunities: {}", dep_result.optimization_suggestions.len());
            let potential_savings: Duration = dep_result.optimization_suggestions.iter()
                .map(|s| s.estimated_time_savings)
                .sum();
            println!("        Potential additional savings: {:.1}s", potential_savings.as_secs_f64());
        }
    }
    
    // Coverage analysis
    if let Some(coverage) = &result.coverage_summary {
        println!("\n     📊 Coverage Analysis:");
        println!("        Line coverage: {:.1}% ({}/{})", 
            coverage.line_coverage_percentage,
            coverage.covered_lines,
            coverage.total_lines
        );
        println!("        Branch coverage: {:.1}% ({}/{})", 
            coverage.branch_coverage_percentage,
            coverage.covered_branches,
            coverage.total_branches
        );
        println!("        Function coverage: {:.1}% ({}/{})", 
            coverage.function_coverage_percentage,
            coverage.covered_functions,
            coverage.total_functions
        );
        println!("        Quality score: {:.1}/100", coverage.test_quality_score);
    }
    
    // Performance insights
    if !result.performance_insights.is_empty() {
        println!("\n     💡 Key Performance Insights:");
        for (i, insight) in result.performance_insights.iter().take(3).enumerate() {
            let impact_icon = match insight.impact_level {
                InsightImpact::Critical => "🔴",
                InsightImpact::High => "🟠",
                InsightImpact::Medium => "🟡", 
                InsightImpact::Low => "🟢",
            };
            println!("        {} {}. {} (Impact: {:?})", 
                impact_icon, i + 1, insight.title, insight.impact_level);
        }
    }
    
    // Top recommendations
    if !result.recommendations.is_empty() {
        println!("\n     🎯 Top Recommendations:");
        for (i, rec) in result.recommendations.iter().take(3).enumerate() {
            let priority_icon = match rec.priority {
                RecommendationPriority::Critical => "🔴",
                RecommendationPriority::High => "🟠",
                RecommendationPriority::Medium => "🟡",
                RecommendationPriority::Low => "🟢",
                RecommendationPriority::Info => "ℹ️",
            };
            println!("        {} {}. {} (Priority: {:?})", 
                priority_icon, i + 1, rec.title, rec.priority);
            println!("           Improvement: {}", rec.estimated_improvement);
        }
    }
    
    println!("\n     📁 Generated Reports:");
    println!("        Dashboard: target/kryon-real-world/orchestration_dashboard.html");
    println!("        Executive Summary: target/kryon-real-world/executive_summary.md");
    println!("        JSON Report: target/kryon-real-world/orchestration_report.json");
    
    Ok(())
}

#[cfg(test)]
mod complete_orchestration_tests {
    use super::*;
    
    #[tokio::test]
    async fn test_orchestrator_initialization() {
        let config = OrchestratorConfig::default();
        let orchestrator = TestOrchestrator::new(config);
        
        assert!(!orchestrator.session_id.is_empty());
        assert!(orchestrator.dependency_manager.is_some());
        assert!(orchestrator.coverage_orchestrator.is_some());
        assert!(orchestrator.metrics_collector.is_some());
    }
    
    #[tokio::test]
    async fn test_simple_orchestration_execution() {
        let config = OrchestratorConfig {
            max_concurrent_tests: 2,
            test_timeout_seconds: 10,
            ..Default::default()
        };
        let mut orchestrator = TestOrchestrator::new(config);
        
        // Add simple tests
        let test_a = create_test_node("test_a", "Test A", "unit", Duration::from_secs(1));
        let test_b = create_test_node("test_b", "Test B", "unit", Duration::from_secs(1));
        
        orchestrator.add_test(test_a).unwrap();
        orchestrator.add_test(test_b).unwrap();
        
        let result = orchestrator.execute_orchestration().await.unwrap();
        
        assert_eq!(result.total_tests, 2);
        assert!(result.total_execution_time.as_secs() < 10); // Should complete quickly
    }
    
    #[tokio::test] 
    async fn test_dependency_chain_orchestration() {
        let config = OrchestratorConfig::default();
        let mut orchestrator = TestOrchestrator::new(config);
        
        // Create dependency chain: A → B → C
        let test_a = create_test_node("test_a", "Test A", "unit", Duration::from_secs(1));
        let mut test_b = create_test_node("test_b", "Test B", "unit", Duration::from_secs(1));
        let mut test_c = create_test_node("test_c", "Test C", "unit", Duration::from_secs(1));
        
        test_b.dependencies.push(create_dependency("test_b", "test_a", DependencyType::Sequential, "B needs A"));
        test_c.dependencies.push(create_dependency("test_c", "test_b", DependencyType::Sequential, "C needs B"));
        
        orchestrator.add_test(test_a).unwrap();
        orchestrator.add_test(test_b).unwrap();
        orchestrator.add_test(test_c).unwrap();
        
        let result = orchestrator.execute_orchestration().await.unwrap();
        
        assert_eq!(result.total_tests, 3);
        
        // Should have dependency resolution
        assert!(result.dependency_resolution.is_some());
        let dep_result = result.dependency_resolution.unwrap();
        assert_eq!(dep_result.execution_order.len(), 3); // Should be 3 sequential batches
    }
}