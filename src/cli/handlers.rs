// FILE: src/cli/handlers.rs
use crate::{
    cli::OutputFormat, // Import from the `cli` module
    compile_file_with_options, CompilerError, CompilerOptions, Result,
};

use notify::{Event, RecommendedWatcher, RecursiveMode, Watcher};
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::mpsc::channel;
use std::time::Instant;
use std::fs;

// --- COMPILE ---
pub fn handle_compile_command(cli: &super::EnhancedCli, matches: &clap::ArgMatches) -> Result<()> {
    let input_path = matches.get_one::<String>("input").unwrap();
    let output_path = matches
        .get_one::<String>("output")
        .map(|s| s.to_string())
        .unwrap_or_else(|| {
            Path::new(input_path)
                .with_extension("krb")
                .to_string_lossy()
                .into_owned()
        });

    let options = cli.build_compiler_options(matches)?;

    if matches.get_flag("watch") {
        watch_and_compile(input_path, &output_path, options)
    } else {
        compile_single_file(input_path, &output_path, options, matches)
    }
}

fn compile_single_file(
    input_path: &str,
    output_path: &str,
    options: CompilerOptions,
    matches: &clap::ArgMatches,
) -> Result<()> {
    println!("🔨 Compiling {} -> {}", input_path, output_path);

    let compile_start = Instant::now();
    let stats = compile_file_with_options(input_path, output_path, options)?;
    let compile_time = compile_start.elapsed();

    println!("✅ Compilation successful!");
    println!("   Output: {} bytes", stats.output_size);
    println!("   Time: {:.2}ms", compile_time.as_millis());

    if stats.source_size > 0 {
        let compression_ratio = (1.0 - stats.compression_ratio) * 100.0;
        println!("   Compression: {:.1}%", compression_ratio);
    }

    if matches.get_flag("stats") {
        print_detailed_stats(&stats)?;
    }

    let format = matches.get_one::<OutputFormat>("format").unwrap();
    match format {
        OutputFormat::Krb => {}
        OutputFormat::Json => {
            output_debug_json(output_path)?;
        }
        OutputFormat::Debug => {
            output_debug_info(output_path)?;
        }
    }

    Ok(())
}

fn watch_and_compile(
    input_path: &str,
    output_path: &str,
    options: CompilerOptions,
) -> Result<()> {
    println!("👀 Watching {} for changes...", input_path);

    let (tx, rx) = channel();
    let mut watcher = RecommendedWatcher::new(
        move |res: notify::Result<Event>| {
            if let Ok(event) = res {
                if let Err(e) = tx.send(event) {
                    eprintln!("Watch error: {}", e);
                }
            }
        },
        notify::Config::default(),
    )
    .map_err(|e| {
        CompilerError::Io(std::io::Error::new(
            std::io::ErrorKind::Other,
            format!("Failed to create file watcher: {}", e),
        ))
    })?;

    watcher
        .watch(Path::new(input_path), RecursiveMode::NonRecursive)
        .map_err(|e| {
            CompilerError::Io(std::io::Error::new(
                std::io::ErrorKind::Other,
                format!("Failed to watch file: {}", e),
            ))
        })?;

    if let Err(e) = compile_file_with_options(input_path, output_path, options.clone()) {
        eprintln!("❌ Initial compilation failed: {}", e);
    } else {
        println!("✅ Initial compilation successful");
    }

    loop {
        match rx.recv() {
            Ok(_event) => {
                println!("🔄 File changed, recompiling...");
                match compile_file_with_options(input_path, output_path, options.clone()) {
                    Ok(stats) => {
                        println!(
                            "✅ Recompiled successfully ({} bytes, {}ms)",
                            stats.output_size, stats.compile_time_ms
                        );
                    }
                    Err(e) => eprintln!("❌ Compilation failed: {}", e),
                }
            }
            Err(e) => {
                eprintln!("Watch error: {}", e);
                break;
            }
        }
    }

    Ok(())
}

// --- CHECK ---
pub fn handle_check_command(matches: &clap::ArgMatches) -> Result<()> {
    let input_path = matches.get_one::<String>("input").unwrap();
    let recursive = matches.get_flag("recursive");

    if recursive && Path::new(input_path).is_dir() {
        check_directory_recursive(input_path)
    } else {
        check_single_file(input_path)
    }
}

fn check_single_file(input_path: &str) -> Result<()> {
    println!("🔍 Checking {}", input_path);
    let options = CompilerOptions {
        debug_mode: true,
        ..Default::default()
    };
    match compile_file_with_options(input_path, "/dev/null", options) {
        Ok(_) => {
            println!("✅ {} - No issues found", input_path);
            Ok(())
        }
        Err(e) => {
            println!("❌ {} - {}", input_path, e);
            Err(e)
        }
    }
}

