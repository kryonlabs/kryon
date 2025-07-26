//! Dedicated benchmark runner for performance testing

use kryon_tests::prelude::*;
use std::env;
use std::process;

#[derive(Debug, Clone)]
struct BenchmarkRunnerConfig {
    benchmark_filter: Option<String>,
    output_format: OutputFormat,
    export_path: Option<PathBuf>,
    warmup_iterations: Option<usize>,
    measurement_iterations: Option<usize>,
    timeout: Duration,
    compare_baseline: Option<PathBuf>,
    save_baseline: Option<PathBuf>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum OutputFormat {
    Human,
    Json,
    Csv,
    Html,
}

#[tokio::main]
async fn main() -> Result<()> {
    let config = parse_args()?;
    
    println!("📊 Kryon Benchmark Runner");
    println!("Output format: {:?}", config.output_format);
    
    if let Some(filter) = &config.benchmark_filter {
        println!("Filter: {}", filter);
    }
    
    if let Some(warmup) = config.warmup_iterations {
        println!("Warmup iterations: {}", warmup);
    }
    
    if let Some(measurement) = config.measurement_iterations {
        println!("Measurement iterations: {}", measurement);
    }
    
    println!("Timeout: {:?}", config.timeout);
    println!();
    
    let start_time = std::time::Instant::now();
    let results = run_benchmarks(&config).await?;
    let total_time = start_time.elapsed();
    
    print_results(&results, total_time, &config)?;
    
    if let Some(export_path) = &config.export_path {
        export_results(&results, export_path, &config)?;
    }
    
    if let Some(baseline_path) = &config.save_baseline {
        save_baseline(&results, baseline_path)?;
    }
    
    if let Some(baseline_path) = &config.compare_baseline {
        compare_with_baseline(&results, baseline_path)?;
    }
    
    Ok(())
}

fn parse_args() -> Result<BenchmarkRunnerConfig> {
    let args: Vec<String> = env::args().collect();
    let mut config = BenchmarkRunnerConfig {
        benchmark_filter: None,
        output_format: OutputFormat::Human,
        export_path: None,
        warmup_iterations: None,
        measurement_iterations: None,
        timeout: Duration::from_secs(600), // 10 minutes default for benchmarks
        compare_baseline: None,
        save_baseline: None,
    };
    
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--filter" | "-f" => {
                i += 1;
                if i < args.len() {
                    config.benchmark_filter = Some(args[i].clone());
                }
            },
            "--output" | "-o" => {
                i += 1;
                if i < args.len() {
                    config.output_format = match args[i].as_str() {
                        "human" => OutputFormat::Human,
                        "json" => OutputFormat::Json,
                        "csv" => OutputFormat::Csv,
                        "html" => OutputFormat::Html,
                        _ => bail!("Invalid output format: {}", args[i]),
                    };
                }
            },
            "--export" | "-e" => {
                i += 1;
                if i < args.len() {
                    config.export_path = Some(PathBuf::from(&args[i]));
                }
            },
            "--warmup" => {
                i += 1;
                if i < args.len() {
                    config.warmup_iterations = Some(args[i].parse()
                        .context("Invalid warmup iterations")?)
                }
            },
            "--iterations" | "-i" => {
                i += 1;
                if i < args.len() {
                    config.measurement_iterations = Some(args[i].parse()
                        .context("Invalid measurement iterations")?)
                }
            },
            "--timeout" => {
                i += 1;
                if i < args.len() {
                    let seconds: u64 = args[i].parse()
                        .context("Invalid timeout value")?;
                    config.timeout = Duration::from_secs(seconds);
                }
            },
            "--compare-baseline" => {
                i += 1;
                if i < args.len() {
                    config.compare_baseline = Some(PathBuf::from(&args[i]));
                }
            },
            "--save-baseline" => {
                i += 1;
                if i < args.len() {
                    config.save_baseline = Some(PathBuf::from(&args[i]));
                }
            },
            "--help" | "-h" => {
                print_help();
                process::exit(0);
            },
            _ => {
                if args[i].starts_with('-') {
                    bail!("Unknown option: {}", args[i]);
                }
            }
        }
        i += 1;
    }
    
    Ok(config)
}

