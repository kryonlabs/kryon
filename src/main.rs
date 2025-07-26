//! Kryon - Unified Cross-Platform UI Framework
//! 
//! A comprehensive tool that automatically compiles and runs Kryon (.kry) files
//! with multiple renderer backends and extensive debugging capabilities.

use std::path::{Path, PathBuf};
use std::process::Command;
use std::fs;

use anyhow::{Result, Context, bail};
use clap::{Parser, Subcommand, ValueEnum};
use colored::*;
use inquire::Select;
use tracing::{debug, warn, error};
use tracing_subscriber::EnvFilter;
use walkdir::WalkDir;

// Import Kryon components
use kryon_compiler::compile_file;

/// Kryon - Unified UI Framework Tool
#[derive(Parser)]
#[command(
    name = "kryon",
    version,
    about = "Kryon cross-platform UI framework - compile and run .kry files seamlessly",
    long_about = "
Kryon is a unified tool for working with Kryon UI files (.kry). It automatically
compiles your source files and runs them with the appropriate renderer backend.

Examples:
  kryon hello_world.kry           # Auto-compile and run with default renderer
  kryon -r wgpu app.kry          # Use specific renderer 
  kryon --debug app.kry          # Run with debug output
  kryon --compile-only app.kry   # Just compile to .krb
  kryon --list-renderers         # Show available renderers
"
)]
pub struct Cli {
    /// Input .kry file to compile and run
    #[arg(value_name = "FILE")]
    pub input: Option<PathBuf>,

    /// Renderer to use
    #[arg(short = 'r', long = "renderer", value_enum)]
    pub renderer: Option<RendererType>,

    /// Enable debug output
    #[arg(short = 'd', long = "debug")]
    pub debug: bool,

    /// Verbose output
    #[arg(short = 'v', long = "verbose")]
    pub verbose: bool,

    /// Only compile, don't run
    #[arg(short = 'c', long = "compile-only")]
    pub compile_only: bool,

    /// Output directory for compiled .krb files
    #[arg(short = 'o', long = "output")]
    pub output: Option<PathBuf>,

    /// List available renderers
    #[arg(long = "list-renderers")]
    pub list_renderers: bool,

    /// Force recompilation even if .krb is newer
    #[arg(short = 'f', long = "force")]
    pub force: bool,

    /// Enable hot reload (watch for file changes)
    #[arg(long = "watch")]
    pub watch: bool,

    /// Clean compiled files
    #[arg(long = "clean")]
    pub clean: bool,

    #[command(subcommand)]
    pub command: Option<Commands>,
}

#[derive(Subcommand)]
pub enum Commands {
    /// Run a compiled .krb file directly
    Run {
        /// Path to .krb file
        file: PathBuf,
        /// Renderer to use
        #[arg(short = 'r', long = "renderer", value_enum)]
        renderer: Option<RendererType>,
    },
    /// Compile .kry to .krb
    Compile {
        /// Input .kry file
        input: PathBuf,
        /// Output .krb file (optional)
        #[arg(short = 'o', long = "output")]
        output: Option<PathBuf>,
    },
    /// Show information about a .kry or .krb file
    Info {
        /// File to analyze
        file: PathBuf,
    },
    /// Initialize a new Kryon project
    Init {
        /// Project name
        name: Option<String>,
    },
    /// Development tools
    Dev {
        #[command(subcommand)]
        dev_command: DevCommands,
    },
}

#[derive(Subcommand)]
pub enum DevCommands {
    /// Run tests
    Test,
    /// Profile performance
    Profile {
        file: PathBuf,
    },
    /// Benchmark renderer
    Bench {
        #[arg(short = 'r', long = "renderer", value_enum)]
        renderer: Option<RendererType>,
    },
}

#[derive(Clone, Debug, ValueEnum, PartialEq)]
pub enum RendererType {
    /// Raylib renderer (2D/3D graphics, default)
    Raylib,
    /// WGPU renderer (high-performance GPU)
    Wgpu,
    /// SDL2 renderer (cross-platform)
    Sdl2,
    /// Ratatui renderer (terminal UI)
    Ratatui,
    /// HTML server renderer (web)
    Html,
    /// Debug text output
    Debug,
}

