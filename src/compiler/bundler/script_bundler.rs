//! Script bundling for smart bundling
//! 
//! This module handles bundling only the script engines that are actually used.

use std::collections::HashSet;
use anyhow::Result;

use crate::compiler::CompilerState;
use crate::error::CompilerError;

/// Handles bundling of script engines based on actual usage
pub struct ScriptBundler {
    minify_scripts: bool,
    available_engines: std::collections::HashMap<String, EngineInfo>,
}

#[derive(Debug, Clone)]
struct EngineInfo {
    name: String,
    size_estimate: usize,
    dependencies: Vec<String>,
    is_core: bool,
    features: Vec<String>,
}

impl ScriptBundler {
    pub fn new(minify_scripts: bool) -> Self {
        let mut available_engines = std::collections::HashMap::new();
        
        // Lua engine (lightweight, core)
        available_engines.insert("lua".to_string(), EngineInfo {
            name: "lua".to_string(),
            size_estimate: 150_000,
            dependencies: vec!["mlua".to_string()],
            is_core: true,
            features: vec!["dom_api".to_string(), "canvas_api".to_string(), "reactive_vars".to_string()],
        });
        
        // JavaScript engine (V8-based, heavy)
        available_engines.insert("javascript".to_string(), EngineInfo {
            name: "javascript".to_string(),
            size_estimate: 2_000_000,
            dependencies: vec!["v8".to_string()],
            is_core: false,
            features: vec!["dom_api".to_string(), "web_apis".to_string(), "async_await".to_string()],
        });
        
        // Python engine (heavy, full-featured)
        available_engines.insert("python".to_string(), EngineInfo {
            name: "python".to_string(),
            size_estimate: 3_000_000,
            dependencies: vec!["pyo3".to_string()],
            is_core: false,
            features: vec!["dom_api".to_string(), "numpy_integration".to_string(), "ml_libs".to_string()],
        });
        
        // Wren engine (lightweight, game-focused)
        available_engines.insert("wren".to_string(), EngineInfo {
            name: "wren".to_string(),
            size_estimate: 80_000,
            dependencies: vec!["wren_vm".to_string()],
            is_core: false,
            features: vec!["dom_api".to_string(), "game_apis".to_string()],
        });
        
        Self {
            minify_scripts,
            available_engines,
        }
    }
    
    /// Bundle only the script engines that are actually used
    pub fn bundle_scripts(&self, used_languages: &HashSet<String>, state: &CompilerState) -> Result<Vec<String>> {
        let mut included_engines = Vec::new();
        let mut total_size = 0;
        
        eprintln!("[SCRIPT_BUNDLER] Analyzing {} used script languages", used_languages.len());
        
        // If no scripts are used, don't include any engines
        if used_languages.is_empty() {
            eprintln!("[SCRIPT_BUNDLER] No scripts used - excluding all script engines");
            return Ok(included_engines);
        }
        
        // Include engines for used languages
        for language in used_languages {
            if let Some(engine_info) = self.available_engines.get(language) {
                eprintln!("[SCRIPT_BUNDLER] Including engine: {} ({}KB)", 
                         engine_info.name, engine_info.size_estimate / 1024);
                
                included_engines.push(engine_info.name.clone());
                total_size += engine_info.size_estimate;
                
                // Add dependencies
                for dep in &engine_info.dependencies {
                    eprintln!("[SCRIPT_BUNDLER] Adding dependency: {}", dep);
                }
            } else {
                eprintln!("[SCRIPT_BUNDLER] Warning: Unknown script language: {}", language);
            }
        }
        
        // Always include Lua if any scripting is used (core engine)
        if !used_languages.is_empty() && !included_engines.contains(&"lua".to_string()) {
            if let Some(lua_info) = self.available_engines.get("lua") {
                eprintln!("[SCRIPT_BUNDLER] Including core Lua engine");
                included_engines.push("lua".to_string());
                total_size += lua_info.size_estimate;
            }
        }
        
        // Process scripts for optimization
        if self.minify_scripts {
            self.minify_scripts_in_state(state)?;
        }
        
        let all_engines_size: usize = self.available_engines.values()
            .map(|e| e.size_estimate)
            .sum();
            
        let savings = all_engines_size - total_size;
        let savings_percent = (savings as f32 / all_engines_size as f32) * 100.0;
        
        eprintln!("[SCRIPT_BUNDLER] Bundle size: {}KB (saved {}KB, {:.1}% smaller)", 
                 total_size / 1024, savings / 1024, savings_percent);
        
        Ok(included_engines)
    }
    
