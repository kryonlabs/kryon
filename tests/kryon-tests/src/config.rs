//! Test Configuration Management
//! 
//! Centralized configuration system for all Kryon tests

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::path::PathBuf;
use std::time::Duration;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestSuiteConfig {
    pub general: GeneralConfig,
    pub renderers: HashMap<String, RendererConfig>,
    pub test_suites: HashMap<String, TestSuiteDefinition>,
    pub performance: PerformanceConfig,
    pub quality: QualityConfig,
    pub reporting: ReportingConfig,
    pub ci: CiConfig,
    pub environments: HashMap<String, EnvironmentConfig>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GeneralConfig {
    pub name: String,
    pub version: String,
    pub description: String,
    pub default_renderer: String,
    pub default_window_width: u32,
    pub default_window_height: u32,
    pub enable_logging: bool,
    pub log_level: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RendererConfig {
    pub binary: String,
    pub enabled: bool,
    pub supports_input: bool,
    pub supports_animation: bool,
    pub supports_transparency: bool,
    pub requires_gpu: Option<bool>,
    pub requires_browser: Option<bool>,
    pub terminal_only: Option<bool>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestSuiteDefinition {
    pub file: String,
    pub description: String,
    pub timeout_seconds: u64,
    pub required_renderers: Vec<String>,
    pub skip_renderers: Option<Vec<String>>,
    pub environment: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PerformanceConfig {
    pub benchmark_duration_seconds: u64,
    pub element_count_levels: Vec<usize>,
    pub stress_test_iterations: usize,
    pub memory_sampling_interval_ms: u64,
    pub min_fps: f64,
    pub max_memory_mb: usize,
    pub max_cpu_percent: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct QualityConfig {
    pub check_warnings: bool,
    pub check_formatting: bool,
    pub check_clippy: bool,
    pub fail_on_warnings: bool,
    pub fail_on_formatting: bool,
    pub fail_on_clippy: bool,
    pub min_code_coverage: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ReportingConfig {
    pub generate_html_report: bool,
    pub generate_json_report: bool,
    pub generate_csv_metrics: bool,
    pub save_screenshots: bool,
    pub include_performance_graphs: bool,
    pub keep_reports_days: u32,
    pub max_report_files: usize,
    pub output_directory: PathBuf,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CiConfig {
    pub parallel_execution: bool,
    pub fail_fast: bool,
    pub timeout_multiplier: f64,
    pub upload_artifacts: bool,
    pub max_parallel_jobs: Option<usize>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EnvironmentConfig {
    pub log_level: String,
    pub enable_debug_renderer: bool,
    pub show_layout_bounds: bool,
    pub enable_performance_overlay: bool,
    pub custom_env_vars: Option<HashMap<String, String>>,
}

impl Default for TestSuiteConfig {
    fn default() -> Self {
        Self {
            general: GeneralConfig {
                name: "Kryon Comprehensive Test Suite".to_string(),
                version: "1.0.0".to_string(),
                description: "Centralized testing ground for the Kryon UI framework".to_string(),
                default_renderer: "ratatui".to_string(),
                default_window_width: 800,
                default_window_height: 600,
                enable_logging: true,
                log_level: "INFO".to_string(),
            },
            renderers: create_default_renderers(),
            test_suites: create_default_test_suites(),
            performance: PerformanceConfig {
                benchmark_duration_seconds: 60,
                element_count_levels: vec![50, 100, 200, 500, 1000],
                stress_test_iterations: 100,
                memory_sampling_interval_ms: 1000,
                min_fps: 30.0,
                max_memory_mb: 500,
                max_cpu_percent: 80.0,
            },
            quality: QualityConfig {
                check_warnings: true,
                check_formatting: true,
                check_clippy: true,
                fail_on_warnings: false,
                fail_on_formatting: false,
                fail_on_clippy: false,
                min_code_coverage: 80.0,
            },
            reporting: ReportingConfig {
                generate_html_report: true,
                generate_json_report: true,
                generate_csv_metrics: true,
                save_screenshots: false,
                include_performance_graphs: true,
                keep_reports_days: 30,
                max_report_files: 100,
                output_directory: PathBuf::from("target/test-reports"),
            },
            ci: CiConfig {
                parallel_execution: true,
                fail_fast: false,
                timeout_multiplier: 1.5,
                upload_artifacts: true,
                max_parallel_jobs: Some(4),
            },
            environments: create_default_environments(),
        }
    }
}

fn create_default_renderers() -> HashMap<String, RendererConfig> {
    let mut renderers = HashMap::new();
    
    renderers.insert("html".to_string(), RendererConfig {
        binary: "kryon-html".to_string(),
        enabled: true,
        supports_input: true,
        supports_animation: true,
        supports_transparency: true,
        requires_gpu: None,
        requires_browser: Some(true),
        terminal_only: None,
    });
    
    renderers.insert("raylib".to_string(), RendererConfig {
        binary: "kryon-raylib".to_string(),
        enabled: true,
        supports_input: true,
        supports_animation: true,
        supports_transparency: true,
        requires_gpu: Some(false),
        requires_browser: None,
        terminal_only: None,
    });
    
    renderers.insert("ratatui".to_string(), RendererConfig {
        binary: "kryon-ratatui".to_string(),
        enabled: true,
        supports_input: true,
        supports_animation: false,
        supports_transparency: false,
        requires_gpu: None,
        requires_browser: None,
        terminal_only: Some(true),
    });
    
    renderers.insert("wgpu".to_string(), RendererConfig {
        binary: "kryon-wgpu".to_string(),
        enabled: true,
        supports_input: true,
        supports_animation: true,
        supports_transparency: true,
        requires_gpu: Some(true),
        requires_browser: None,
        terminal_only: None,
    });
    
    renderers.insert("sdl2".to_string(), RendererConfig {
        binary: "kryon-sdl2".to_string(),
        enabled: true,
        supports_input: true,
        supports_animation: true,
        supports_transparency: true,
        requires_gpu: Some(false),
        requires_browser: None,
        terminal_only: None,
    });
    
    renderers
}

fn create_default_test_suites() -> HashMap<String, TestSuiteDefinition> {
    let mut suites = HashMap::new();
    
    suites.insert("core_components".to_string(), TestSuiteDefinition {
        file: "01_core_components.kry".to_string(),
        description: "Basic UI components and functionality".to_string(),
        timeout_seconds: 30,
        required_renderers: vec!["html".to_string(), "ratatui".to_string()],
        skip_renderers: None,
        environment: Some("testing".to_string()),
    });
    
    suites.insert("input_fields".to_string(), TestSuiteDefinition {
        file: "02_input_fields.kry".to_string(),
        description: "Comprehensive input field testing".to_string(),
        timeout_seconds: 45,
        required_renderers: vec!["html".to_string(), "wgpu".to_string()],
        skip_renderers: Some(vec!["ratatui".to_string()]),
        environment: Some("testing".to_string()),
    });
    
    suites.insert("layout_engine".to_string(), TestSuiteDefinition {
        file: "03_layout_engine.kry".to_string(),
        description: "Layout and flexbox system testing".to_string(),
        timeout_seconds: 30,
        required_renderers: vec!["html".to_string(), "ratatui".to_string()],
        skip_renderers: None,
        environment: Some("testing".to_string()),
    });
    
    suites.insert("renderer_comparison".to_string(), TestSuiteDefinition {
        file: "04_renderer_comparison.kry".to_string(),
        description: "Cross-renderer consistency testing".to_string(),
        timeout_seconds: 60,
        required_renderers: vec!["html".to_string(), "ratatui".to_string(), "wgpu".to_string()],
        skip_renderers: None,
        environment: Some("testing".to_string()),
    });
    
    suites.insert("performance_benchmark".to_string(), TestSuiteDefinition {
        file: "05_performance_benchmark.kry".to_string(),
        description: "Performance and stress testing".to_string(),
        timeout_seconds: 120,
        required_renderers: vec!["wgpu".to_string()],
        skip_renderers: Some(vec!["ratatui".to_string()]),
        environment: Some("performance".to_string()),
    });
    
    suites.insert("playground_e2e".to_string(), TestSuiteDefinition {
        file: "playground_test.kry".to_string(),
        description: "End-to-end playground functionality".to_string(),
        timeout_seconds: 180,
        required_renderers: vec!["html".to_string()],
        skip_renderers: None,
        environment: Some("e2e".to_string()),
    });
    
    suites
}

fn create_default_environments() -> HashMap<String, EnvironmentConfig> {
    let mut environments = HashMap::new();
    
    environments.insert("development".to_string(), EnvironmentConfig {
        log_level: "DEBUG".to_string(),
        enable_debug_renderer: true,
        show_layout_bounds: true,
        enable_performance_overlay: true,
        custom_env_vars: Some(HashMap::from([
            ("RUST_BACKTRACE".to_string(), "full".to_string()),
            ("KRYON_DEBUG".to_string(), "1".to_string()),
        ])),
    });
    
    environments.insert("testing".to_string(), EnvironmentConfig {
        log_level: "INFO".to_string(),
        enable_debug_renderer: false,
        show_layout_bounds: false,
        enable_performance_overlay: false,
        custom_env_vars: Some(HashMap::from([
            ("RUST_BACKTRACE".to_string(), "1".to_string()),
            ("KRYON_TEST_MODE".to_string(), "1".to_string()),
        ])),
    });
    
    environments.insert("performance".to_string(), EnvironmentConfig {
        log_level: "WARN".to_string(),
        enable_debug_renderer: false,
        show_layout_bounds: false,
        enable_performance_overlay: true,
        custom_env_vars: Some(HashMap::from([
            ("KRYON_BENCHMARK_MODE".to_string(), "1".to_string()),
        ])),
    });
    
    environments.insert("e2e".to_string(), EnvironmentConfig {
        log_level: "INFO".to_string(),
        enable_debug_renderer: false,
        show_layout_bounds: false,
        enable_performance_overlay: false,
        custom_env_vars: Some(HashMap::from([
            ("KRYON_E2E_MODE".to_string(), "1".to_string()),
            ("HEADLESS".to_string(), "1".to_string()),
        ])),
    });
    
    environments.insert("ci".to_string(), EnvironmentConfig {
        log_level: "INFO".to_string(),
        enable_debug_renderer: false,
        show_layout_bounds: false,
        enable_performance_overlay: false,
        custom_env_vars: Some(HashMap::from([
            ("CI".to_string(), "true".to_string()),
            ("RUST_BACKTRACE".to_string(), "1".to_string()),
        ])),
    });
    
    environments
}

impl TestSuiteConfig {
    /// Load configuration from TOML file
    pub fn load_from_file<P: AsRef<std::path::Path>>(path: P) -> crate::Result<Self> {
        let content = std::fs::read_to_string(path)?;
        let config: TestSuiteConfig = toml::from_str(&content)?;
        Ok(config)
    }
    
    /// Save configuration to TOML file
    pub fn save_to_file<P: AsRef<std::path::Path>>(&self, path: P) -> crate::Result<()> {
        let content = toml::to_string_pretty(self)?;
        std::fs::write(path, content)?;
        Ok(())
    }
    
    /// Get renderer configuration by name
    pub fn get_renderer(&self, name: &str) -> Option<&RendererConfig> {
        self.renderers.get(name)
    }
    
    /// Get test suite definition by name
    pub fn get_test_suite(&self, name: &str) -> Option<&TestSuiteDefinition> {
        self.test_suites.get(name)
    }
    
    /// Get environment configuration by name
    pub fn get_environment(&self, name: &str) -> Option<&EnvironmentConfig> {
        self.environments.get(name)
    }
    
    /// Get enabled renderers
    pub fn enabled_renderers(&self) -> Vec<&String> {
        self.renderers
            .iter()
            .filter(|(_, config)| config.enabled)
            .map(|(name, _)| name)
            .collect()
    }
    
    /// Check if a renderer supports a specific feature
    pub fn renderer_supports_feature(&self, renderer: &str, feature: RendererFeature) -> bool {
        if let Some(config) = self.get_renderer(renderer) {
            match feature {
                RendererFeature::Input => config.supports_input,
                RendererFeature::Animation => config.supports_animation,
                RendererFeature::Transparency => config.supports_transparency,
                RendererFeature::Gpu => config.requires_gpu.unwrap_or(false),
                RendererFeature::Browser => config.requires_browser.unwrap_or(false),
                RendererFeature::Terminal => config.terminal_only.unwrap_or(false),
            }
        } else {
            false
        }
    }
    
    /// Get timeout for a test suite with environment multiplier
    pub fn get_test_timeout(&self, suite_name: &str) -> Duration {
        if let Some(suite) = self.get_test_suite(suite_name) {
            let base_timeout = Duration::from_secs(suite.timeout_seconds);
            
            // Apply CI timeout multiplier if in CI environment
            if std::env::var("CI").is_ok() {
                let multiplier = self.ci.timeout_multiplier;
                Duration::from_secs_f64(base_timeout.as_secs_f64() * multiplier)
            } else {
                base_timeout
            }
        } else {
            Duration::from_secs(300) // Default 5 minutes
        }
    }
    
    /// Validate configuration
    pub fn validate(&self) -> crate::Result<()> {
        // Check that default renderer exists and is enabled
        if !self.renderers.contains_key(&self.general.default_renderer) {
            return Err(crate::Error::Config(format!(
                "Default renderer '{}' not found in renderer configurations",
                self.general.default_renderer
            )));
        }
        
        let default_renderer = &self.renderers[&self.general.default_renderer];
        if !default_renderer.enabled {
            return Err(crate::Error::Config(format!(
                "Default renderer '{}' is disabled",
                self.general.default_renderer
            )));
        }
        
        // Check that test suites reference valid renderers
        for (suite_name, suite) in &self.test_suites {
            for renderer in &suite.required_renderers {
                if !self.renderers.contains_key(renderer) {
                    return Err(crate::Error::Config(format!(
                        "Test suite '{}' references unknown renderer '{}'",
                        suite_name, renderer
                    )));
                }
            }
            
            if let Some(skip_renderers) = &suite.skip_renderers {
                for renderer in skip_renderers {
                    if !self.renderers.contains_key(renderer) {
                        return Err(crate::Error::Config(format!(
                            "Test suite '{}' references unknown renderer '{}' in skip list",
                            suite_name, renderer
                        )));
                    }
                }
            }
            
            if let Some(env_name) = &suite.environment {
                if !self.environments.contains_key(env_name) {
                    return Err(crate::Error::Config(format!(
                        "Test suite '{}' references unknown environment '{}'",
                        suite_name, env_name
                    )));
                }
            }
        }
        
        Ok(())
    }
}

#[derive(Debug, Clone, Copy)]
pub enum RendererFeature {
    Input,
    Animation,
    Transparency,
    Gpu,
    Browser,
    Terminal,
}

/// Test execution context that combines configuration with runtime state
#[derive(Debug, Clone)]
pub struct TestContext {
    pub config: TestSuiteConfig,
    pub current_suite: Option<String>,
    pub current_renderer: Option<String>,
    pub current_environment: Option<String>,
    pub start_time: std::time::Instant,
    pub temp_dir: PathBuf,
    pub output_dir: PathBuf,
}

impl TestContext {
    pub fn new(config: TestSuiteConfig) -> crate::Result<Self> {
        let temp_dir = std::env::temp_dir().join(format!("kryon-tests-{}", uuid::Uuid::new_v4()));
        let output_dir = config.reporting.output_directory.clone();
        
        std::fs::create_dir_all(&temp_dir)?;
        std::fs::create_dir_all(&output_dir)?;
        
        Ok(Self {
            config,
            current_suite: None,
            current_renderer: None,
            current_environment: None,
            start_time: std::time::Instant::now(),
            temp_dir,
            output_dir,
        })
    }
    
    pub fn with_suite(&mut self, suite_name: String) -> &mut Self {
        self.current_suite = Some(suite_name);
        self
    }
    
    pub fn with_renderer(&mut self, renderer_name: String) -> &mut Self {
        self.current_renderer = Some(renderer_name);
        self
    }
    
    pub fn with_environment(&mut self, env_name: String) -> &mut Self {
        self.current_environment = Some(env_name);
        self
    }
    
    pub fn apply_environment(&self) -> crate::Result<()> {
        if let Some(env_name) = &self.current_environment {
            if let Some(env_config) = self.config.get_environment(env_name) {
                // Set log level
                std::env::set_var("RUST_LOG", &env_config.log_level);
                
                // Set custom environment variables
                if let Some(custom_vars) = &env_config.custom_env_vars {
                    for (key, value) in custom_vars {
                        std::env::set_var(key, value);
                    }
                }
                
                // Set debug flags
                if env_config.enable_debug_renderer {
                    std::env::set_var("KRYON_DEBUG_RENDERER", "1");
                }
                
                if env_config.show_layout_bounds {
                    std::env::set_var("KRYON_SHOW_LAYOUT_BOUNDS", "1");
                }
                
                if env_config.enable_performance_overlay {
                    std::env::set_var("KRYON_PERFORMANCE_OVERLAY", "1");
                }
            }
        }
        
        Ok(())
    }
}

impl Drop for TestContext {
    fn drop(&mut self) {
        // Clean up temporary directory
        let _ = std::fs::remove_dir_all(&self.temp_dir);
    }
}