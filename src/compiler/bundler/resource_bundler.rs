//! Resource bundling for smart bundling
//! 
//! This module handles bundling only the resources that are actually referenced.

use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::fs;
use anyhow::Result;

use super::{ResourceInfo, dependency_analyzer::ResourceReference};
use crate::error::CompilerError;

/// Handles bundling of resources based on actual usage
pub struct ResourceBundler {
    compress_resources: bool,
}

impl ResourceBundler {
    pub fn new(compress_resources: bool) -> Self {
        Self {
            compress_resources,
        }
    }
    
    /// Bundle only the resources that are actually referenced
    pub fn bundle_resources(
        &self,
        referenced_resources: &HashMap<String, ResourceReference>,
        output_path: &Path,
    ) -> Result<HashMap<String, ResourceInfo>> {
        let mut resource_manifest = HashMap::new();
        let mut total_size = 0;
        let mut compressed_size = 0;
        
        eprintln!("[RESOURCE_BUNDLER] Bundling {} referenced resources", referenced_resources.len());
        
        let resources_dir = output_path.parent().unwrap_or(Path::new(".")).join("resources");
        fs::create_dir_all(&resources_dir)?;
        
        for (resource_key, resource_ref) in referenced_resources {
            let resource_info = self.process_resource(resource_ref, &resources_dir)?;
            
            total_size += resource_info.size;
            if self.compress_resources {
                compressed_size += resource_info.size; // Placeholder for actual compression
            }
            
            eprintln!("[RESOURCE_BUNDLER] Bundled resource: {} ({}KB, used by: {:?})", 
                     resource_key, resource_info.size / 1024, resource_ref.used_by);
            
            resource_manifest.insert(resource_key.clone(), resource_info);
        }
        
        if self.compress_resources && total_size > 0 {
            let compression_ratio = (compressed_size as f32 / total_size as f32) * 100.0;
            eprintln!("[RESOURCE_BUNDLER] Compressed resources: {}KB -> {}KB ({:.1}%)", 
                     total_size / 1024, compressed_size / 1024, compression_ratio);
        } else {
            eprintln!("[RESOURCE_BUNDLER] Total resource size: {}KB", total_size / 1024);
        }
        
        Ok(resource_manifest)
    }
    
    /// Process a single resource
    fn process_resource(&self, resource_ref: &ResourceReference, resources_dir: &Path) -> Result<ResourceInfo> {
        let source_path = Path::new(&resource_ref.path);
        let resource_name = source_path.file_name()
            .and_then(|n| n.to_str())
            .unwrap_or("unknown");
        
        let output_path = resources_dir.join(resource_name);
        
        // Check if resource exists
        if source_path.exists() {
            // Copy resource to bundle directory
            fs::copy(source_path, &output_path)?;
            
            let metadata = fs::metadata(&output_path)?;
            let size = metadata.len() as usize;
            
            // Compress if enabled
            let final_size = if self.compress_resources {
                self.compress_resource(&output_path, &resource_ref.resource_type)?
            } else {
                size
            };
            
            Ok(ResourceInfo {
                path: output_path,
                size: final_size,
                mime_type: self.get_mime_type(&resource_ref.resource_type),
                is_embedded: true,
            })
        } else {
            // Resource doesn't exist - create placeholder or warning
            eprintln!("[RESOURCE_BUNDLER] Warning: Resource not found: {}", resource_ref.path);
            
            // Create a placeholder file
            let placeholder_content = format!("Resource not found: {}", resource_ref.path);
            fs::write(&output_path, placeholder_content.as_bytes())?;
            
            Ok(ResourceInfo {
                path: output_path,
                size: placeholder_content.len(),
                mime_type: "text/plain".to_string(),
                is_embedded: true,
            })
        }
    }
    
    /// Compress a resource based on its type
    fn compress_resource(&self, path: &Path, resource_type: &super::dependency_analyzer::ResourceType) -> Result<usize> {
        use super::dependency_analyzer::ResourceType;
        
        match resource_type {
            ResourceType::Image => self.compress_image(path),
            ResourceType::Audio => self.compress_audio(path),
            ResourceType::Video => self.compress_video(path),
            ResourceType::Font => self.compress_font(path),
            ResourceType::Script => self.compress_script(path),
            ResourceType::WasmModule => self.compress_wasm(path),
            _ => {
                // Generic compression for other types
                self.compress_generic(path)
            }
        }
    }
    
