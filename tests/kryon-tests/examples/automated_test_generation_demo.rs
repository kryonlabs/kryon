//! Automated Test Generation Demo
//! 
//! This example demonstrates the comprehensive automated test generation system,
//! showing how to create comprehensive test suites from formal specifications,
//! documentation, and code analysis.

use kryon_tests::prelude::*;
use std::time::Duration;
use std::collections::HashMap;

#[tokio::main]
async fn main() -> Result<()> {
    setup_test_environment();
    
    println!("🤖 Kryon Automated Test Generation Demo");
    println!("=======================================");
    
    // Demo 1: Basic Specification-Based Test Generation
    println!("\n1. 📋 Basic Specification-Based Test Generation");
    demo_basic_spec_generation().await?;
    
    // Demo 2: Complex API Specification Testing
    println!("\n2. 🔌 Complex API Specification Testing");
    demo_api_spec_generation().await?;
    
    // Demo 3: Performance Specification Testing
    println!("\n3. ⚡ Performance Specification Testing");
    demo_performance_spec_generation().await?;
    
    // Demo 4: Error Condition and Edge Case Generation
    println!("\n4. 🚨 Error Condition and Edge Case Generation");
    demo_error_edge_case_generation().await?;
    
    // Demo 5: Property-Based Test Generation
    println!("\n5. 🎲 Property-Based Test Generation");
    demo_property_based_generation().await?;
    
    // Demo 6: Real-World Kryon Compiler Specification
    println!("\n6. 🌍 Real-World Kryon Compiler Specification");
    demo_kryon_compiler_spec_generation().await?;
    
    println!("\n🎉 Automated Test Generation Demo Complete!");
    println!("\nDemonstrated Capabilities:");
    println!("  ✅ Specification-based test generation");
    println!("  ✅ API contract testing");
    println!("  ✅ Performance requirement validation");
    println!("  ✅ Comprehensive error condition testing");
    println!("  ✅ Edge case and boundary value testing");
    println!("  ✅ Property-based test inference");
    println!("  ✅ Multi-format test output generation");
    println!("  ✅ Quality metrics and coverage analysis");
    
    Ok(())
}

async fn demo_basic_spec_generation() -> Result<()> {
    println!("  📋 Creating basic specification and generating tests...");
    
    let config = GenerationConfig {
        enable_spec_parsing: true,
        enable_edge_case_generation: true,
        max_generated_tests_per_spec: 10,
        coverage_target_percentage: 90.0,
        test_naming_convention: NamingConvention::Descriptive,
        ..Default::default()
    };
    
    let mut generator = TestGenerator::new(config);
    
    // Create a basic specification for a string validation function
    let mut spec = create_simple_spec(
        "string_validator_001",
        "String Validator",
        "Validates input strings according to specified rules"
    );
    
    // Add input parameters
    spec.input_parameters.push(Parameter {
        name: "input".to_string(),
        param_type: "String".to_string(),
        description: "The string to validate".to_string(),
        constraints: vec![
            ParameterConstraint {
                constraint_type: ConstraintType::Length,
                value: "1..255".to_string(),
                description: "String length must be between 1 and 255 characters".to_string(),
            }
        ],
        default_value: None,
        examples: vec!["hello".to_string(), "test123".to_string()],
    });
    
    // Add preconditions
    spec.preconditions.push(Condition {
        condition_id: "input_not_null".to_string(),
        description: "Input string must not be null".to_string(),
        expression: "input != null".to_string(),
        condition_type: ConditionType::Precondition,
        priority: ConditionPriority::Critical,
    });
    
    // Add postconditions
    spec.postconditions.push(Condition {
        condition_id: "result_boolean".to_string(),
        description: "Result must be a boolean value".to_string(),
        expression: "typeof(result) == boolean".to_string(),
        condition_type: ConditionType::Postcondition,
        priority: ConditionPriority::High,
    });
    
    // Add examples
    let mut example1 = create_spec_example(
        "valid_string_example",
        "Valid string should return true",
        ScenarioType::TypicalCase
    );
    example1.inputs.insert("input".to_string(), "\"hello world\"".to_string());
    example1.expected_outputs.insert("result".to_string(), "true".to_string());
    spec.examples.push(example1);
    
    let mut example2 = create_spec_example(
        "empty_string_example",
        "Empty string should return false",
        ScenarioType::EdgeCase
    );
    example2.inputs.insert("input".to_string(), "\"\"".to_string());
    example2.expected_outputs.insert("result".to_string(), "false".to_string());
    spec.examples.push(example2);
    
    // Add error conditions
    spec.error_conditions.push(ErrorCondition {
        error_id: "null_input_error".to_string(),
        error_type: "NullPointerException".to_string(),
        description: "Null input should throw exception".to_string(),
        trigger_conditions: vec!["input == null".to_string()],
        expected_behavior: "Throw NullPointerException".to_string(),
        recovery_actions: vec!["Validate input before processing".to_string()],
    });
    
    generator.add_specification(spec);
    
    // Generate tests
    let result = generator.generate_tests().await?;
    generator.print_generation_summary(&result);
    
    println!("     📊 Basic Generation Results:");
    println!("        Tests generated: {}", result.total_tests_generated);
    println!("        Generation time: {:.2}s", result.generation_time.as_secs_f64());
    
    // Show some generated tests
    for (i, test) in result.generated_tests.iter().take(3).enumerate() {
        println!("        {}. {} ({:?})", i + 1, test.test_name, test.test_type);
    }
    
    Ok(())
}