fn check_directory_recursive(dir_path: &str) -> Result<()> {
    let mut total_files = 0;
    let mut error_files = 0;

    for entry in walkdir::WalkDir::new(dir_path) {
        let entry = entry.map_err(|e| {
            CompilerError::Io(std::io::Error::new(
                std::io::ErrorKind::Other,
                format!("Directory traversal error: {}", e),
            ))
        })?;
        if entry.file_type().is_file() {
            if let Some(ext) = entry.path().extension() {
                if ext == "kry" {
                    total_files += 1;
                    if check_single_file(entry.path().to_str().unwrap()).is_err() {
                        error_files += 1;
                    }
                }
            }
        }
    }

    println!("\n📊 Check Summary:");
    println!("   Total files: {}", total_files);
    println!("   Files with errors: {}", error_files);
    if total_files > 0 {
        println!(
            "   Success rate: {:.1}%",
            (total_files - error_files) as f64 / total_files as f64 * 100.0
        );
    }

    if error_files > 0 {
        Err(CompilerError::Semantic {
            file: "".to_string(),
            line: 0,
            message: format!("{} files have errors", error_files),
        })
    } else {
        Ok(())
    }
}

// --- ANALYZE ---
pub fn handle_analyze_command(matches: &clap::ArgMatches) -> Result<()> {
    let input_path = matches.get_one::<String>("input").unwrap();
    let output_path = matches.get_one::<String>("output");
    let format = matches.get_one::<OutputFormat>("format").unwrap();

    println!("🔬 Analyzing {}", input_path);

    if input_path.ends_with(".krb") {
        analyze_krb_file(input_path, output_path, format)
    } else {
        analyze_kry_file(input_path, output_path, format)
    }
}

fn analyze_krb_file(
    input_path: &str,
    output_path: Option<&String>,
    format: &OutputFormat,
) -> Result<()> {
    let krb_info = crate::analyze_krb_file(input_path)?;
    let analysis = match format {
        OutputFormat::Json => {
            serde_json::to_string_pretty(&krb_info).map_err(|e| CompilerError::CodeGen {
                message: format!("JSON serialization error: {}", e),
            })?
        }
        _ => format!("KRB File Analysis: {}\n\n{:#?}", input_path, krb_info),
    };
    if let Some(output_file) = output_path {
        std::fs::write(output_file, analysis)?;
        println!("✅ Analysis saved to {}", output_file);
    } else {
        println!("{}", analysis);
    }
    Ok(())
}

fn analyze_kry_file(
    input_path: &str,
    output_path: Option<&String>,
    format: &OutputFormat,
) -> Result<()> {
    let options = CompilerOptions {
        debug_mode: true,
        optimization_level: 0,
        ..Default::default()
    };
    let stats = compile_file_with_options(input_path, "/dev/null", options)?;
    let analysis = match format {
        OutputFormat::Json => serde_json::to_string_pretty(&stats).map_err(|e| {
            CompilerError::CodeGen {
                message: format!("JSON serialization error: {}", e),
            }
        })?,
        _ => format!("KRY File Analysis: {}\n\n{:#?}", input_path, stats),
    };
    if let Some(output_file) = output_path {
        std::fs::write(output_file, analysis)?;
        println!("✅ Analysis saved to {}", output_file);
    } else {
        println!("{}", analysis);
    }
    Ok(())
}

// --- INIT ---
pub fn handle_init_command(matches: &clap::ArgMatches) -> Result<()> {
    let project_name = matches.get_one::<String>("name").unwrap();
    let template = matches.get_one::<String>("template").unwrap();

    println!("🚀 Initializing new Kryon project: {}", project_name);

    let project_dir = PathBuf::from(project_name);
    if project_dir.exists() {
        return Err(CompilerError::InvalidFormat {
            message: format!("Directory '{}' already exists", project_name),
        });
    }

    std::fs::create_dir_all(&project_dir)?;


    match template.as_str() {
        // These calls are now correct because the functions take 2 arguments
        "simple" => create_simple_template(&project_dir, project_name)?,
        "component" => create_component_template(&project_dir, project_name)?,
        "game" => create_game_template(&project_dir, project_name)?,
        _ => return Err(CompilerError::InvalidFormat {
            message: format!("Unknown template: {}", template),
        }),
    }

    println!("✅ Project created successfully!");
    println!("   Directory: {}", project_dir.display());
    println!("   Template: {}", template);
    println!("\nNext steps:");
    println!("   cd {}", project_name);
    println!("   kryc compile app.kry");

    Ok(())
}