impl RendererType {
    fn binary_name(&self) -> &'static str {
        match self {
            RendererType::Raylib => "kryon-renderer-raylib",
            RendererType::Wgpu => "kryon-renderer-wgpu", 
            RendererType::Sdl2 => "kryon-renderer-sdl2",
            RendererType::Ratatui => "kryon-renderer-ratatui",
            RendererType::Html => "kryon-renderer-html",
            RendererType::Debug => "kryon-renderer-debug",
        }
    }

    fn description(&self) -> &'static str {
        match self {
            RendererType::Raylib => "2D/3D graphics renderer (desktop)",
            RendererType::Wgpu => "High-performance GPU renderer (desktop)",
            RendererType::Sdl2 => "SDL2 cross-platform renderer (desktop)",
            RendererType::Ratatui => "Terminal UI renderer (text-based)",
            RendererType::Html => "HTML/CSS web renderer (server)",
            RendererType::Debug => "Debug text output renderer",
        }
    }

    fn is_available(&self) -> bool {
        // Check if the renderer binary exists or if features are compiled in
        match self {
            RendererType::Raylib => cfg!(feature = "raylib"),
            RendererType::Wgpu => cfg!(feature = "wgpu"),
            RendererType::Sdl2 => cfg!(feature = "sdl2"),
            RendererType::Ratatui => cfg!(feature = "ratatui"),
            RendererType::Html => cfg!(feature = "html-server"),
            RendererType::Debug => true, // Always available
        }
    }
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    
    // Initialize logging
    let log_level = if cli.debug {
        "debug"
    } else if cli.verbose {
        "info"
    } else {
        "warn"
    };
    
    tracing_subscriber::fmt()
        .with_env_filter(EnvFilter::from_default_env().add_directive(log_level.parse()?))
        .with_target(false)
        .init();

    // Print banner
    if !cli.compile_only && !cli.list_renderers && !cli.clean {
        print_banner();
    }

    // Handle special flags
    if cli.list_renderers {
        return list_renderers();
    }

    if cli.clean {
        return clean_compiled_files();
    }

    // Handle subcommands
    if let Some(command) = cli.command {
        return handle_command(command);
    }

    // Main workflow: compile and optionally run
    if let Some(input) = cli.input.clone() {
        run_main_workflow(cli, input)
    } else {
        // No input file specified, show help
        println!("{}", "No input file specified. Use --help for usage information.".red());
        std::process::exit(1);
    }
}