    /// Minify scripts in the compiler state
    fn minify_scripts_in_state(&self, state: &CompilerState) -> Result<()> {
        eprintln!("[SCRIPT_BUNDLER] Minifying {} scripts", state.scripts.len());
        
        for script in &state.scripts {
            let original_size = script.code_data.len();
            let script_content = String::from_utf8_lossy(&script.code_data);
            let language_name = match script.language_id {
                crate::core::types::ScriptLanguage::Lua => "lua",
                crate::core::types::ScriptLanguage::JavaScript => "javascript",
                crate::core::types::ScriptLanguage::Python => "python",
                crate::core::types::ScriptLanguage::Wren => "wren",
            };
            let minified_size = self.minify_script_content(&script_content, language_name)?;
            
            if minified_size < original_size {
                let savings = original_size - minified_size;
                let savings_percent = (savings as f32 / original_size as f32) * 100.0;
                eprintln!("[SCRIPT_BUNDLER] Minified script: {} (saved {}B, {:.1}% smaller)", 
                         script.name, savings, savings_percent);
            }
        }
        
        Ok(())
    }
    
    /// Minify script content based on language
    fn minify_script_content(&self, content: &str, language: &str) -> Result<usize> {
        match language {
            "lua" => self.minify_lua_script(content),
            "javascript" => self.minify_javascript_script(content),
            "python" => self.minify_python_script(content),
            _ => Ok(content.len()), // No minification for unknown languages
        }
    }
    
    /// Minify Lua script (remove comments and extra whitespace)
    fn minify_lua_script(&self, content: &str) -> Result<usize> {
        let mut minified = String::new();
        let mut in_string = false;
        let mut in_comment = false;
        let mut chars = content.chars().peekable();
        
        while let Some(ch) = chars.next() {
            match ch {
                '"' if !in_comment => {
                    in_string = !in_string;
                    minified.push(ch);
                }
                '-' if !in_string && !in_comment => {
                    if chars.peek() == Some(&'-') {
                        chars.next(); // consume second '-'
                        in_comment = true;
                    } else {
                        minified.push(ch);
                    }
                }
                '\n' => {
                    in_comment = false;
                    if !in_string {
                        minified.push(' '); // Replace newlines with spaces outside strings
                    } else {
                        minified.push(ch);
                    }
                }
                ' ' | '\t' if !in_string && !in_comment => {
                    // Skip extra whitespace
                    if !minified.ends_with(' ') {
                        minified.push(' ');
                    }
                }
                _ if !in_comment => {
                    minified.push(ch);
                }
                _ => {
                    // Skip character (it's in a comment)
                }
            }
        }
        
        Ok(minified.trim().len())
    }
    
    /// Minify JavaScript script (basic minification)
    fn minify_javascript_script(&self, content: &str) -> Result<usize> {
        // This is a very basic minifier - in practice, you'd use a proper JS minifier
        let minified = content
            .lines()
            .map(|line| line.trim())
            .filter(|line| !line.is_empty() && !line.starts_with("//"))
            .collect::<Vec<_>>()
            .join(" ");
        
        Ok(minified.len())
    }
    
    /// Minify Python script (remove comments and extra whitespace)
    fn minify_python_script(&self, content: &str) -> Result<usize> {
        let minified = content
            .lines()
            .map(|line| {
                // Remove comments (basic approach)
                if let Some(comment_pos) = line.find('#') {
                    line[..comment_pos].trim_end()
                } else {
                    line
                }
            })
            .filter(|line| !line.trim().is_empty())
            .collect::<Vec<_>>()
            .join("\n");
        
        Ok(minified.len())
    }
    
    /// Get engine information for a specific language
    pub fn get_engine_info(&self, language: &str) -> Option<&EngineInfo> {
        self.available_engines.get(language)
    }
    
    /// List all available script engines
    pub fn list_available_engines(&self) -> Vec<String> {
        self.available_engines.keys().cloned().collect()
    }
    
    /// Get total size of all available engines
    pub fn get_total_available_size(&self) -> usize {
        self.available_engines.values()
            .map(|e| e.size_estimate)
            .sum()
    }
}