fn create_simple_template(project_dir: &Path, project_name: &str) -> Result<()> {
    let app_content = format!(
        r##"# Simple Kryon Application
    @variables {{{{
        app_title: "{}"
        primary_color: "#007BFF"
        window_size: 800
    }}}}

    App {{{{
        window_title: $app_title
        window_width: $window_size
        window_height: 600
        background_color: "#F8F9FA"
        
        Container {{{{
            layout: "column center"
            padding: 32
            
            Text {{{{
                text: "Welcome to {}"
                font_size: 24
                font_weight: "bold"
                text_color: $primary_color
                margin: "0 0 16 0"
            }}}}
            
            Text {{{{
                text: "Your Kryon application is ready!"
                font_size: 16
                text_color: "#6C757D"
                margin: "0 0 32 0"
            }}}}
            
            Button {{{{
                text: "Get Started"
                background_color: $primary_color
                text_color: "#FFFFFF"
                padding: "12 24"
                border_radius: 6
                
                &:hover {{{{
                    background_color: "#0056B3"
                }}}}
            }}}}
        }}}}
    }}}}
    "##,
        project_name, project_name
    );

    std::fs::write(project_dir.join("app.kry"), app_content)?;
    let config_content = r#"{ ... }"#; // Body from original file
    std::fs::write(project_dir.join("kryon.json"), config_content)?;
    Ok(())
}


fn create_component_template(project_dir: &Path, project_name: &str) -> Result<()> {
    // Create components directory
    let components_dir = project_dir.join("components");
    fs::create_dir_all(&components_dir)?;
    
    // Create Card component
    let card_component = r##"Define Card {
Properties {
    title: String = "Card Title"
    content: String = "Card content goes here"
    width: String = "300px"
}

Container {
    width: $width
    background_color: "#FFFFFF"
    border_color: "#DEE2E6"
    border_width: 1
    border_radius: 8
    padding: 16
    
    Text {
        text: $title
        font_size: 18
        font_weight: "bold"
        margin: "0 0 12 0"
    }
    
    Text {
        text: $content
        font_size: 14
        text_color: "#6C757D"
    }
}
}
"##;

Ok(())
}
    

fn create_game_template(project_dir: &Path, project_name: &str) -> Result<()> {
    // Create scripts directory
    let scripts_dir = project_dir.join("scripts");
    fs::create_dir_all(&scripts_dir)?;
    
    // Create game logic script
    let game_script = r##"-- Game state
local game = {
    score = 0,
    lives = 3,
    level = 1,
    paused = false
}

-- Initialize game
function initGame()
    game.score = 0
    game.lives = 3
    game.level = 1
    game.paused = false
    
    updateUI()
end

-- Update UI elements
function updateUI()
    local scoreText = kryon.getElementById("score")
    if scoreText then
        scoreText.text = "Score: " .. game.score
    end
    
    local livesText = kryon.getElementById("lives")
    if livesText then
        livesText.text = "Lives: " .. game.lives
    end
    
    local levelText = kryon.getElementById("level")
    if levelText then
        levelText.text = "Level: " .. game.level
    end
end

-- Handle button clicks
function startGame()
    if game.paused then
        game.paused = false
        kryon.getElementById("startBtn").text = "Pause"
    else
        game.paused = true
        kryon.getElementById("startBtn").text = "Start"
    end
end

function resetGame()
    initGame()
    local startBtn = kryon.getElementById("startBtn")
    if startBtn then
        startBtn.text = "Start"
    end
end

function addScore(points)
    game.score = game.score + points
    updateUI()
    
    -- Level up every 1000 points
    if game.score > 0 and game.score % 1000 == 0 then
        game.level = game.level + 1
        updateUI()
    end
end

-- Initialize when script loads
initGame()
"##;
    
    fs::write(scripts_dir.join("game.lua"), game_script)?;
    

    let app_content = format!(r##"@variables {{{{
        app_title: "{} Game"
        primary_color: "#007BFF"
        success_color: "#28A745"
        danger_color: "#DC3545"
        dark_color: "#343A40"
    }}}}

    @script "lua" from "scripts/game.lua"

    style "game_button" {{{{
        padding: "12 24"
        border_radius: 6
        text_color: "#FFFFFF"
        font_weight: "bold"
        cursor: "pointer"
        
        &:hover {{{{
            opacity: 0.9
        }}}}
        
        &:active {{{{
            opacity: 0.8
        }}}}
    }}}}

    App {{{{
        window_title: $app_title
        window_width: 800
        window_height: 600
        background_color: "#F8F9FA"
        
        Container {{{{
            layout: "column"
            padding: 24
            
            Text {{{{
                text: $app_title
                font_size: 32
                font_weight: "bold"
                text_color: $primary_color
            }}}}
            
            Button {{{{
                id: "startBtn"
                text: "Start"
                style: "game_button"
                background_color: $success_color
                onClick: "startGame"
            }}}}
        }}}}
    }}}}
    "##, project_name);
    
    fs::write(project_dir.join("app.kry"), app_content)?;
    
    Ok(())
}