fn print_banner() {
    println!("{}", "
╦╔═╦═╗╦ ╦╔═╗╔╗╔
╠╩╗╠╦╝╚╦╝║ ║║║║
╩ ╩╩╚═ ╩ ╚═╝╝╚╝
".bright_blue());
    println!("{}", "Kryon Cross-Platform UI Framework".bright_white().bold());
    println!();
}

fn list_renderers() -> Result<()> {
    println!("{}", "Available Renderers:".bright_white().bold());
    println!();

    let renderers = [
        RendererType::Raylib,
        RendererType::Wgpu,
        RendererType::Ratatui,
        RendererType::Html,
        RendererType::Debug,
    ];

    for renderer in &renderers {
        let status = if renderer.is_available() {
            "✓".bright_green()
        } else {
            "✗".bright_red()
        };
        
        println!("  {} {:<10} - {}", 
            status,
            format!("{:?}", renderer).bright_cyan(),
            renderer.description()
        );
    }

    println!();
    println!("To enable additional renderers, rebuild with feature flags:");
    println!("  {}", "cargo build --features wgpu,ratatui,html-server".bright_yellow());

    Ok(())
}

fn clean_compiled_files() -> Result<()> {
    println!("{}", "Cleaning compiled .krb files...".bright_white());
    
    let current_dir = std::env::current_dir()?;
    let mut count = 0;

    for entry in WalkDir::new(current_dir) {
        let entry = entry?;
        if entry.path().extension() == Some(std::ffi::OsStr::new("krb")) {
            fs::remove_file(entry.path())?;
            println!("  {} {}", "Removed".bright_red(), entry.path().display());
            count += 1;
        }
    }

    println!("{} {} compiled files", "Cleaned".bright_green(), count);
    Ok(())
}

fn run_main_workflow(cli: Cli, input: PathBuf) -> Result<()> {
    // Validate input file
    if !input.exists() {
        bail!("Input file does not exist: {}", input.display());
    }

    let input_ext = input.extension()
        .and_then(|s| s.to_str())
        .unwrap_or("");

    match input_ext {
        "kry" => {
            // Compile .kry to .krb
            let krb_path = compile_kry_file(&input, &cli)?;
            
            if cli.compile_only {
                println!("{} Compiled to: {}", 
                    "✓".bright_green(), 
                    krb_path.display().to_string().bright_cyan()
                );
                return Ok(());
            }

            // Run the compiled .krb file
            run_krb_file(&krb_path, &cli)
        },
        "krb" => {
            // Run .krb file directly
            run_krb_file(&input, &cli)
        },
        _ => {
            bail!("Unsupported file extension. Expected .kry or .krb, got: {}", input_ext);
        }
    }
}

fn compile_kry_file(input: &Path, cli: &Cli) -> Result<PathBuf> {
    let output_path = if let Some(output) = &cli.output {
        if output.is_dir() {
            output.join(input.file_stem().unwrap()).with_extension("krb")
        } else {
            output.clone()
        }
    } else {
        input.with_extension("krb")
    };

    // Check if recompilation is needed
    if !cli.force && should_skip_compilation(input, &output_path)? {
        debug!("Skipping compilation, .krb is up to date");
        return Ok(output_path);
    }

    println!("{} Compiling {} → {}", 
        "⚡".bright_yellow(),
        input.display().to_string().bright_white(),
        output_path.display().to_string().bright_cyan()
    );

    // Compile using kryon-compiler
    match compile_to_krb(input, &output_path) {
        Ok(()) => {
            println!("{} Compilation successful", "✓".bright_green());
            Ok(output_path)
        },
        Err(e) => {
            error!("Compilation failed: {}", e);
            bail!("Failed to compile {}: {}", input.display(), e);
        }
    }
}

fn should_skip_compilation(kry_path: &Path, krb_path: &Path) -> Result<bool> {
    if !krb_path.exists() {
        return Ok(false);
    }

    let kry_modified = fs::metadata(kry_path)?.modified()?;
    let krb_modified = fs::metadata(krb_path)?.modified()?;

    Ok(krb_modified >= kry_modified)
}

fn compile_to_krb(input: &Path, output: &Path) -> Result<()> {
    // Use kryon-compiler to compile
    compile_file(
        input.to_str().context("Invalid input path")?, 
        output.to_str().context("Invalid output path")?
    ).context("Failed to compile .kry to .krb")?;

    Ok(())
}

fn run_krb_file(krb_path: &Path, cli: &Cli) -> Result<()> {
    // Select renderer
    let renderer = select_renderer(cli.renderer.clone())?;
    
    println!("{} Running with {} renderer", 
        "🚀".bright_green(),
        format!("{:?}", renderer).bright_cyan()
    );

    // Execute the renderer
    execute_renderer(&renderer, krb_path, cli)
}

fn select_renderer(preferred: Option<RendererType>) -> Result<RendererType> {
    // If a specific renderer was requested, use it
    if let Some(renderer) = preferred {
        if renderer.is_available() {
            return Ok(renderer);
        } else {
            warn!("Requested renderer {:?} is not available", renderer);
        }
    }

    // Get list of available renderers
    let available: Vec<RendererType> = [
        RendererType::Raylib,
        RendererType::Wgpu,
        RendererType::Ratatui,
        RendererType::Html,
        RendererType::Debug,
    ]
    .into_iter()
    .filter(|r| r.is_available())
    .collect();

    if available.is_empty() {
        bail!("No renderers are available. Please rebuild with renderer features enabled.");
    }

    // If only one renderer available, use it
    if available.len() == 1 {
        return Ok(available[0].clone());
    }

    // Default to WGPU if available, otherwise Raylib
    if available.contains(&RendererType::Wgpu) {
        return Ok(RendererType::Wgpu);
    }
    if available.contains(&RendererType::Raylib) {
        return Ok(RendererType::Raylib);
    }

    // Multiple renderers available, ask user to choose
    let options: Vec<String> = available
        .iter()
        .map(|r| format!("{:?} - {}", r, r.description()))
        .collect();

    let selection = Select::new("Select renderer:", options.clone())
        .prompt()
        .context("Failed to get renderer selection")?;

    // Parse selection back to RendererType
    let selected_idx = options.iter().position(|opt| opt == &selection).unwrap();
    Ok(available[selected_idx].clone())
}

fn execute_renderer(renderer: &RendererType, krb_path: &Path, cli: &Cli) -> Result<()> {
    let binary_name = renderer.binary_name();
    
    // Try to find the binary in the same directory as current executable
    let current_exe = std::env::current_exe()?;
    let exe_dir = current_exe.parent().unwrap();
    let renderer_path = exe_dir.join(binary_name);
    
    let mut cmd = if renderer_path.exists() {
        Command::new(&renderer_path)
    } else {
        // Fallback to system PATH
        Command::new(binary_name)
    };
    
    cmd.arg(krb_path);

    // Add debug flags if needed
    if cli.debug {
        cmd.env("RUST_LOG", "debug");
    } else if cli.verbose {
        cmd.env("RUST_LOG", "info");
    }

    // Execute command
    debug!("Executing: {} {}", binary_name, krb_path.display());
    
    let status = cmd.status()
        .with_context(|| format!("Failed to execute {}. Make sure the renderer is installed.", binary_name))?;

    if !status.success() {
        bail!("Renderer exited with error code: {:?}", status.code());
    }

    Ok(())
}

fn handle_command(command: Commands) -> Result<()> {
    match command {
        Commands::Run { file, renderer } => {
            let cli = Cli {
                input: Some(file.clone()),
                renderer,
                debug: false,
                verbose: false,
                compile_only: false,
                output: None,
                list_renderers: false,
                force: false,
                watch: false,
                clean: false,
                command: None,
            };
            run_krb_file(&file, &cli)
        },
        
        Commands::Compile { input, output } => {
            let cli = Cli {
                input: Some(input.clone()),
                renderer: None,
                debug: false,
                verbose: false,
                compile_only: true,
                output,
                list_renderers: false,
                force: true,
                watch: false,
                clean: false,
                command: None,
            };
            compile_kry_file(&input, &cli)?;
            Ok(())
        },
        
        Commands::Info { file } => {
            show_file_info(&file)
        },
        
        Commands::Init { name } => {
            init_project(name)
        },
        
        Commands::Dev { dev_command } => {
            handle_dev_command(dev_command)
        },
    }
}

fn show_file_info(file: &Path) -> Result<()> {
    if !file.exists() {
        bail!("File does not exist: {}", file.display());
    }

    let metadata = fs::metadata(file)?;
    let ext = file.extension().and_then(|s| s.to_str()).unwrap_or("unknown");
    
    println!("{}", "File Information:".bright_white().bold());
    println!("  Path: {}", file.display());
    println!("  Type: {}", ext);
    println!("  Size: {} bytes", metadata.len());
    println!("  Modified: {:?}", metadata.modified()?);

    match ext {
        "kry" => {
            // Analyze .kry file
            let content = fs::read_to_string(file)?;
            println!("  Lines: {}", content.lines().count());
            println!("  Characters: {}", content.chars().count());
            
            // Try to extract elements and styles
            analyze_kry_content(&content)?;
        },
        "krb" => {
            // Analyze .krb file
            let content = fs::read(file)?;
            println!("  Binary size: {} bytes", content.len());
            // TODO: Add KRB analysis
        },
        _ => {
            println!("  {}", "Unknown file type".yellow());
        }
    }

    Ok(())
}

fn analyze_kry_content(content: &str) -> Result<()> {
    // Simple analysis - count elements and styles
    let element_count = content.matches('{').count();
    let style_count = content.matches("Style ").count();
    
    println!("  Elements: ~{}", element_count);
    println!("  Styles: ~{}", style_count);
    
    Ok(())
}

fn init_project(name: Option<String>) -> Result<()> {
    let project_name = name.unwrap_or_else(|| {
        inquire::Text::new("Project name:")
            .with_default("my-kryon-app")
            .prompt()
            .unwrap_or_else(|_| "my-kryon-app".to_string())
    });

    let project_dir = PathBuf::from(&project_name);
    
    if project_dir.exists() {
        bail!("Directory {} already exists", project_name);
    }

    fs::create_dir_all(&project_dir)?;

    // Create basic project structure
    let main_kry = project_dir.join("main.kry");
    let project_content = format!("// {} - Kryon Application

App {{
    window-width: 800
    window-height: 600
    window-title: \"{}\"
    
    Container {{
        layout: \"column\"
        padding: 20
        background-color: \"#f5f5f5\"
        
        Text {{
            text: \"Welcome to Kryon!\"
            font-size: 24
            font-weight: 700
            color: \"#333\"
            text-align: \"center\"
            margin-bottom: 20
        }}
        
        Button {{
            text: \"Click Me\"
            padding: \"10px 20px\"
            background-color: \"#007acc\"
            color: \"white\"
            border-radius: 5
            margin: \"0 auto\"
            
            script {{
                function onclick() {{
                    print(\"Hello from Kryon!\")
                }}
            }}
        }}
    }}
}}
", project_name, project_name);

    fs::write(&main_kry, project_content)?;

    // Create README
    let readme_content = format!(r#"# {}

A Kryon UI application.

## Usage

```bash
# Run the application
kryon main.kry

# Compile only
kryon --compile-only main.kry

# Run with specific renderer
kryon --renderer wgpu main.kry
```

## Learn More

- [Kryon Documentation](https://docs.kryon.dev)
- [Examples](https://github.com/kryonlabs/kryon/tree/main/examples)
"#, project_name);

    fs::write(project_dir.join("README.md"), readme_content)?;

    println!("{} Created new Kryon project: {}", 
        "✓".bright_green(), 
        project_name.bright_cyan()
    );
    println!("  {}/", project_name.bright_white());
    println!("  ├── main.kry");
    println!("  └── README.md");
    println!();
    println!("Get started:");
    println!("  {} {}", "cd".bright_white(), project_name);
    println!("  {} {}", "kryon".bright_white(), "main.kry".bright_cyan());

    Ok(())
}

fn handle_dev_command(dev_command: DevCommands) -> Result<()> {
    match dev_command {
        DevCommands::Test => {
            println!("{}", "Running Kryon tests...".bright_white());
            let status = Command::new("cargo")
                .args(["test", "--workspace"])
                .status()?;
            
            if !status.success() {
                bail!("Tests failed");
            }
            println!("{} All tests passed", "✓".bright_green());
            Ok(())
        },
        
        DevCommands::Profile { file } => {
            println!("{} Profiling: {}", "📊".bright_yellow(), file.display());
            // TODO: Implement profiling
            println!("Profiling not yet implemented");
            Ok(())
        },
        
        DevCommands::Bench { renderer } => {
            println!("{} Running benchmarks", "🏁".bright_yellow());
            if let Some(r) = renderer {
                println!("  Renderer: {:?}", r);
            }
            // TODO: Implement benchmarking
            println!("Benchmarking not yet implemented");
            Ok(())
        },
    }
}