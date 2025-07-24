// FILE: src/cli/mod.rs

mod config;
mod handlers;

use crate::error::{CompilerError, Result};
use crate::{CompilerOptions, TargetPlatform};
use clap::{Arg, ArgAction, Command, ValueEnum};
use std::time::Instant;

#[derive(Debug, Clone, ValueEnum)]
pub enum OptimizationLevel {
    None,
    Basic,
    Aggressive,
}

#[derive(Debug, Clone, ValueEnum)]
pub enum Platform {
    Desktop,
    Mobile,
    Web,
    Embedded,
    Universal,
}

#[derive(Debug, Clone, ValueEnum)]
pub enum OutputFormat {
    Krb,
    Json,
    Debug,
}

pub struct EnhancedCli {
    config: config::ConfigFile,
    start_time: Instant,
}

impl EnhancedCli {
    pub fn new() -> Self {
        Self {
            config: config::ConfigFile::default(),
            start_time: Instant::now(),
        }
    }

    pub fn run(&mut self) -> Result<()> {
        self.start_time = Instant::now();
        let matches = self.build_cli().get_matches();

        if let Some(config_path) = matches.get_one::<String>("config") {
            self.config = config::load(config_path)?;
        }

        self.setup_logging(matches.get_count("verbose"))?;

        match matches.subcommand() {
            Some(("compile", sub_matches)) => handlers::handle_compile_command(self, sub_matches),
            Some(("check", sub_matches)) => handlers::handle_check_command(sub_matches),
            Some(("analyze", sub_matches)) => handlers::handle_analyze_command(sub_matches),
            Some(("init", sub_matches)) => handlers::handle_init_command(sub_matches),
            Some(("benchmark", sub_matches)) => handlers::handle_benchmark_command(sub_matches),
            _ => {
                println!("No subcommand specified. Use --help for usage information.");
                Ok(())
            }
        }
    }

    fn build_cli(&self) -> Command {
        Command::new(crate::NAME)
            .version(crate::VERSION)
            .about(crate::DESCRIPTION)
            .author("Kryon Development Team")
            .arg(
                Arg::new("config")
                    .short('c')
                    .long("config")
                    .value_name("FILE")
                    .help("Configuration file path")
                    .action(ArgAction::Set),
            )
            .arg(
                Arg::new("verbose")
                    .short('v')
                    .long("verbose")
                    .help("Increase verbosity (can be used multiple times)")
                    .action(ArgAction::Count),
            )
            .subcommand(
                Command::new("compile")
                    .about("Compile KRY files to KRB format")
                    .arg(Arg::new("input").help("Input KRY file").required(true).index(1))
                    .arg(Arg::new("output").short('o').long("output").value_name("FILE").help("Output KRB file"))
                    .arg(Arg::new("optimization").short('O').long("optimization").value_parser(clap::value_parser!(OptimizationLevel)).default_value("basic").help("Optimization level"))
                    .arg(Arg::new("platform").short('p').long("platform").value_parser(clap::value_parser!(Platform)).default_value("universal").help("Target platform"))
                    .arg(Arg::new("format").short('f').long("format").value_parser(clap::value_parser!(OutputFormat)).default_value("krb").help("Output format"))
                    .arg(Arg::new("embed-scripts").long("embed-scripts").help("Embed scripts inline instead of external references").action(ArgAction::SetTrue))
                    .arg(Arg::new("compress").long("compress").help("Enable output compression").action(ArgAction::SetTrue))
                    .arg(Arg::new("debug").short('d').long("debug").help("Enable debug mode with extra validation").action(ArgAction::SetTrue))
                    .arg(Arg::new("include").short('I').long("include").value_name("DIR").help("Add include directory").action(ArgAction::Append))
                    .arg(Arg::new("define").short('D').long("define").value_name("VAR=VALUE").help("Define custom variable").action(ArgAction::Append))
                    .arg(Arg::new("stats").long("stats").help("Show detailed compilation statistics").action(ArgAction::SetTrue))
                    .arg(Arg::new("watch").short('w').long("watch").help("Watch for file changes and recompile").action(ArgAction::SetTrue)),
            )
            .subcommand(
                Command::new("check")
                    .about("Check KRY files for syntax and semantic errors")
                    .arg(Arg::new("input").help("Input KRY file or directory").required(true).index(1))
                    .arg(Arg::new("recursive").short('r').long("recursive").help("Check all KRY files in directory recursively").action(ArgAction::SetTrue)),
            )
            .subcommand(
                Command::new("analyze")
                    .about("Analyze KRY/KRB files and show detailed information")
                    .arg(Arg::new("input").help("Input KRY or KRB file").required(true).index(1))
                    .arg(Arg::new("output").short('o').long("output").value_name("FILE").help("Output analysis to file"))
                    .arg(Arg::new("format").short('f').long("format").value_parser(clap::value_parser!(OutputFormat)).default_value("debug").help("Analysis output format")),
            )
            .subcommand(
                Command::new("init")
                    .about("Initialize a new Kryon project")
                    .arg(Arg::new("name").help("Project name").required(true).index(1))
                    .arg(Arg::new("template").short('t').long("template").value_name("TEMPLATE").help("Project template (simple, component, game)").default_value("simple")),
            )
            .subcommand(
                Command::new("benchmark")
                    .about("Run compilation benchmarks")
                    .arg(Arg::new("input").help("Input KRY file or directory").required(true).index(1))
                    .arg(Arg::new("iterations").short('n').long("iterations").value_name("N").help("Number of benchmark iterations").default_value("10"))
                    .arg(Arg::new("warmup").long("warmup").value_name("N").help("Number of warmup iterations").default_value("3")),
            )
    }

