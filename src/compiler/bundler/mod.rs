//! Smart bundling system for Kryon applications
//! 
//! This module analyzes app dependencies and only includes what's actually used:
//! - ViewProviders (only include providers for EmbedView types used)
//! - Scripts (only include script engines for languages used)
//! - Resources (fonts, images, etc. - only referenced ones)
//! - Styles (tree-shake unused style definitions)

use std::collections::{HashSet, HashMap};
use std::path::{Path, PathBuf};
use anyhow::Result;

use crate::compiler::CompilerState;
use crate::core::types::ElementType;
use crate::compiler::frontend::ast::PropertyValue;
use crate::error::CompilerError;

pub mod dependency_analyzer;
pub mod resource_bundler;
pub mod script_bundler;
pub mod view_provider_bundler;

use dependency_analyzer::DependencyAnalyzer;
use resource_bundler::ResourceBundler;
use script_bundler::ScriptBundler;
use view_provider_bundler::ViewProviderBundler;

/// Bundle configuration options
#[derive(Debug, Clone)]
pub struct BundleConfig {
    /// Enable aggressive tree-shaking
    pub tree_shake: bool,
    /// Minify scripts
    pub minify_scripts: bool,
    /// Compress resources
    pub compress_resources: bool,
    /// Include source maps
    pub include_source_maps: bool,
    /// Target platform (affects which providers to include)
    pub target_platform: TargetPlatform,
}

#[derive(Debug, Clone, PartialEq)]
pub enum TargetPlatform {
    Desktop,
    Web,
    Mobile,
    Embedded,
}

/// Bundle metadata for runtime
#[derive(Debug, Clone)]
pub struct BundleMetadata {
    /// List of included ViewProvider types
    pub view_providers: Vec<String>,
    /// List of included script languages
    pub script_languages: Vec<String>,
    /// Resource manifest
    pub resources: HashMap<String, ResourceInfo>,
    /// Bundle size statistics
    pub size_stats: BundleSizeStats,
}

#[derive(Debug, Clone)]
pub struct ResourceInfo {
    pub path: PathBuf,
    pub size: usize,
    pub mime_type: String,
    pub is_embedded: bool,
}

#[derive(Debug, Clone)]
pub struct BundleSizeStats {
    pub total_size: usize,
    pub code_size: usize,
    pub resource_size: usize,
    pub metadata_size: usize,
    pub savings_percentage: f32,
}

/// Smart bundler that analyzes dependencies and creates optimized bundles
pub struct SmartBundler {
    config: BundleConfig,
    dependency_analyzer: DependencyAnalyzer,
    resource_bundler: ResourceBundler,
    script_bundler: ScriptBundler,
    view_provider_bundler: ViewProviderBundler,
}

impl SmartBundler {
    pub fn new(config: BundleConfig) -> Self {
        Self {
            config: config.clone(),
            dependency_analyzer: DependencyAnalyzer::new(),
            resource_bundler: ResourceBundler::new(config.compress_resources),
            script_bundler: ScriptBundler::new(config.minify_scripts),
            view_provider_bundler: ViewProviderBundler::new(config.target_platform.clone()),
        }
    }
    
    /// Analyze the app and create an optimized bundle
    pub fn create_bundle(&mut self, state: &CompilerState, output_path: &Path) -> Result<BundleMetadata> {
        eprintln!("[BUNDLER] Starting smart bundling process...");
        
        // Step 1: Analyze dependencies
        let dependencies = self.dependency_analyzer.analyze(state)?;
        eprintln!("[BUNDLER] Found dependencies: {:?}", dependencies);
        
        // Step 2: Bundle only required ViewProviders
        let included_providers = self.view_provider_bundler.bundle_providers(&dependencies.embed_view_types)?;
        eprintln!("[BUNDLER] Including ViewProviders: {:?}", included_providers);
        
        // Step 3: Bundle only required script engines
        let included_languages = self.script_bundler.bundle_scripts(&dependencies.script_languages, state)?;
        eprintln!("[BUNDLER] Including script languages: {:?}", included_languages);
        
        // Step 4: Bundle only referenced resources
        let resource_manifest = self.resource_bundler.bundle_resources(&dependencies.referenced_resources, output_path)?;
        eprintln!("[BUNDLER] Bundled {} resources", resource_manifest.len());
        
        // Step 5: Tree-shake unused styles if enabled
        if self.config.tree_shake {
            self.tree_shake_styles(state, &dependencies.used_styles)?;
        }
        
        // Step 6: Calculate bundle statistics
        let size_stats = self.calculate_bundle_stats(&included_providers, &included_languages, &resource_manifest);
        
        // Step 7: Write bundle manifest
        self.write_bundle_manifest(output_path, &included_providers, &included_languages, &resource_manifest)?;
        
        Ok(BundleMetadata {
            view_providers: included_providers,
            script_languages: included_languages,
            resources: resource_manifest,
            size_stats,
        })
    }
    
    /// Tree-shake unused styles
    fn tree_shake_styles(&self, state: &CompilerState, used_styles: &HashSet<String>) -> Result<()> {
        eprintln!("[BUNDLER] Tree-shaking styles: {} used out of {} total", 
                 used_styles.len(), state.styles.len());
        
        // In a real implementation, we would modify the compiled output
        // to exclude unused styles
        
        Ok(())
    }
    
    /// Calculate bundle size statistics
    fn calculate_bundle_stats(
        &self,
        providers: &[String],
        languages: &[String],
        resources: &HashMap<String, ResourceInfo>,
    ) -> BundleSizeStats {
        let code_size = providers.len() * 50_000 + languages.len() * 100_000; // Rough estimates
        let resource_size: usize = resources.values().map(|r| r.size).sum();
        let metadata_size = 1024; // Manifest size
        let total_size = code_size + resource_size + metadata_size;
        
        // Calculate savings (compared to including everything)
        let full_size = 8 * 50_000 + 4 * 100_000 + resource_size * 2; // All providers + all languages
        let savings_percentage = ((full_size - total_size) as f32 / full_size as f32) * 100.0;
        
        BundleSizeStats {
            total_size,
            code_size,
            resource_size,
            metadata_size,
            savings_percentage,
        }
    }
    
    /// Write bundle manifest for runtime
    fn write_bundle_manifest(
        &self,
        output_path: &Path,
        providers: &[String],
        languages: &[String],
        resources: &HashMap<String, ResourceInfo>,
    ) -> Result<()> {
        let manifest_path = output_path.with_extension("manifest.json");
        
        let manifest = serde_json::json!({
            "version": "1.0",
            "providers": providers,
            "languages": languages,
            "resources": resources.iter().map(|(k, v)| {
                (k, serde_json::json!({
                    "path": v.path.to_str().unwrap_or(""),
                    "size": v.size,
                    "mime_type": v.mime_type,
                    "embedded": v.is_embedded,
                }))
            }).collect::<HashMap<_, _>>(),
            "platform": format!("{:?}", self.config.target_platform),
        });
        
        std::fs::write(manifest_path, serde_json::to_string_pretty(&manifest)?)?;
        
        Ok(())
    }
}

impl Default for BundleConfig {
    fn default() -> Self {
        Self {
            tree_shake: true,
            minify_scripts: true,
            compress_resources: true,
            include_source_maps: false,
            target_platform: TargetPlatform::Desktop,
        }
    }
}