fn print_help() {
    println!("Kryon Benchmark Runner");
    println!();
    println!("USAGE:");
    println!("    benchmark-runner [OPTIONS]");
    println!();
    println!("OPTIONS:");
    println!("    -f, --filter <FILTER>           Filter benchmarks by name pattern");
    println!("    -o, --output <FORMAT>           Output format [default: human]");
    println!("                                     [possible values: human, json, csv, html]");
    println!("    -e, --export <FILE>             Export results to file");
    println!("    --warmup <COUNT>                Number of warmup iterations");
    println!("    -i, --iterations <COUNT>        Number of measurement iterations");
    println!("    --timeout <SECONDS>             Benchmark timeout in seconds [default: 600]");
    println!("    --compare-baseline <FILE>       Compare results with baseline");
    println!("    --save-baseline <FILE>          Save results as new baseline");
    println!("    -h, --help                      Print help information");
    println!();
    println!("EXAMPLES:");
    println!("    benchmark-runner                                    # Run all benchmarks");
    println!("    benchmark-runner -f compilation                     # Run compilation benchmarks only");
    println!("    benchmark-runner -o json -e results.json           # Export as JSON");
    println!("    benchmark-runner --warmup 5 --iterations 50        # Custom iteration counts");
    println!("    benchmark-runner --save-baseline baseline.json     # Save baseline");
    println!("    benchmark-runner --compare-baseline baseline.json  # Compare with baseline");
}

async fn run_benchmarks(config: &BenchmarkRunnerConfig) -> Result<BenchmarkSuite> {
    let test_config = TestConfig {
        timeout_seconds: config.timeout.as_secs(),
        enable_snapshots: false,
        enable_benchmarks: true,
        parallel_execution: false, // Benchmarks should run sequentially for accuracy
        output_directory: PathBuf::from("target/benchmark-output"),
    };
    
    let mut suite = create_default_benchmark_suite();
    
    // Apply custom iterations if specified
    if let Some(warmup) = config.warmup_iterations {
        for benchmark in &mut suite.benchmarks {
            benchmark.config.warmup_iterations = warmup;
        }
    }
    
    if let Some(measurement) = config.measurement_iterations {
        for benchmark in &mut suite.benchmarks {
            benchmark.config.measurement_iterations = measurement;
        }
    }
    
    // Filter benchmarks if requested
    if let Some(filter) = &config.benchmark_filter {
        suite.benchmarks.retain(|b| b.config.name.contains(filter));
    }
    
    // Add custom benchmarks for comprehensive testing
    add_comprehensive_benchmarks(&mut suite);
    
    suite.run_all().await?;
    
    Ok(suite)
}

fn add_comprehensive_benchmarks(suite: &mut BenchmarkSuite) {
    // Large layout benchmark
    suite.add_compilation_benchmark(
        "large_layout",
        &create_large_layout_source(100), // 100 nested elements
    );
    
    // Deep nesting benchmark
    suite.add_compilation_benchmark(
        "deep_nesting",
        &create_deep_nesting_source(50), // 50 levels deep
    );
    
    // Complex theming benchmark
    suite.add_compilation_benchmark(
        "complex_theming",
        &create_complex_theming_source(),
    );
    
    // Large text benchmark
    suite.add_compilation_benchmark(
        "large_text",
        &create_large_text_source(10000), // 10k characters
    );
}

fn create_large_layout_source(element_count: usize) -> String {
    let mut source = "App {\n  Container { layout: \"column\"\n".to_string();
    
    for i in 0..element_count {
        source.push_str(&format!(
            "    Button {{ text: \"Button {}\" }}\n", i
        ));
    }
    
    source.push_str("  }\n}\n");
    source
}

fn create_deep_nesting_source(depth: usize) -> String {
    let mut source = "App {\n".to_string();
    
    // Create nested containers
    for i in 0..depth {
        source.push_str(&"  ".repeat(i + 1));
        source.push_str("Container {\n");
    }
    
    // Add inner text
    source.push_str(&"  ".repeat(depth + 1));
    source.push_str("Text { text: \"Deep text\" }\n");
    
    // Close all containers
    for i in (0..depth).rev() {
        source.push_str(&"  ".repeat(i + 1));
        source.push_str("}\n");
    }
    
    source.push_str("}\n");
    source
}

fn create_complex_theming_source() -> String {
    r#"
Theme light {
    primary: "#3b82f6"
    secondary: "#64748b"
    background: "#ffffff"
    surface: "#f8fafc"
    text: "#1e293b"
    text_muted: "#64748b"
    spacing: 16
    radius: 8
}

Theme dark extends light {
    primary: "#60a5fa"
    background: "#0f172a"
    surface: "#1e293b"
    text: "#f1f5f9"
    text_muted: "#94a3b8"
}

App {
    background_color: ${theme.background}
    padding: ${theme.spacing}
    
    Container {
        layout: "column"
        gap: ${theme.spacing}
        
        Text {
            text: "Complex Theming Demo"
            text_color: ${theme.primary}
            font_size: 24
            font_weight: "bold"
        }
        
        Container {
            layout: "row"
            gap: ${theme.spacing}
            justify_content: "space-between"
            
            Button {
                text: "Primary"
                background_color: ${theme.primary}
                text_color: ${theme.background}
                padding: ${theme.spacing}
                border_radius: ${theme.radius}
            }
            
            Button {
                text: "Secondary"
                background_color: ${theme.secondary}
                text_color: ${theme.background}
                padding: ${theme.spacing}
                border_radius: ${theme.radius}
            }
        }
        
        Container {
            background_color: ${theme.surface}
            padding: ${theme.spacing}
            border_radius: ${theme.radius}
            
            Text {
                text: "Themed surface with responsive text"
                text_color: ${theme.text_muted}
            }
        }
    }
}
"#.to_string()
}

