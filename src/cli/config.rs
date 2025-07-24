// FILE: src/cli/config.rs

use crate::error::{CompilerError, Result};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;

#[derive(Debug, Default, Serialize, Deserialize)]
pub struct ConfigFile {
    pub optimization_level: Option<u8>,
    pub target_platform: Option<String>,
    pub embed_scripts: Option<bool>,
    pub compress_output: Option<bool>,
    pub include_directories: Option<Vec<String>>,
    pub custom_variables: Option<HashMap<String, String>>,
    pub max_file_size: Option<u64>,
    pub output_directory: Option<String>,
}

pub fn load(config_path: &str) -> Result<ConfigFile> {
    log::info!("Loaded configuration from {}", config_path);
    let config_content = fs::read_to_string(config_path).map_err(|e| {
        CompilerError::FileNotFound {
            path: format!("Config file {}: {}", config_path, e),
        }
    })?;

    if config_path.ends_with(".json") {
        serde_json::from_str(&config_content).map_err(|e| CompilerError::InvalidFormat {
            message: format!("Invalid JSON config: {}", e),
        })
    } else if config_path.ends_with(".toml") {
        toml::from_str(&config_content).map_err(|e| CompilerError::InvalidFormat {
            message: format!("Invalid TOML config: {}", e),
        })
    } else {
        Err(CompilerError::InvalidFormat {
            message: "Config file must be .json or .toml format".to_string(),
        })
    }
}
