//! ViewProvider bundling for smart bundling
//! 
//! This module handles bundling only the ViewProviders that are actually used by the app.

use std::collections::HashSet;
use anyhow::Result;

use super::{TargetPlatform, BundleConfig};
use crate::error::CompilerError;

/// Handles bundling of ViewProviders based on actual usage
pub struct ViewProviderBundler {
    target_platform: TargetPlatform,
    available_providers: HashMap<String, ProviderInfo>,
}

#[derive(Debug, Clone)]
struct ProviderInfo {
    name: String,
    supported_platforms: Vec<TargetPlatform>,
    dependencies: Vec<String>,
    size_estimate: usize,
    is_core: bool,
}

use std::collections::HashMap;

impl ViewProviderBundler {
    pub fn new(target_platform: TargetPlatform) -> Self {
        let mut available_providers = HashMap::new();
        
        // Core providers
        available_providers.insert("canvas".to_string(), ProviderInfo {
            name: "canvas".to_string(),
            supported_platforms: vec![TargetPlatform::Desktop, TargetPlatform::Web, TargetPlatform::Mobile],
            dependencies: vec!["lua".to_string()],
            size_estimate: 30_000,
            is_core: true,
        });
        
        // Web-related providers
        available_providers.insert("webview".to_string(), ProviderInfo {
            name: "webview".to_string(),
            supported_platforms: vec![TargetPlatform::Desktop, TargetPlatform::Mobile],
            dependencies: vec!["webview_sys".to_string()],
            size_estimate: 150_000,
            is_core: false,
        });
        
        available_providers.insert("iframe".to_string(), ProviderInfo {
            name: "iframe".to_string(),
            supported_platforms: vec![TargetPlatform::Web],
            dependencies: vec![],
            size_estimate: 20_000,
            is_core: false,
        });
        
        // WASM provider
        available_providers.insert("wasm".to_string(), ProviderInfo {
            name: "wasm".to_string(),
            supported_platforms: vec![TargetPlatform::Desktop, TargetPlatform::Web],
            dependencies: vec!["wasmtime".to_string()],
            size_estimate: 200_000,
            is_core: false,
        });
        
        // Native renderer provider
        available_providers.insert("native_renderer".to_string(), ProviderInfo {
            name: "native_renderer".to_string(),
            supported_platforms: vec![TargetPlatform::Desktop, TargetPlatform::Mobile],
            dependencies: vec!["native_graphics".to_string()],
            size_estimate: 100_000,
            is_core: false,
        });
        
        // Gaming/Emulation providers
        available_providers.insert("gba".to_string(), ProviderInfo {
            name: "gba".to_string(),
            supported_platforms: vec![TargetPlatform::Desktop, TargetPlatform::Mobile],
            dependencies: vec!["gba_emulator".to_string()],
            size_estimate: 500_000,
            is_core: false,
        });
        
        available_providers.insert("dos".to_string(), ProviderInfo {
            name: "dos".to_string(),
            supported_platforms: vec![TargetPlatform::Desktop],
            dependencies: vec!["dosbox".to_string()],
            size_estimate: 800_000,
            is_core: false,
        });
        
        available_providers.insert("uxn".to_string(), ProviderInfo {
            name: "uxn".to_string(),
            supported_platforms: vec![TargetPlatform::Desktop, TargetPlatform::Mobile, TargetPlatform::Embedded],
            dependencies: vec!["uxn_vm".to_string()],
            size_estimate: 50_000,
            is_core: false,
        });
        
        // Development providers
        available_providers.insert("code_editor".to_string(), ProviderInfo {
            name: "code_editor".to_string(),
            supported_platforms: vec![TargetPlatform::Desktop, TargetPlatform::Web],
            dependencies: vec!["syntax_highlighting".to_string()],
            size_estimate: 300_000,
            is_core: false,
        });
        
        available_providers.insert("terminal".to_string(), ProviderInfo {
            name: "terminal".to_string(),
            supported_platforms: vec![TargetPlatform::Desktop],
            dependencies: vec!["pty".to_string()],
            size_estimate: 100_000,
            is_core: false,
        });
        
        available_providers.insert("model_viewer".to_string(), ProviderInfo {
            name: "model_viewer".to_string(),
            supported_platforms: vec![TargetPlatform::Desktop, TargetPlatform::Web],
            dependencies: vec!["3d_renderer".to_string()],
            size_estimate: 400_000,
            is_core: false,
        });
        
        Self {
            target_platform,
            available_providers,
        }
    }
    
    /// Bundle only the ViewProviders that are actually used
    pub fn bundle_providers(&self, used_types: &HashSet<String>) -> Result<Vec<String>> {
        let mut included_providers = Vec::new();
        let mut total_size = 0;
        
        eprintln!("[VIEW_BUNDLER] Analyzing {} used EmbedView types", used_types.len());
        
        for view_type in used_types {
            if let Some(provider_info) = self.available_providers.get(view_type) {
                // Check if provider is supported on target platform
                if provider_info.supported_platforms.contains(&self.target_platform) {
                    eprintln!("[VIEW_BUNDLER] Including provider: {} ({}KB)", 
                             provider_info.name, provider_info.size_estimate / 1024);
                    
                    included_providers.push(provider_info.name.clone());
                    total_size += provider_info.size_estimate;
                    
                    // Add dependencies
                    for dep in &provider_info.dependencies {
                        eprintln!("[VIEW_BUNDLER] Adding dependency: {}", dep);
                    }
                } else {
                    eprintln!("[VIEW_BUNDLER] Skipping provider {} - not supported on {:?}", 
                             provider_info.name, self.target_platform);
                }
            } else {
                eprintln!("[VIEW_BUNDLER] Warning: Unknown provider type: {}", view_type);
            }
        }
        
        // Always include core providers if any EmbedView is used
        if !used_types.is_empty() {
            for (name, provider_info) in &self.available_providers {
                if provider_info.is_core && !included_providers.contains(name) {
                    if provider_info.supported_platforms.contains(&self.target_platform) {
                        eprintln!("[VIEW_BUNDLER] Including core provider: {}", name);
                        included_providers.push(name.clone());
                        total_size += provider_info.size_estimate;
                    }
                }
            }
        }
        
        let all_providers_size: usize = self.available_providers.values()
            .filter(|p| p.supported_platforms.contains(&self.target_platform))
            .map(|p| p.size_estimate)
            .sum();
            
        let savings = all_providers_size - total_size;
        let savings_percent = (savings as f32 / all_providers_size as f32) * 100.0;
        
        eprintln!("[VIEW_BUNDLER] Bundle size: {}KB (saved {}KB, {:.1}% smaller)", 
                 total_size / 1024, savings / 1024, savings_percent);
        
        Ok(included_providers)
    }
    
    /// Get provider information for a specific type
    pub fn get_provider_info(&self, provider_type: &str) -> Option<&ProviderInfo> {
        self.available_providers.get(provider_type)
    }
    
    /// List all available providers for the target platform
    pub fn list_available_providers(&self) -> Vec<String> {
        self.available_providers.values()
            .filter(|p| p.supported_platforms.contains(&self.target_platform))
            .map(|p| p.name.clone())
            .collect()
    }
    
    /// Get total size of all available providers
    pub fn get_total_available_size(&self) -> usize {
        self.available_providers.values()
            .filter(|p| p.supported_platforms.contains(&self.target_platform))
            .map(|p| p.size_estimate)
            .sum()
    }
}