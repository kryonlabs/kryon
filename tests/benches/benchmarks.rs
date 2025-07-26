//! Performance benchmarks for Kryon

use criterion::{black_box, criterion_group, criterion_main, Criterion};
use kryon_compiler::{compile_string, CompilerOptions};
use kryon_core::KrbFile;
use kryon_layout::TaffyLayoutEngine;

fn bench_compile_simple(c: &mut Criterion) {
    let kry_source = r#"
        App {
            window_width: 800
            window_height: 600
            
            Container {
                display: flex
                flex_direction: column
                
                Text { text: "Hello" }
                Text { text: "World" }
            }
        }
    "#;
    
    c.bench_function("compile_simple", |b| {
        b.iter(|| {
            let result = compile_string(black_box(kry_source), &CompilerOptions::default());
            assert!(result.is_ok());
        });
    });
}

fn bench_compile_complex(c: &mut Criterion) {
    let kry_source = std::fs::read_to_string("tests/fixtures/nested_layout.kry")
        .expect("Failed to read fixture");
    
    c.bench_function("compile_complex", |b| {
        b.iter(|| {
            let result = compile_string(black_box(&kry_source), &CompilerOptions::default());
            assert!(result.is_ok());
        });
    });
}

fn bench_parse_krb(c: &mut Criterion) {
    // First compile to get KRB data
    let kry_source = std::fs::read_to_string("tests/fixtures/nested_layout.kry")
        .expect("Failed to read fixture");
    let krb_data = compile_string(&kry_source, &CompilerOptions::default())
        .expect("Failed to compile");
    
    c.bench_function("parse_krb", |b| {
        b.iter(|| {
            let result = KrbFile::parse(black_box(&krb_data));
            assert!(result.is_ok());
        });
    });
}

fn bench_layout_computation(c: &mut Criterion) {
    // Create a complex layout scenario
    let mut group = c.benchmark_group("layout");
    
    // Simple layout
    group.bench_function("simple_flex", |b| {
        b.iter(|| {
            let mut engine = TaffyLayoutEngine::new();
            // ... setup simple layout
            engine.compute_layout(/* ... */);
        });
    });
    
    // Complex nested layout
    group.bench_function("nested_flex", |b| {
        b.iter(|| {
            let mut engine = TaffyLayoutEngine::new();
            // ... setup complex layout
            engine.compute_layout(/* ... */);
        });
    });
    
    group.finish();
}

fn bench_property_resolution(c: &mut Criterion) {
    c.bench_function("resolve_properties", |b| {
        b.iter(|| {
            // Benchmark property resolution
            // This would test style inheritance, property lookup, etc.
        });
    });
}

criterion_group!(
    benches,
    bench_compile_simple,
    bench_compile_complex,
    bench_parse_krb,
    bench_layout_computation,
    bench_property_resolution
);
criterion_main!(benches);