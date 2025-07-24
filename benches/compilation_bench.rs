//! Compilation performance benchmarks

use criterion::{black_box, criterion_group, criterion_main, Criterion};
use kryc::*;
use std::fs;
use tempfile::TempDir;

fn bench_simple_compilation(c: &mut Criterion) {
    let temp_dir = TempDir::new().unwrap();
    let input_path = temp_dir.path().join("simple.kry");
    let output_path = temp_dir.path().join("simple.krb");
    
    let content = r#"
App {
    window_title: "Benchmark Test"
    Text { text: "Hello World" }
}
"#;
    
    fs::write(&input_path, content).unwrap();
    
    c.bench_function("simple_compilation", |b| {
        b.iter(|| {
            compile_file(
                black_box(input_path.to_str().unwrap()),
                black_box(output_path.to_str().unwrap())
            ).unwrap()
        })
    });
}

fn bench_complex_compilation(c: &mut Criterion) {
    let temp_dir = TempDir::new().unwrap();
    let input_path = temp_dir.path().join("complex.kry");
    let output_path = temp_dir.path().join("complex.krb");
    
    // Load the calculator example
    let content = include_str!("../kryon-examples/calculator.kry");
    fs::write(&input_path, content).unwrap();
    
    c.bench_function("complex_compilation", |b| {
        b.iter(|| {
            compile_file(
                black_box(input_path.to_str().unwrap()),
                black_box(output_path.to_str().unwrap())
            ).unwrap()
        })
    });
}

fn bench_large_file_compilation(c: &mut Criterion) {
    let temp_dir = TempDir::new().unwrap();
    let input_path = temp_dir.path().join("large.kry");
    let output_path = temp_dir.path().join("large.krb");
    
    // Generate large file
    let mut content = String::from("App { window_title: \"Large Test\"\nContainer { layout: \"column\"\n");
    for i in 0..1000 {
        content.push_str(&format!("Text {{ text: \"Item {}\" }}\n", i));
    }
    content.push_str("}}");
    
    fs::write(&input_path, content).unwrap();
    
    c.bench_function("large_file_compilation", |b| {
        b.iter(|| {
            compile_file(
                black_box(input_path.to_str().unwrap()),
                black_box(output_path.to_str().unwrap())
            ).unwrap()
        })
    });
}

fn bench_optimization_levels(c: &mut Criterion) {
    let temp_dir = TempDir::new().unwrap();
    let input_path = temp_dir.path().join("opt.kry");
    
    let content = include_str!("../kryon-examples/calculator.kry");
    fs::write(&input_path, content).unwrap();
    
    let mut group = c.benchmark_group("optimization_levels");
    
    for opt_level in 0..=2 {
        let output_path = temp_dir.path().join(format!("opt{}.krb", opt_level));
        
        group.bench_with_input(
            format!("opt_level_{}", opt_level),
            &opt_level,
            |b, &opt_level| {
                let options = CompilerOptions {
                    optimization_level: opt_level,
                    ..Default::default()
                };
                
                b.iter(|| {
                    compile_file_with_options(
                        black_box(input_path.to_str().unwrap()),
                        black_box(output_path.to_str().unwrap()),
                        black_box(options.clone())
                    ).unwrap()
                })
            }
        );
    }
    
    group.finish();
}

criterion_group!(
    benches,
    bench_simple_compilation,
    bench_complex_compilation,
    bench_large_file_compilation,
    bench_optimization_levels
);

criterion_main!(benches);