// --- BENCHMARK ---
pub fn handle_benchmark_command(matches: &clap::ArgMatches) -> Result<()> {
    let input_path = matches.get_one::<String>("input").unwrap();
    let iterations: usize = matches
        .get_one::<String>("iterations")
        .unwrap()
        .parse()
        .map_err(|_| CompilerError::InvalidFormat {
            message: "Invalid iterations number".to_string(),
        })?;
    let warmup: usize = matches
        .get_one::<String>("warmup")
        .unwrap()
        .parse()
        .map_err(|_| CompilerError::InvalidFormat {
            message: "Invalid warmup number".to_string(),
        })?;

    println!("🏁 Running compilation benchmarks");
    println!("   Input: {}", input_path);
    println!("   Warmup iterations: {}", warmup);
    println!("   Benchmark iterations: {}", iterations);

    let options = CompilerOptions::default();

    print!("   Warming up");
    for _ in 0..warmup {
        print!(".");
        std::io::stdout().flush().unwrap();
        let _ = compile_file_with_options(input_path, "/dev/null", options.clone());
    }
    println!(" done");

    let mut times = Vec::new();
    print!("   Benchmarking");

    for _ in 0..iterations {
        print!(".");
        std::io::stdout().flush().unwrap();
        let start = Instant::now();
        if let Ok(_) = compile_file_with_options(input_path, "/dev/null", options.clone()) {
            times.push(start.elapsed().as_nanos() as f64 / 1_000_000.0);
        }
    }
    println!(" done");

    if times.is_empty() {
        return Err(CompilerError::CodeGen {
            message: "All benchmark iterations failed".to_string(),
        });
    }

    times.sort_by(|a, b| a.partial_cmp(b).unwrap());
    let min = times[0];
    let max = times[times.len() - 1];
    let median = times[times.len() / 2];
    let mean = times.iter().sum::<f64>() / times.len() as f64;
    let std_dev = {
        let variance = times.iter().map(|&x| (x - mean).powi(2)).sum::<f64>() / times.len() as f64;
        variance.sqrt()
    };

    println!("\n📊 Benchmark Results:");
    println!("   Successful iterations: {}/{}", times.len(), iterations);
    println!("   Min time: {:.2}ms", min);
    println!("   Max time: {:.2}ms", max);
    println!("   Median time: {:.2}ms", median);
    println!("   Mean time: {:.2}ms ± {:.2}ms", mean, std_dev);
    if std_dev / mean > 0.1 {
        println!("   ⚠️  High variance detected ({:.1}%)", (std_dev / mean) * 100.0);
    }
    Ok(())
}

// --- HELPERS ---
fn print_detailed_stats(stats: &crate::CompilationStats) -> Result<()> {
    println!("\n📊 Detailed Compilation Statistics:");
    println!("   Source size: {} bytes", stats.source_size);
    println!("   Output size: {} bytes", stats.output_size);
    println!("   Compression ratio: {:.1}%", (1.0 - stats.compression_ratio) * 100.0);
    println!("   Compile time: {}ms", stats.compile_time_ms);
    println!("   Peak memory: {} bytes", stats.peak_memory_usage);
    println!("\n   Element breakdown:");
    println!("     Elements: {}", stats.element_count);
    println!("     Styles: {}", stats.style_count);
    println!("     Components: {}", stats.component_count);
    println!("     Scripts: {}", stats.script_count);
    println!("     Resources: {}", stats.resource_count);
    println!("     Strings: {}", stats.string_count);
    println!("     Variables: {}", stats.variable_count);
    if stats.include_count > 0 {
        println!("     Includes: {}", stats.include_count);
    }
    Ok(())
}

fn output_debug_json(output_path: &str) -> Result<()> {
    let json_path = Path::new(output_path).with_extension("json");
    println!("   Debug JSON: {}", json_path.display());
    Ok(())
}

fn output_debug_info(output_path: &str) -> Result<()> {
    let debug_path = Path::new(output_path).with_extension("debug");
    println!("   Debug info: {}", debug_path.display());
    Ok(())
}