fn create_large_text_source(char_count: usize) -> String {
    let large_text = "A".repeat(char_count);
    format!(
        "App {{\n  Text {{ text: \"{}\" }}\n}}\n",
        large_text
    )
}

fn print_results(suite: &BenchmarkSuite, total_time: Duration, config: &BenchmarkRunnerConfig) -> Result<()> {
    match config.output_format {
        OutputFormat::Human => print_human_results(suite, total_time),
        OutputFormat::Json => print_json_results(suite)?,
        OutputFormat::Csv => print_csv_results(suite)?,
        OutputFormat::Html => print_html_results(suite)?,
    }
    Ok(())
}

fn print_human_results(suite: &BenchmarkSuite, total_time: Duration) {
    println!("\n=== 🏁 Benchmark Results ===");
    println!("Total benchmarks: {}", suite.results.len());
    println!("Total execution time: {:.2}s", total_time.as_secs_f64());
    println!();
    
    // Print detailed results
    suite.print_summary();
    
    // Performance insights
    print_performance_insights(suite);
}

fn print_performance_insights(suite: &BenchmarkSuite) {
    println!("\n=== 📊 Performance Insights ===");
    
    // Find slowest compilation
    if let Some((slowest_name, slowest_result)) = suite.results.iter()
        .max_by_key(|(_, result)| result.statistics.compilation_stats.mean) {
        println!(
            "Slowest compilation: {} ({:.2}ms)",
            slowest_name,
            slowest_result.statistics.compilation_stats.mean.as_secs_f64() * 1000.0
        );
    }
    
    // Find fastest compilation
    if let Some((fastest_name, fastest_result)) = suite.results.iter()
        .min_by_key(|(_, result)| result.statistics.compilation_stats.mean) {
        println!(
            "Fastest compilation: {} ({:.2}ms)",
            fastest_name,
            fastest_result.statistics.compilation_stats.mean.as_secs_f64() * 1000.0
        );
    }
    
    // Performance thresholds
    let slow_threshold = Duration::from_millis(100);
    let slow_benchmarks: Vec<_> = suite.results.iter()
        .filter(|(_, result)| result.statistics.compilation_stats.mean > slow_threshold)
        .collect();
    
    if !slow_benchmarks.is_empty() {
        println!("\n⚠️  Slow benchmarks (>100ms):");
        for (name, result) in slow_benchmarks {
            println!(
                "  {} - {:.2}ms",
                name,
                result.statistics.compilation_stats.mean.as_secs_f64() * 1000.0
            );
        }
    }
    
    // Memory usage insights (when available)
    let benchmarks_with_memory: Vec<_> = suite.results.iter()
        .filter(|(_, result)| result.memory_usage.is_some())
        .collect();
    
    if !benchmarks_with_memory.is_empty() {
        println!("\n📦 Memory Usage:");
        for (name, result) in benchmarks_with_memory {
            if let Some(memory) = &result.memory_usage {
                println!(
                    "  {} - Peak: {}KB, Avg: {}KB",
                    name, memory.peak_memory_kb, memory.average_memory_kb
                );
            }
        }
    }
}

fn print_json_results(suite: &BenchmarkSuite) -> Result<()> {
    let json_output = serde_json::to_string_pretty(&suite.results)?;
    println!("{}", json_output);
    Ok(())
}

fn print_csv_results(suite: &BenchmarkSuite) -> Result<()> {
    println!("benchmark_name,mean_compilation_ms,median_compilation_ms,min_compilation_ms,max_compilation_ms,mean_parsing_ms,mean_total_ms");
    
    for (name, result) in &suite.results {
        println!(
            "{},{:.3},{:.3},{:.3},{:.3},{:.3},{:.3}",
            name,
            result.statistics.compilation_stats.mean.as_secs_f64() * 1000.0,
            result.statistics.compilation_stats.median.as_secs_f64() * 1000.0,
            result.statistics.compilation_stats.min.as_secs_f64() * 1000.0,
            result.statistics.compilation_stats.max.as_secs_f64() * 1000.0,
            result.statistics.parse_stats.mean.as_secs_f64() * 1000.0,
            result.statistics.total_stats.mean.as_secs_f64() * 1000.0,
        );
    }
    
    Ok(())
}