    /// Compress an image resource
    fn compress_image(&self, path: &Path) -> Result<usize> {
        // In a real implementation, you'd use image compression libraries
        // For now, just return the original size
        let metadata = fs::metadata(path)?;
        Ok(metadata.len() as usize)
    }
    
    /// Compress an audio resource
    fn compress_audio(&self, path: &Path) -> Result<usize> {
        // In a real implementation, you'd use audio compression
        let metadata = fs::metadata(path)?;
        Ok(metadata.len() as usize)
    }
    
    /// Compress a video resource
    fn compress_video(&self, path: &Path) -> Result<usize> {
        // In a real implementation, you'd use video compression
        let metadata = fs::metadata(path)?;
        Ok(metadata.len() as usize)
    }
    
    /// Compress a font resource
    fn compress_font(&self, path: &Path) -> Result<usize> {
        // Font compression (subset based on used characters)
        let metadata = fs::metadata(path)?;
        Ok(metadata.len() as usize)
    }
    
    /// Compress a script resource
    fn compress_script(&self, path: &Path) -> Result<usize> {
        // Script minification
        let content = fs::read_to_string(path)?;
        let minified = self.minify_script_content(&content);
        fs::write(path, minified.as_bytes())?;
        Ok(minified.len())
    }
    
    /// Compress a WASM module
    fn compress_wasm(&self, path: &Path) -> Result<usize> {
        // WASM optimization
        let metadata = fs::metadata(path)?;
        Ok(metadata.len() as usize)
    }
    
    /// Generic compression using gzip
    fn compress_generic(&self, path: &Path) -> Result<usize> {
        use std::io::prelude::*;
        use flate2::Compression;
        use flate2::write::GzEncoder;
        
        let content = fs::read(path)?;
        let mut encoder = GzEncoder::new(Vec::new(), Compression::default());
        encoder.write_all(&content)?;
        let compressed = encoder.finish()?;
        
        // Write compressed version
        let compressed_path = path.with_extension(format!("{}.gz", 
            path.extension().and_then(|e| e.to_str()).unwrap_or("bin")));
        fs::write(compressed_path, &compressed)?;
        
        Ok(compressed.len())
    }
    
    /// Minify script content
    fn minify_script_content(&self, content: &str) -> String {
        // Basic minification - remove comments and extra whitespace
        content
            .lines()
            .map(|line| line.trim())
            .filter(|line| !line.is_empty() && !line.starts_with("//") && !line.starts_with("#"))
            .collect::<Vec<_>>()
            .join("\n")
    }
    
    /// Get MIME type for a resource type
    fn get_mime_type(&self, resource_type: &super::dependency_analyzer::ResourceType) -> String {
        use super::dependency_analyzer::ResourceType;
        
        match resource_type {
            ResourceType::Image => "image/*".to_string(),
            ResourceType::Audio => "audio/*".to_string(),
            ResourceType::Video => "video/*".to_string(),
            ResourceType::Font => "font/*".to_string(),
            ResourceType::Script => "text/plain".to_string(),
            ResourceType::WasmModule => "application/wasm".to_string(),
            ResourceType::Stylesheet => "text/css".to_string(),
            ResourceType::Other(_) => "application/octet-stream".to_string(),
        }
    }
    
    /// Calculate total size of all resources
    pub fn calculate_total_size(&self, resources: &HashMap<String, ResourceInfo>) -> usize {
        resources.values().map(|r| r.size).sum()
    }
    
    /// Get compression statistics
    pub fn get_compression_stats(&self, resources: &HashMap<String, ResourceInfo>) -> (usize, usize, f32) {
        let total_size = self.calculate_total_size(resources);
        let compressed_size = total_size; // Placeholder
        let compression_ratio = if total_size > 0 {
            (compressed_size as f32 / total_size as f32) * 100.0
        } else {
            0.0
        };
        
        (total_size, compressed_size, compression_ratio)
    }
}