    fn setup_logging(&self, verbose_count: u8) -> Result<()> {
        let log_level = match verbose_count {
            0 => log::LevelFilter::Warn,
            1 => log::LevelFilter::Info,
            2 => log::LevelFilter::Debug,
            _ => log::LevelFilter::Trace,
        };
        env_logger::Builder::from_default_env()
            .filter_level(log_level)
            .format_timestamp_secs()
            .init();
        Ok(())
    }

    pub fn build_compiler_options(&self, matches: &clap::ArgMatches) -> Result<CompilerOptions> {
        let mut options = CompilerOptions::default();
        if let Some(opt_level) = matches.get_one::<OptimizationLevel>("optimization") {
            options.optimization_level = match opt_level {
                OptimizationLevel::None => 0,
                OptimizationLevel::Basic => 1,
                OptimizationLevel::Aggressive => 2,
            };
        }
        if let Some(platform) = matches.get_one::<Platform>("platform") {
            options.target_platform = match platform {
                Platform::Desktop => TargetPlatform::Desktop,
                Platform::Mobile => TargetPlatform::Mobile,
                Platform::Web => TargetPlatform::Web,
                Platform::Embedded => TargetPlatform::Embedded,
                Platform::Universal => TargetPlatform::Universal,
            };
        }
        options.debug_mode = matches.get_flag("debug");
        options.embed_scripts =
            matches.get_flag("embed-scripts") || self.config.embed_scripts.unwrap_or(false);
        options.compress_output =
            matches.get_flag("compress") || self.config.compress_output.unwrap_or(false);
        if let Some(include_dirs) = matches.get_many::<String>("include") {
            options.include_directories.extend(include_dirs.cloned());
        }
        if let Some(config_includes) = &self.config.include_directories {
            options.include_directories.extend(config_includes.clone());
        }
        if let Some(defines) = matches.get_many::<String>("define") {
            for define in defines {
                if let Some((key, value)) = define.split_once('=') {
                    options.custom_variables.insert(key.to_string(), value.to_string());
                } else {
                    return Err(CompilerError::InvalidFormat {
                        message: format!("Invalid variable definition: {}. Use VAR=VALUE format.", define),
                    });
                }
            }
        }
        if let Some(config_vars) = &self.config.custom_variables {
            for (key, value) in config_vars {
                options.custom_variables.entry(key.clone()).or_insert_with(|| value.clone());
            }
        }
        if let Some(max_size) = self.config.max_file_size {
            options.max_file_size = max_size;
        }
        Ok(options)
    }
}