fn print_html_results(suite: &BenchmarkSuite) -> Result<()> {
    println!("<!DOCTYPE html>");
    println!("<html><head><title>Kryon Benchmark Results</title></head><body>");
    println!("<h1>Kryon Benchmark Results</h1>");
    println!("<table border='1'>");
    println!("<tr><th>Benchmark</th><th>Mean Compilation (ms)</th><th>Mean Parsing (ms)</th><th>Mean Total (ms)</th></tr>");
    
    for (name, result) in &suite.results {
        println!(
            "<tr><td>{}</td><td>{:.2}</td><td>{:.2}</td><td>{:.2}</td></tr>",
            name,
            result.statistics.compilation_stats.mean.as_secs_f64() * 1000.0,
            result.statistics.parse_stats.mean.as_secs_f64() * 1000.0,
            result.statistics.total_stats.mean.as_secs_f64() * 1000.0,
        );
    }
    
    println!("</table>");
    println!("</body></html>");
    
    Ok(())
}

fn export_results(suite: &BenchmarkSuite, export_path: &Path, config: &BenchmarkRunnerConfig) -> Result<()> {
    let content = match config.output_format {
        OutputFormat::Json => serde_json::to_string_pretty(&suite.results)?,
        OutputFormat::Csv => {
            let mut csv = "benchmark_name,mean_compilation_ms,median_compilation_ms,min_compilation_ms,max_compilation_ms,mean_parsing_ms,mean_total_ms\n".to_string();
            for (name, result) in &suite.results {
                csv.push_str(&format!(
                    "{},{:.3},{:.3},{:.3},{:.3},{:.3},{:.3}\n",
                    name,
                    result.statistics.compilation_stats.mean.as_secs_f64() * 1000.0,
                    result.statistics.compilation_stats.median.as_secs_f64() * 1000.0,
                    result.statistics.compilation_stats.min.as_secs_f64() * 1000.0,
                    result.statistics.compilation_stats.max.as_secs_f64() * 1000.0,
                    result.statistics.parse_stats.mean.as_secs_f64() * 1000.0,
                    result.statistics.total_stats.mean.as_secs_f64() * 1000.0,
                ));
            }
            csv
        },
        _ => bail!("Export format not supported for file output"),
    };
    
    fs::write(export_path, content)?;
    println!("Results exported to: {}", export_path.display());
    
    Ok(())
}

fn save_baseline(suite: &BenchmarkSuite, baseline_path: &Path) -> Result<()> {
    let baseline_data = serde_json::to_string_pretty(&suite.results)?;
    fs::write(baseline_path, baseline_data)?;
    println!("Baseline saved to: {}", baseline_path.display());
    Ok(())
}

fn compare_with_baseline(suite: &BenchmarkSuite, baseline_path: &Path) -> Result<()> {
    let baseline_content = fs::read_to_string(baseline_path)
        .context("Failed to read baseline file")?;
    
    let baseline_results: std::collections::BTreeMap<String, BenchmarkResult> = 
        serde_json::from_str(&baseline_content)
            .context("Failed to parse baseline JSON")?;
    
    println!("\n=== 📈 Baseline Comparison ===");
    
    for (name, current_result) in &suite.results {
        if let Some(baseline_result) = baseline_results.get(name) {
            let current_time = current_result.statistics.compilation_stats.mean.as_secs_f64() * 1000.0;
            let baseline_time = baseline_result.statistics.compilation_stats.mean.as_secs_f64() * 1000.0;
            let ratio = current_time / baseline_time;
            
            let status = if ratio > 1.1 {
                format!("🔴 SLOWER ({:.1}x)", ratio).red()
            } else if ratio < 0.9 {
                format!("🔵 FASTER ({:.1}x)", 1.0 / ratio).green()
            } else {
                "⚪ SIMILAR".to_string().into()
            };
            
            println!(
                "{}: {:.2}ms vs {:.2}ms - {}",
                name, current_time, baseline_time, status
            );
        } else {
            println!("{}: 🆕 NEW BENCHMARK", name);
        }
    }
    
    // Check for removed benchmarks
    for baseline_name in baseline_results.keys() {
        if !suite.results.contains_key(baseline_name) {
            println!("{}: ❌ REMOVED", baseline_name);
        }
    }
    
    Ok(())
}