async fn demo_api_spec_generation() -> Result<()> {
    println!("  🔌 Creating API specification and generating comprehensive tests...");
    
    let config = GenerationConfig {
        enable_spec_parsing: true,
        enable_edge_case_generation: true,
        enable_property_inference: true,
        max_generated_tests_per_spec: 25,
        test_naming_convention: NamingConvention::BehaviorDriven,
        ..Default::default()
    };
    
    let mut generator = TestGenerator::new(config);
    
    // Create a comprehensive API specification
    let mut api_spec = TestSpecification {
        spec_id: "user_api_001".to_string(),
        spec_name: "User Management API".to_string(),
        spec_type: SpecificationType::ApiSpec,
        description: "RESTful API for user management operations".to_string(),
        preconditions: vec![
            Condition {
                condition_id: "authenticated_user".to_string(),
                description: "User must be authenticated".to_string(),
                expression: "auth_token != null && auth_token.valid()".to_string(),
                condition_type: ConditionType::Precondition,
                priority: ConditionPriority::Critical,
            }
        ],
        postconditions: vec![
            Condition {
                condition_id: "valid_response".to_string(),
                description: "Response must have valid HTTP status".to_string(),
                expression: "response.status >= 200 && response.status < 600".to_string(),
                condition_type: ConditionType::Postcondition,
                priority: ConditionPriority::High,
            }
        ],
        invariants: vec![
            Invariant {
                invariant_id: "user_data_consistency".to_string(),
                description: "User data must remain consistent across operations".to_string(),
                expression: "user.email == user.email.toLowerCase()".to_string(),
                scope: InvariantScope::Function,
                strength: InvariantStrength::Always,
            }
        ],
        input_parameters: vec![
            Parameter {
                name: "user_id".to_string(),
                param_type: "UUID".to_string(),
                description: "Unique identifier for the user".to_string(),
                constraints: vec![
                    ParameterConstraint {
                        constraint_type: ConstraintType::Pattern,
                        value: "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$".to_string(),
                        description: "Must be valid UUID format".to_string(),
                    }
                ],
                default_value: None,
                examples: vec!["550e8400-e29b-41d4-a716-446655440000".to_string()],
            },
            Parameter {
                name: "user_data".to_string(),
                param_type: "UserData".to_string(),
                description: "User information object".to_string(),
                constraints: vec![
                    ParameterConstraint {
                        constraint_type: ConstraintType::Custom,
                        value: "email must be valid format".to_string(),
                        description: "Email field must contain valid email address".to_string(),
                    }
                ],
                default_value: None,
                examples: vec![r#"{"name": "John Doe", "email": "john@example.com"}"#.to_string()],
            }
        ],
        expected_outputs: vec![
            OutputSpec {
                output_name: "user".to_string(),
                output_type: "User".to_string(),
                description: "Created or updated user object".to_string(),
                validation_rules: vec![
                    ValidationRule {
                        rule_type: ValidationRuleType::Custom,
                        expression: "user.id != null".to_string(),
                        error_message: "User ID must be assigned".to_string(),
                    }
                ],
                examples: vec![r#"{"id": "uuid", "name": "John Doe", "email": "john@example.com"}"#.to_string()],
            }
        ],
        error_conditions: vec![
            ErrorCondition {
                error_id: "user_not_found".to_string(),
                error_type: "UserNotFoundException".to_string(),
                description: "User with specified ID does not exist".to_string(),
                trigger_conditions: vec!["user_id not in database".to_string()],
                expected_behavior: "Return 404 Not Found status".to_string(),
                recovery_actions: vec!["Check user ID validity".to_string(), "Create user if intended".to_string()],
            },
            ErrorCondition {
                error_id: "invalid_email".to_string(),
                error_type: "ValidationException".to_string(),
                description: "Invalid email format provided".to_string(),
                trigger_conditions: vec!["email format is invalid".to_string()],
                expected_behavior: "Return 400 Bad Request with validation errors".to_string(),
                recovery_actions: vec!["Validate email format before submission".to_string()],
            }
        ],
        performance_requirements: Some(PerformanceSpec {
            max_execution_time: Duration::from_millis(500),
            max_memory_usage: 1024 * 1024, // 1MB
            throughput_requirements: Some(1000.0), // 1000 requests per second
            latency_requirements: Some(Duration::from_millis(100)),
            scalability_requirements: vec![
                ScalabilityRequirement {
                    input_size: 1000,
                    max_time: Duration::from_millis(600),
                    max_memory: 2 * 1024 * 1024, // 2MB
                }
            ],
        }),
        examples: vec![
            {
                let mut example = create_spec_example(
                    "create_user_success",
                    "Successfully create a new user",
                    ScenarioType::TypicalCase
                );
                example.inputs.insert("user_data".to_string(), r#"{"name": "Alice Smith", "email": "alice@example.com"}"#.to_string());
                example.expected_outputs.insert("status".to_string(), "201".to_string());
                example.expected_outputs.insert("user.name".to_string(), "\"Alice Smith\"".to_string());
                example
            },
            {
                let mut example = create_spec_example(
                    "get_user_not_found",
                    "Attempt to get non-existent user",
                    ScenarioType::ErrorCase
                );
                example.inputs.insert("user_id".to_string(), "\"550e8400-e29b-41d4-a716-446655440999\"".to_string());
                example.expected_outputs.insert("status".to_string(), "404".to_string());
                example
            },
            {
                let mut example = create_spec_example(
                    "update_user_invalid_email",
                    "Update user with invalid email format",
                    ScenarioType::ErrorCase
                );
                example.inputs.insert("user_data".to_string(), r#"{"email": "invalid-email"}"#.to_string());
                example.expected_outputs.insert("status".to_string(), "400".to_string());
                example
            }
        ],
    };
    
    generator.add_specification(api_spec);
    
    // Generate comprehensive API tests
    let result = generator.generate_tests().await?;
    generator.print_generation_summary(&result);
    
    println!("     🔌 API Generation Results:");
    println!("        API endpoints tested: 1");
    println!("        Tests generated: {}", result.total_tests_generated);
    println!("        Coverage achieved:");
    println!("          Specification: {:.1}%", result.coverage_analysis.specification_coverage);
    println!("          Error conditions: {:.1}%", result.coverage_analysis.error_condition_coverage);
    println!("          Edge cases: {:.1}%", result.coverage_analysis.edge_case_coverage);
    
    // Show test distribution by type
    let mut type_counts: HashMap<GeneratedTestType, usize> = HashMap::new();
    for test in &result.generated_tests {
        *type_counts.entry(test.test_type.clone()).or_insert(0) += 1;
    }
    
    println!("        Test distribution:");
    for (test_type, count) in type_counts {
        println!("          {:?}: {}", test_type, count);
    }
    
    Ok(())
}

async fn demo_performance_spec_generation() -> Result<()> {
    println!("  ⚡ Creating performance specifications and generating benchmark tests...");
    
    let config = GenerationConfig {
        enable_spec_parsing: true,
        enable_edge_case_generation: false, // Focus on performance
        max_generated_tests_per_spec: 15,
        output_format: GeneratedTestFormat::BenchmarkTest,
        ..Default::default()
    };
    
    let mut generator = TestGenerator::new(config);
    
    // Create performance-focused specification
    let mut perf_spec = TestSpecification {
        spec_id: "layout_engine_perf_001".to_string(),
        spec_name: "Layout Engine Performance".to_string(),
        spec_type: SpecificationType::PerformanceSpec,
        description: "Performance requirements for the Kryon layout engine".to_string(),
        preconditions: vec![
            Condition {
                condition_id: "initialized_engine".to_string(),
                description: "Layout engine must be properly initialized".to_string(),
                expression: "layout_engine.is_initialized()".to_string(),
                condition_type: ConditionType::Precondition,
                priority: ConditionPriority::Critical,
            }
        ],
        postconditions: vec![
            Condition {
                condition_id: "layout_computed".to_string(),
                description: "Layout must be successfully computed".to_string(),
                expression: "result.layout != null".to_string(),
                condition_type: ConditionType::Postcondition,
                priority: ConditionPriority::High,
            }
        ],
        invariants: vec![
            Invariant {
                invariant_id: "memory_bounded".to_string(),
                description: "Memory usage must remain within bounds".to_string(),
                expression: "memory_usage < max_memory_limit".to_string(),
                scope: InvariantScope::Function,
                strength: InvariantStrength::Always,
            }
        ],
        input_parameters: vec![
            Parameter {
                name: "element_count".to_string(),
                param_type: "usize".to_string(),
                description: "Number of UI elements to layout".to_string(),
                constraints: vec![
                    ParameterConstraint {
                        constraint_type: ConstraintType::Range,
                        value: "1..10000".to_string(),
                        description: "Support 1 to 10,000 elements".to_string(),
                    }
                ],
                default_value: Some("100".to_string()),
                examples: vec!["10".to_string(), "100".to_string(), "1000".to_string()],
            },
            Parameter {
                name: "nesting_depth".to_string(),
                param_type: "usize".to_string(),
                description: "Maximum nesting depth of elements".to_string(),
                constraints: vec![
                    ParameterConstraint {
                        constraint_type: ConstraintType::Range,
                        value: "1..50".to_string(),
                        description: "Support up to 50 levels of nesting".to_string(),
                    }
                ],
                default_value: Some("5".to_string()),
                examples: vec!["3".to_string(), "10".to_string(), "25".to_string()],
            }
        ],
        expected_outputs: vec![
            OutputSpec {
                output_name: "layout_result".to_string(),
                output_type: "LayoutResult".to_string(),
                description: "Computed layout information".to_string(),
                validation_rules: vec![
                    ValidationRule {
                        rule_type: ValidationRuleType::Custom,
                        expression: "layout_result.elements.len() == input.element_count".to_string(),
                        error_message: "All elements must have layout computed".to_string(),
                    }
                ],
                examples: vec!["LayoutResult { elements: [...], bounds: Rect {...} }".to_string()],
            }
        ],
        error_conditions: vec![
            ErrorCondition {
                error_id: "memory_exhausted".to_string(),
                error_type: "OutOfMemoryError".to_string(),
                description: "Memory limit exceeded during layout computation".to_string(),
                trigger_conditions: vec!["memory_usage > memory_limit".to_string()],
                expected_behavior: "Fail gracefully with error message".to_string(),
                recovery_actions: vec!["Reduce element count".to_string(), "Optimize layout algorithm".to_string()],
            }
        ],
        performance_requirements: Some(PerformanceSpec {
            max_execution_time: Duration::from_millis(100),
            max_memory_usage: 50 * 1024 * 1024, // 50MB
            throughput_requirements: Some(60.0), // 60 FPS
            latency_requirements: Some(Duration::from_millis(16)), // 16ms for 60 FPS
            scalability_requirements: vec![
                ScalabilityRequirement {
                    input_size: 100,
                    max_time: Duration::from_millis(10),
                    max_memory: 1024 * 1024, // 1MB
                },
                ScalabilityRequirement {
                    input_size: 1000,
                    max_time: Duration::from_millis(50),
                    max_memory: 10 * 1024 * 1024, // 10MB
                },
                ScalabilityRequirement {
                    input_size: 10000,
                    max_time: Duration::from_millis(100),
                    max_memory: 50 * 1024 * 1024, // 50MB
                },
            ],
        }),
        examples: vec![
            {
                let mut example = create_spec_example(
                    "small_layout_benchmark",
                    "Layout 100 elements within time limit",
                    ScenarioType::PerformanceCase
                );
                example.inputs.insert("element_count".to_string(), "100".to_string());
                example.inputs.insert("nesting_depth".to_string(), "5".to_string());
                example.expected_outputs.insert("execution_time".to_string(), "< 10ms".to_string());
                example
            },
            {
                let mut example = create_spec_example(
                    "large_layout_benchmark",
                    "Layout 10000 elements within time limit",
                    ScenarioType::StressCase
                );
                example.inputs.insert("element_count".to_string(), "10000".to_string());
                example.inputs.insert("nesting_depth".to_string(), "10".to_string());
                example.expected_outputs.insert("execution_time".to_string(), "< 100ms".to_string());
                example
            }
        ],
    };
    
    generator.add_specification(perf_spec);
    
    // Generate performance tests
    let result = generator.generate_tests().await?;
    generator.print_generation_summary(&result);
    
    println!("     ⚡ Performance Generation Results:");
    println!("        Performance tests: {}", result.generated_tests.iter().filter(|t| matches!(t.test_type, GeneratedTestType::PerformanceTest)).count());
    println!("        Scalability tests: {}", result.generated_tests.iter().filter(|t| t.test_name.contains("scalability")).count());
    println!("        Quality score: {:.1}/100", result.quality_metrics.execution_efficiency_score);
    
    // Show performance-specific tests
    for test in result.generated_tests.iter().filter(|t| matches!(t.test_type, GeneratedTestType::PerformanceTest)).take(3) {
        println!("        - {} (Est. {}s)", test.test_name, test.metadata.estimated_execution_time.as_secs_f64());
    }
    
    Ok(())
}

async fn demo_error_edge_case_generation() -> Result<()> {
    println!("  🚨 Generating comprehensive error condition and edge case tests...");
    
    let config = GenerationConfig {
        enable_edge_case_generation: true,
        max_generated_tests_per_spec: 30,
        test_naming_convention: NamingConvention::Descriptive,
        ..Default::default()
    };
    
    let mut generator = TestGenerator::new(config);
    
    // Create specification with many error conditions and edge cases
    let mut robust_spec = TestSpecification {
        spec_id: "file_processor_001".to_string(),
        spec_name: "File Processor".to_string(),
        spec_type: SpecificationType::BehaviorSpec,
        description: "Processes various file formats with robust error handling".to_string(),
        preconditions: vec![
            Condition {
                condition_id: "file_exists".to_string(),
                description: "File must exist and be accessible".to_string(),
                expression: "file.exists() && file.readable()".to_string(),
                condition_type: ConditionType::Precondition,
                priority: ConditionPriority::Critical,
            }
        ],
        postconditions: vec![
            Condition {
                condition_id: "processed_successfully".to_string(),
                description: "File must be processed without corruption".to_string(),
                expression: "result.checksum == expected_checksum".to_string(),
                condition_type: ConditionType::Postcondition,
                priority: ConditionPriority::High,
            }
        ],
        invariants: vec![
            Invariant {
                invariant_id: "no_data_loss".to_string(),
                description: "No data should be lost during processing".to_string(),
                expression: "input_size <= output_size".to_string(),
                scope: InvariantScope::Function,
                strength: InvariantStrength::Always,
            }
        ],
        input_parameters: vec![
            Parameter {
                name: "file_path".to_string(),
                param_type: "String".to_string(),
                description: "Path to the file to process".to_string(),
                constraints: vec![
                    ParameterConstraint {
                        constraint_type: ConstraintType::Length,
                        value: "1..4096".to_string(),
                        description: "Path length must be reasonable".to_string(),
                    },
                    ParameterConstraint {
                        constraint_type: ConstraintType::Pattern,
                        value: r"^[a-zA-Z0-9/\._-]+$".to_string(),
                        description: "Path must contain valid characters".to_string(),
                    }
                ],
                default_value: None,
                examples: vec!["/path/to/file.txt".to_string(), "data.json".to_string()],
            },
            Parameter {
                name: "file_size".to_string(),
                param_type: "usize".to_string(),
                description: "Size of the file in bytes".to_string(),
                constraints: vec![
                    ParameterConstraint {
                        constraint_type: ConstraintType::Range,
                        value: "0..1073741824".to_string(), // 0 to 1GB
                        description: "File size must be within reasonable limits".to_string(),
                    }
                ],
                default_value: None,
                examples: vec!["1024".to_string(), "1048576".to_string()],
            }
        ],
        expected_outputs: vec![
            OutputSpec {
                output_name: "processed_data".to_string(),
                output_type: "ProcessedData".to_string(),
                description: "Processed file data".to_string(),
                validation_rules: vec![
                    ValidationRule {
                        rule_type: ValidationRuleType::Custom,
                        expression: "processed_data.is_valid()".to_string(),
                        error_message: "Processed data must be valid".to_string(),
                    }
                ],
                examples: vec!["ProcessedData { content: [...] }".to_string()],
            }
        ],
        error_conditions: vec![
            ErrorCondition {
                error_id: "file_not_found".to_string(),
                error_type: "FileNotFoundError".to_string(),
                description: "File does not exist at specified path".to_string(),
                trigger_conditions: vec!["!file.exists()".to_string()],
                expected_behavior: "Throw FileNotFoundError with descriptive message".to_string(),
                recovery_actions: vec!["Check file path spelling".to_string(), "Verify file permissions".to_string()],
            },
            ErrorCondition {
                error_id: "permission_denied".to_string(),
                error_type: "PermissionError".to_string(),
                description: "Insufficient permissions to read file".to_string(),
                trigger_conditions: vec!["!file.readable()".to_string()],
                expected_behavior: "Throw PermissionError".to_string(),
                recovery_actions: vec!["Check file permissions".to_string(), "Run with elevated privileges".to_string()],
            },
            ErrorCondition {
                error_id: "file_too_large".to_string(),
                error_type: "FileSizeError".to_string(),
                description: "File exceeds maximum size limit".to_string(),
                trigger_conditions: vec!["file.size() > MAX_FILE_SIZE".to_string()],
                expected_behavior: "Throw FileSizeError with size information".to_string(),
                recovery_actions: vec!["Split file into smaller chunks".to_string(), "Increase size limit".to_string()],
            },
            ErrorCondition {
                error_id: "corrupted_file".to_string(),
                error_type: "CorruptionError".to_string(),
                description: "File is corrupted and cannot be processed".to_string(),
                trigger_conditions: vec!["!file.checksum_valid()".to_string()],
                expected_behavior: "Throw CorruptionError with corruption details".to_string(),
                recovery_actions: vec!["Use backup file".to_string(), "Attempt file repair".to_string()],
            },
            ErrorCondition {
                error_id: "disk_full".to_string(),
                error_type: "DiskSpaceError".to_string(),
                description: "Insufficient disk space for processing".to_string(),
                trigger_conditions: vec!["available_space < required_space".to_string()],
                expected_behavior: "Throw DiskSpaceError".to_string(),
                recovery_actions: vec!["Free up disk space".to_string(), "Use alternative storage".to_string()],
            }
        ],
        performance_requirements: None,
        examples: vec![
            {
                let mut example = create_spec_example(
                    "empty_file_edge_case",
                    "Process empty file (0 bytes)",
                    ScenarioType::EdgeCase
                );
                example.inputs.insert("file_path".to_string(), "\"empty.txt\"".to_string());
                example.inputs.insert("file_size".to_string(), "0".to_string());
                example.expected_outputs.insert("processed_data.size".to_string(), "0".to_string());
                example
            },
            {
                let mut example = create_spec_example(
                    "maximum_size_edge_case",
                    "Process file at maximum allowed size",
                    ScenarioType::EdgeCase
                );
                example.inputs.insert("file_path".to_string(), "\"large.bin\"".to_string());
                example.inputs.insert("file_size".to_string(), "1073741823".to_string()); // Just under 1GB
                example.expected_outputs.insert("result".to_string(), "ProcessedData".to_string());
                example
            },
            {
                let mut example = create_spec_example(
                    "special_characters_path",
                    "Handle file path with edge case characters",
                    ScenarioType::EdgeCase
                );
                example.inputs.insert("file_path".to_string(), "\"./test_file.txt\"".to_string());
                example.expected_outputs.insert("result".to_string(), "ProcessedData".to_string());
                example
            }
        ],
    };
    
    generator.add_specification(robust_spec);
    
    // Generate comprehensive error and edge case tests
    let result = generator.generate_tests().await?;
    generator.print_generation_summary(&result);
    
    println!("     🚨 Error & Edge Case Results:");
    
    // Count different types of generated tests
    let error_tests = result.generated_tests.iter().filter(|t| t.test_name.contains("error") || t.test_name.contains("throw")).count();
    let edge_case_tests = result.generated_tests.iter().filter(|t| t.test_name.contains("edge") || t.test_name.contains("boundary")).count();
    let total_assertions = result.generated_tests.iter().map(|t| t.expected_results.len()).sum::<usize>();
    
    println!("        Error condition tests: {}", error_tests);
    println!("        Edge case tests: {}", edge_case_tests);
    println!("        Total assertions: {}", total_assertions);
    println!("        Coverage:");
    println!("          Error conditions: {:.1}%", result.coverage_analysis.error_condition_coverage);
    println!("          Edge cases: {:.1}%", result.coverage_analysis.edge_case_coverage);
    
    // Show sample tests
    println!("        Sample generated tests:");
    for (i, test) in result.generated_tests.iter().take(5).enumerate() {
        let assertion_count = test.expected_results.len();
        println!("          {}. {} ({} assertions)", i + 1, test.test_name, assertion_count);
    }
    
    Ok(())
}

async fn demo_property_based_generation() -> Result<()> {
    println!("  🎲 Generating property-based tests from specifications...");
    
    let config = GenerationConfig {
        enable_property_inference: true,
        enable_edge_case_generation: true,
        max_generated_tests_per_spec: 20,
        output_format: GeneratedTestFormat::PropertyTest,
        ..Default::default()
    };
    
    let mut generator = TestGenerator::new(config);
    
    // Create specification with strong invariants for property testing
    let mut property_spec = TestSpecification {
        spec_id: "sorting_algorithm_001".to_string(),
        spec_name: "Sorting Algorithm Properties".to_string(),
        spec_type: SpecificationType::FormalSpec,
        description: "Formal specification of sorting algorithm properties".to_string(),
        preconditions: vec![
            Condition {
                condition_id: "input_is_list".to_string(),
                description: "Input must be a list of comparable elements".to_string(),
                expression: "input.is_list() && input.all(|x| x.comparable())".to_string(),
                condition_type: ConditionType::Precondition,
                priority: ConditionPriority::Critical,
            }
        ],
        postconditions: vec![
            Condition {
                condition_id: "output_sorted".to_string(),
                description: "Output must be sorted in ascending order".to_string(),
                expression: "output.is_sorted_ascending()".to_string(),
                condition_type: ConditionType::Postcondition,
                priority: ConditionPriority::Critical,
            },
            Condition {
                condition_id: "same_elements".to_string(),
                description: "Output must contain exactly the same elements as input".to_string(),
                expression: "output.multiset_eq(input)".to_string(),
                condition_type: ConditionType::Postcondition,
                priority: ConditionPriority::Critical,
            }
        ],
        invariants: vec![
            Invariant {
                invariant_id: "length_preserved".to_string(),
                description: "Length of list must be preserved".to_string(),
                expression: "output.len() == input.len()".to_string(),
                scope: InvariantScope::Function,
                strength: InvariantStrength::Always,
            },
            Invariant {
                invariant_id: "idempotency".to_string(),
                description: "Sorting an already sorted list should not change it".to_string(),
                expression: "sort(sort(x)) == sort(x)".to_string(),
                scope: InvariantScope::Function,
                strength: InvariantStrength::Always,
            },
            Invariant {
                invariant_id: "stability_for_duplicates".to_string(),
                description: "Duplicate elements should maintain relative order".to_string(),
                expression: "stable_sort_property(input, output)".to_string(),
                scope: InvariantScope::Function,
                strength: InvariantStrength::Usually,
            }
        ],
        input_parameters: vec![
            Parameter {
                name: "input_list".to_string(),
                param_type: "Vec<T>".to_string(),
                description: "List of elements to sort".to_string(),
                constraints: vec![
                    ParameterConstraint {
                        constraint_type: ConstraintType::Length,
                        value: "0..10000".to_string(),
                        description: "Support lists up to 10,000 elements".to_string(),
                    }
                ],
                default_value: None,
                examples: vec!["[3, 1, 4, 1, 5]".to_string(), "[1, 2, 3, 4, 5]".to_string(), "[]".to_string()],
            }
        ],
        expected_outputs: vec![
            OutputSpec {
                output_name: "sorted_list".to_string(),
                output_type: "Vec<T>".to_string(),
                description: "Sorted list of elements".to_string(),
                validation_rules: vec![
                    ValidationRule {
                        rule_type: ValidationRuleType::Custom,
                        expression: "is_sorted(sorted_list)".to_string(),
                        error_message: "Output must be sorted".to_string(),
                    }
                ],
                examples: vec!["[1, 1, 3, 4, 5]".to_string(), "[1, 2, 3, 4, 5]".to_string(), "[]".to_string()],
            }
        ],
        error_conditions: vec![],
        performance_requirements: Some(PerformanceSpec {
            max_execution_time: Duration::from_millis(100),
            max_memory_usage: 1024 * 1024, // 1MB
            throughput_requirements: None,
            latency_requirements: None,
            scalability_requirements: vec![
                ScalabilityRequirement {
                    input_size: 1000,
                    max_time: Duration::from_millis(10),
                    max_memory: 100 * 1024, // 100KB
                }
            ],
        }),
        examples: vec![
            {
                let mut example = create_spec_example(
                    "empty_list_property",
                    "Empty list should remain empty when sorted",
                    ScenarioType::EdgeCase
                );
                example.inputs.insert("input_list".to_string(), "[]".to_string());
                example.expected_outputs.insert("sorted_list".to_string(), "[]".to_string());
                example
            },
            {
                let mut example = create_spec_example(
                    "single_element_property",
                    "Single element list should remain unchanged",
                    ScenarioType::EdgeCase
                );
                example.inputs.insert("input_list".to_string(), "[42]".to_string());
                example.expected_outputs.insert("sorted_list".to_string(), "[42]".to_string());
                example
            },
            {
                let mut example = create_spec_example(
                    "already_sorted_property",
                    "Already sorted list should remain unchanged",
                    ScenarioType::TypicalCase
                );
                example.inputs.insert("input_list".to_string(), "[1, 2, 3, 4, 5]".to_string());
                example.expected_outputs.insert("sorted_list".to_string(), "[1, 2, 3, 4, 5]".to_string());
                example
            }
        ],
    };
    
    generator.add_specification(property_spec);
    
    // Generate property-based tests
    let result = generator.generate_tests().await?;
    generator.print_generation_summary(&result);
    
    println!("     🎲 Property-Based Generation Results:");
    
    let property_tests = result.generated_tests.iter().filter(|t| matches!(t.test_type, GeneratedTestType::PropertyBasedTest)).count();
    let invariant_tests = result.generated_tests.iter().filter(|t| t.test_name.contains("invariant") || t.test_name.contains("property")).count();
    
    println!("        Property-based tests: {}", property_tests);
    println!("        Invariant validation tests: {}", invariant_tests);
    println!("        Quality metrics:");
    println!("          Test diversity: {:.1}/100", result.quality_metrics.test_diversity_score);
    println!("          Specification fidelity: {:.1}/100", result.quality_metrics.specification_fidelity_score);
    
    // Show property tests generated
    println!("        Generated property tests:");
    for (i, test) in result.generated_tests.iter().filter(|t| matches!(t.test_type, GeneratedTestType::PropertyBasedTest)).take(3).enumerate() {
        println!("          {}. {} (Complexity: {})", i + 1, test.test_name, test.metadata.complexity_score);
    }
    
    Ok(())
}

async fn demo_kryon_compiler_spec_generation() -> Result<()> {
    println!("  🌍 Generating tests for real-world Kryon compiler specification...");
    
    let config = GenerationConfig {
        enable_spec_parsing: true,
        enable_code_analysis: true,
        enable_edge_case_generation: true,
        enable_property_inference: true,
        max_generated_tests_per_spec: 40,
        complexity_threshold: 15,
        coverage_target_percentage: 95.0,
        test_naming_convention: NamingConvention::Functional,
        ..Default::default()
    };
    
    let mut generator = TestGenerator::new(config);
    
    // Create comprehensive Kryon compiler specification
    let kryon_spec = TestSpecification {
        spec_id: "kryon_compiler_001".to_string(),
        spec_name: "Kryon UI Language Compiler".to_string(),
        spec_type: SpecificationType::FormalSpec,
        description: "Comprehensive specification for the Kryon UI language compiler".to_string(),
        preconditions: vec![
            Condition {
                condition_id: "valid_source_code".to_string(),
                description: "Input must be valid Kryon source code".to_string(),
                expression: "input.is_valid_kryon_syntax()".to_string(),
                condition_type: ConditionType::Precondition,
                priority: ConditionPriority::Critical,
            },
            Condition {
                condition_id: "compiler_initialized".to_string(),
                description: "Compiler must be properly initialized".to_string(),
                expression: "compiler.is_initialized()".to_string(),
                condition_type: ConditionType::Precondition,
                priority: ConditionPriority::Critical,
            }
        ],
        postconditions: vec![
            Condition {
                condition_id: "bytecode_generated".to_string(),
                description: "Valid bytecode must be generated for valid input".to_string(),
                expression: "result.bytecode.is_valid()".to_string(),
                condition_type: ConditionType::Postcondition,
                priority: ConditionPriority::Critical,
            },
            Condition {
                condition_id: "semantic_preservation".to_string(),
                description: "Bytecode must preserve semantic meaning of source".to_string(),
                expression: "semantically_equivalent(source, result.bytecode)".to_string(),
                condition_type: ConditionType::Postcondition,
                priority: ConditionPriority::High,
            }
        ],
        invariants: vec![
            Invariant {
                invariant_id: "compilation_deterministic".to_string(),
                description: "Compilation must be deterministic".to_string(),
                expression: "compile(source) == compile(source)".to_string(),
                scope: InvariantScope::Function,
                strength: InvariantStrength::Always,
            },
            Invariant {
                invariant_id: "error_recovery".to_string(),
                description: "Compiler must recover from errors gracefully".to_string(),
                expression: "compiler.state.is_consistent() after error".to_string(),
                scope: InvariantScope::Global,
                strength: InvariantStrength::Always,
            }
        ],
        input_parameters: vec![
            Parameter {
                name: "source_code".to_string(),
                param_type: "String".to_string(),
                description: "Kryon source code to compile".to_string(),
                constraints: vec![
                    ParameterConstraint {
                        constraint_type: ConstraintType::Length,
                        value: "1..1048576".to_string(), // 1MB max
                        description: "Source code size must be reasonable".to_string(),
                    }
                ],
                default_value: None,
                examples: vec![
                    "App { Text { text: \"Hello World\" } }".to_string(),
                    "App { window_width: 800 Text { text: \"Test\" color: \"#ff0000\" } }".to_string()
                ],
            },
            Parameter {
                name: "compilation_options".to_string(),
                param_type: "CompilerOptions".to_string(),
                description: "Compiler configuration options".to_string(),
                constraints: vec![],
                default_value: Some("CompilerOptions::default()".to_string()),
                examples: vec!["CompilerOptions { optimize: true }".to_string()],
            }
        ],
        expected_outputs: vec![
            OutputSpec {
                output_name: "compilation_result".to_string(),
                output_type: "CompilationResult".to_string(),
                description: "Result of compilation process".to_string(),
                validation_rules: vec![
                    ValidationRule {
                        rule_type: ValidationRuleType::Custom,
                        expression: "result.success == errors.is_empty()".to_string(),
                        error_message: "Success flag must match error state".to_string(),
                    }
                ],
                examples: vec!["CompilationResult { success: true, bytecode: [...] }".to_string()],
            }
        ],
        error_conditions: vec![
            ErrorCondition {
                error_id: "syntax_error".to_string(),
                error_type: "SyntaxError".to_string(),
                description: "Invalid syntax in source code".to_string(),
                trigger_conditions: vec!["!source.is_syntactically_valid()".to_string()],
                expected_behavior: "Return compilation error with location and description".to_string(),
                recovery_actions: vec!["Fix syntax errors in source code".to_string()],
            },
            ErrorCondition {
                error_id: "semantic_error".to_string(),
                error_type: "SemanticError".to_string(),
                description: "Semantic errors in source code".to_string(),
                trigger_conditions: vec!["!source.is_semantically_valid()".to_string()],
                expected_behavior: "Return semantic error with context".to_string(),
                recovery_actions: vec!["Fix semantic issues".to_string(), "Check type compatibility".to_string()],
            },
            ErrorCondition {
                error_id: "resource_exhaustion".to_string(),
                error_type: "CompilerResourceError".to_string(),
                description: "Compiler runs out of memory or time".to_string(),
                trigger_conditions: vec!["memory_usage > limit || time > timeout".to_string()],
                expected_behavior: "Fail gracefully with resource error".to_string(),
                recovery_actions: vec!["Increase resource limits".to_string(), "Simplify source code".to_string()],
            }
        ],
        performance_requirements: Some(PerformanceSpec {
            max_execution_time: Duration::from_secs(5),
            max_memory_usage: 100 * 1024 * 1024, // 100MB
            throughput_requirements: Some(10.0), // 10 compilations per second
            latency_requirements: Some(Duration::from_millis(100)),
            scalability_requirements: vec![
                ScalabilityRequirement {
                    input_size: 1000,   // 1KB source
                    max_time: Duration::from_millis(50),
                    max_memory: 10 * 1024 * 1024, // 10MB
                },
                ScalabilityRequirement {
                    input_size: 100000, // 100KB source
                    max_time: Duration::from_millis(500),
                    max_memory: 50 * 1024 * 1024, // 50MB
                },
                ScalabilityRequirement {
                    input_size: 1000000, // 1MB source
                    max_time: Duration::from_secs(5),
                    max_memory: 100 * 1024 * 1024, // 100MB
                }
            ],
        }),
        examples: vec![
            {
                let mut example = create_spec_example(
                    "simple_app_compilation",
                    "Compile simple Kryon application",
                    ScenarioType::TypicalCase
                );
                example.inputs.insert("source_code".to_string(), "\"App { Text { text: \\\"Hello World\\\" } }\"".to_string());
                example.expected_outputs.insert("success".to_string(), "true".to_string());
                example.expected_outputs.insert("bytecode_size".to_string(), "> 0".to_string());
                example
            },
            {
                let mut example = create_spec_example(
                    "complex_layout_compilation",
                    "Compile complex layout with multiple components",
                    ScenarioType::TypicalCase
                );
                example.inputs.insert("source_code".to_string(), r#""App { 
                    window_width: 800
                    window_height: 600
                    Flex {
                        direction: column
                        Text { text: \"Header\" }
                        Flex {
                            direction: row
                            Text { text: \"Left\" }
                            Text { text: \"Right\" }
                        }
                    }
                }""#.to_string());
                example.expected_outputs.insert("success".to_string(), "true".to_string());
                example
            },
            {
                let mut example = create_spec_example(
                    "syntax_error_handling",
                    "Handle syntax error gracefully",
                    ScenarioType::ErrorCase
                );
                example.inputs.insert("source_code".to_string(), "\"App { Text { text: \\\"Unclosed string }\"".to_string());
                example.expected_outputs.insert("success".to_string(), "false".to_string());
                example.expected_outputs.insert("error_type".to_string(), "\"SyntaxError\"".to_string());
                example
            },
            {
                let mut example = create_spec_example(
                    "empty_source_edge_case",
                    "Handle empty source code",
                    ScenarioType::EdgeCase
                );
                example.inputs.insert("source_code".to_string(), "\"\"".to_string());
                example.expected_outputs.insert("success".to_string(), "false".to_string());
                example
            },
            {
                let mut example = create_spec_example(
                    "large_source_stress_test",
                    "Compile very large source file",
                    ScenarioType::StressCase
                );
                example.inputs.insert("source_code".to_string(), "generate_large_kryon_source(50000)".to_string());
                example.expected_outputs.insert("compilation_time".to_string(), "< 5s".to_string());
                example
            }
        ],
    };
    
    generator.add_specification(kryon_spec);
    
    // Generate comprehensive compiler tests
    let result = generator.generate_tests().await?;
    generator.print_generation_summary(&result);
    
    println!("     🌍 Kryon Compiler Generation Results:");
    println!("        ═══════════════════════════════════════");
    
    // Detailed analysis of generated tests
    let mut test_type_distribution: HashMap<GeneratedTestType, usize> = HashMap::new();
    let mut priority_distribution: HashMap<TestPriority, usize> = HashMap::new();
    let mut total_estimated_time = Duration::ZERO;
    
    for test in &result.generated_tests {
        *test_type_distribution.entry(test.test_type.clone()).or_insert(0) += 1;
        *priority_distribution.entry(test.metadata.priority.clone()).or_insert(0) += 1;
        total_estimated_time += test.metadata.estimated_execution_time;
    }
    
    println!("        Total tests generated: {}", result.total_tests_generated);
    println!("        Estimated total execution time: {:.1}s", total_estimated_time.as_secs_f64());
    
    println!("        Test type distribution:");
    for (test_type, count) in test_type_distribution {
        println!("          {:?}: {}", test_type, count);
    }
    
    println!("        Priority distribution:");
    for (priority, count) in priority_distribution {
        println!("          {:?}: {}", priority, count);
    }
    
    println!("        Coverage achieved:");
    println!("          Specification coverage: {:.1}%", result.coverage_analysis.specification_coverage);
    println!("          Condition coverage: {:.1}%", result.coverage_analysis.condition_coverage);
    println!("          Path coverage: {:.1}%", result.coverage_analysis.path_coverage);
    println!("          Edge case coverage: {:.1}%", result.coverage_analysis.edge_case_coverage);
    println!("          Error condition coverage: {:.1}%", result.coverage_analysis.error_condition_coverage);
    
    println!("        Quality metrics:");
    println!("          Test completeness: {:.1}/100", result.quality_metrics.test_completeness_score);
    println!("          Test diversity: {:.1}/100", result.quality_metrics.test_diversity_score);
    println!("          Specification fidelity: {:.1}/100", result.quality_metrics.specification_fidelity_score);
    println!("          Code quality: {:.1}/100", result.quality_metrics.generated_code_quality);
    println!("          Maintainability: {:.1}/100", result.quality_metrics.maintainability_score);
    
    if !result.warnings.is_empty() {
        println!("        ⚠️  Warnings generated: {}", result.warnings.len());
        for warning in result.warnings.iter().take(2) {
            println!("          - {}", warning.message);
        }
    }
    
    if !result.recommendations.is_empty() {
        println!("        💡 Key recommendations:");
        for rec in result.recommendations.iter().take(3) {
            println!("          - {} (Impact: {:?})", rec.title, rec.impact);
        }
    }
    
    println!("        📁 Sample generated tests:");
    for (i, test) in result.generated_tests.iter().take(5).enumerate() {
        println!("          {}. {} ({:?}, Priority: {:?})", 
            i + 1, test.test_name, test.test_type, test.metadata.priority);
    }
    
    Ok(())
}

#[cfg(test)]
mod generation_demo_tests {
    use super::*;
    
    #[tokio::test]
    async fn test_basic_generator_creation() {
        let config = GenerationConfig::default();
        let generator = TestGenerator::new(config);
        
        assert!(generator.specifications.is_empty());
        assert!(!generator.generation_templates.is_empty());
    }
    
    #[tokio::test]
    async fn test_specification_addition() {
        let config = GenerationConfig::default();
        let mut generator = TestGenerator::new(config);
        
        let spec = create_simple_spec("test_spec", "Test Spec", "Test description");
        generator.add_specification(spec);
        
        assert_eq!(generator.specifications.len(), 1);
        assert!(generator.specifications.contains_key("test_spec"));
    }
    
    #[tokio::test]
    async fn test_basic_test_generation() {
        let config = GenerationConfig::default();
        let mut generator = TestGenerator::new(config);
        
        let mut spec = create_simple_spec("basic_test_spec", "Basic Test", "Basic test specification");
        
        // Add a simple example
        let mut example = create_spec_example("example_1", "Basic example", ScenarioType::TypicalCase);
        example.inputs.insert("input".to_string(), "\"test\"".to_string());
        example.expected_outputs.insert("output".to_string(), "\"result\"".to_string());
        spec.examples.push(example);
        
        generator.add_specification(spec);
        
        let result = generator.generate_tests().await.unwrap();
        
        assert!(result.total_tests_generated > 0);
        assert!(!result.generated_tests.is_empty());
        assert!(result.generation_time > Duration::ZERO);
    }
    
    #[tokio::test]
    async fn test_error_condition_generation() {
        let config = GenerationConfig::default();
        let mut generator = TestGenerator::new(config);
        
        let mut spec = create_simple_spec("error_test_spec", "Error Test", "Test with error conditions");
        
        // Add error condition
        spec.error_conditions.push(ErrorCondition {
            error_id: "test_error".to_string(),
            error_type: "TestException".to_string(),
            description: "Test error condition".to_string(),
            trigger_conditions: vec!["input == null".to_string()],
            expected_behavior: "Throw TestException".to_string(),
            recovery_actions: vec!["Handle error".to_string()],
        });
        
        generator.add_specification(spec);
        
        let result = generator.generate_tests().await.unwrap();
        
        assert!(result.total_tests_generated > 0);
        
        // Should have at least one error test
        let error_tests = result.generated_tests.iter()
            .filter(|t| t.test_name.contains("error") || t.test_name.contains("throw"))
            .count();
        assert!(error_tests > 0);
    }
}