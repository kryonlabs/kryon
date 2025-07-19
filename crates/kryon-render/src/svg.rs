//! SVG rendering support using resvg
//!
//! This module provides SVG rendering capabilities including:
//! - Loading SVG files from disk or embedded content
//! - Rendering SVG to raster images using resvg
//! - Caching rendered SVG images with Arc<> for efficient memory usage
//! - Integration with the existing image rendering pipeline

use std::collections::HashMap;
use std::path::Path;
use std::sync::Arc;
use glam::Vec2;
use anyhow::{Result, anyhow};

#[cfg(feature = "svg")]
use resvg::usvg;
#[cfg(feature = "svg")]
use resvg::tiny_skia;

/// SVG image data that can be shared across render commands
#[derive(Clone)]
pub struct SvgImageData {
    /// Rendered bitmap data
    pub pixels: Arc<Vec<u8>>,
    /// Image dimensions
    pub width: u32,
    pub height: u32,
    /// Original SVG size for scaling calculations
    pub original_size: Vec2,
}

/// SVG rendering options
#[derive(Debug, Clone)]
pub struct SvgRenderOptions {
    /// Target width for rendering (None = use original width)
    pub target_width: Option<u32>,
    /// Target height for rendering (None = use original height)
    pub target_height: Option<u32>,
    /// DPI scaling factor
    pub dpi_scale: f32,
    /// Background color (RGBA)
    pub background_color: Option<[u8; 4]>,
}

impl Default for SvgRenderOptions {
    fn default() -> Self {
        Self {
            target_width: None,
            target_height: None,
            dpi_scale: 1.0,
            background_color: None,
        }
    }
}

/// SVG cache for efficient reuse of rendered SVG images
pub struct SvgCache {
    /// Cache of rendered SVG images by source path/content hash
    cache: HashMap<String, SvgImageData>,
    /// Maximum cache size in bytes
    max_cache_size: usize,
    /// Current cache size in bytes
    current_cache_size: usize,
}

impl SvgCache {
    /// Create a new SVG cache
    pub fn new(max_cache_size: usize) -> Self {
        Self {
            cache: HashMap::new(),
            max_cache_size,
            current_cache_size: 0,
        }
    }

    /// Get cached SVG image data
    pub fn get(&self, key: &str) -> Option<&SvgImageData> {
        self.cache.get(key)
    }

    /// Insert SVG image data into cache
    pub fn insert(&mut self, key: String, data: SvgImageData) {
        let data_size = data.pixels.len();
        
        // Check if we need to evict entries
        while self.current_cache_size + data_size > self.max_cache_size && !self.cache.is_empty() {
            // Simple eviction: remove first entry (could be improved with LRU)
            if let Some((_, evicted_data)) = self.cache.iter().next() {
                let evicted_key = self.cache.keys().next().unwrap().clone();
                let evicted_size = evicted_data.pixels.len();
                self.cache.remove(&evicted_key);
                self.current_cache_size -= evicted_size;
            }
        }
        
        self.cache.insert(key, data);
        self.current_cache_size += data_size;
    }

    /// Clear all cached SVG images
    pub fn clear(&mut self) {
        self.cache.clear();
        self.current_cache_size = 0;
    }

    /// Get cache statistics
    pub fn stats(&self) -> (usize, usize, usize) {
        (self.cache.len(), self.current_cache_size, self.max_cache_size)
    }
}

/// SVG renderer using resvg
pub struct SvgRenderer {
    /// Cache for rendered SVG images
    cache: SvgCache,
}

impl SvgRenderer {
    /// Create a new SVG renderer
    pub fn new() -> Self {
        Self {
            cache: SvgCache::new(50 * 1024 * 1024), // 50MB cache by default
        }
    }

    /// Create a new SVG renderer with custom cache size
    pub fn with_cache_size(max_cache_size: usize) -> Self {
        Self {
            cache: SvgCache::new(max_cache_size),
        }
    }

    /// Render SVG from file path
    pub fn render_file(&mut self, path: &Path, options: &SvgRenderOptions) -> Result<SvgImageData> {
        let path_str = path.to_string_lossy();
        let cache_key = format!("{}:{:?}", path_str, options);
        
        // Check cache first
        if let Some(cached_data) = self.cache.get(&cache_key) {
            return Ok(cached_data.clone());
        }

        // Read and render SVG
        let svg_data = std::fs::read(path)?;
        let rendered_data = self.render_svg_data(&svg_data, options)?;
        
        // Cache the result
        self.cache.insert(cache_key, rendered_data.clone());
        
        Ok(rendered_data)
    }

    /// Render SVG from string content
    pub fn render_string(&mut self, svg_content: &str, options: &SvgRenderOptions) -> Result<SvgImageData> {
        let cache_key = format!("str:{}:{:?}", 
            &svg_content[..std::cmp::min(100, svg_content.len())], options);
        
        // Check cache first
        if let Some(cached_data) = self.cache.get(&cache_key) {
            return Ok(cached_data.clone());
        }

        // Render SVG
        let rendered_data = self.render_svg_data(svg_content.as_bytes(), options)?;
        
        // Cache the result
        self.cache.insert(cache_key, rendered_data.clone());
        
        Ok(rendered_data)
    }

    /// Render SVG from raw bytes
    #[cfg(feature = "svg")]
    fn render_svg_data(&self, svg_data: &[u8], options: &SvgRenderOptions) -> Result<SvgImageData> {
        // Parse SVG with usvg
        let mut svg_options = usvg::Options::default();
        svg_options.dpi = 96.0 * options.dpi_scale;
        
        let svg_tree = usvg::Tree::from_data(svg_data, &svg_options)
            .map_err(|e| anyhow!("Failed to parse SVG: {}", e))?;

        let original_size = Vec2::new(
            svg_tree.size().width() as f32,
            svg_tree.size().height() as f32
        );

        // Calculate target dimensions
        let target_width = options.target_width.unwrap_or(original_size.x as u32);
        let target_height = options.target_height.unwrap_or(original_size.y as u32);

        // Create pixmap for rendering
        let mut pixmap = tiny_skia::Pixmap::new(target_width, target_height)
            .ok_or_else(|| anyhow!("Failed to create pixmap"))?;

        // Fill background if specified
        if let Some(bg_color) = options.background_color {
            pixmap.fill(tiny_skia::Color::from_rgba8(bg_color[0], bg_color[1], bg_color[2], bg_color[3]));
        }

        // Calculate transform for scaling
        let scale_x = target_width as f32 / original_size.x;
        let scale_y = target_height as f32 / original_size.y;
        let transform = tiny_skia::Transform::from_scale(scale_x, scale_y);

        // Render SVG to pixmap
        resvg::render(&svg_tree, transform, &mut pixmap.as_mut());

        // Convert pixmap to RGBA bytes
        let pixels = pixmap.take();
        
        Ok(SvgImageData {
            pixels: Arc::new(pixels),
            width: target_width,
            height: target_height,
            original_size,
        })
    }

    /// Render SVG from raw bytes (fallback when SVG feature is disabled)
    #[cfg(not(feature = "svg"))]
    fn render_svg_data(&self, _svg_data: &[u8], _options: &SvgRenderOptions) -> Result<SvgImageData> {
        Err(anyhow!("SVG rendering not available - compile with 'svg' feature"))
    }

    /// Get cache statistics
    pub fn cache_stats(&self) -> (usize, usize, usize) {
        self.cache.stats()
    }

    /// Clear SVG cache
    pub fn clear_cache(&mut self) {
        self.cache.clear();
    }
}

impl Default for SvgRenderer {
    fn default() -> Self {
        Self::new()
    }
}

/// Helper function to create SVG render options with specific dimensions
pub fn svg_options_with_size(width: u32, height: u32) -> SvgRenderOptions {
    SvgRenderOptions {
        target_width: Some(width),
        target_height: Some(height),
        ..Default::default()
    }
}

/// Helper function to create SVG render options with DPI scaling
pub fn svg_options_with_dpi(dpi_scale: f32) -> SvgRenderOptions {
    SvgRenderOptions {
        dpi_scale,
        ..Default::default()
    }
}

/// Helper function to create SVG render options with background color
pub fn svg_options_with_background(background_color: [u8; 4]) -> SvgRenderOptions {
    SvgRenderOptions {
        background_color: Some(background_color),
        ..Default::default()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_svg_cache_creation() {
        let cache = SvgCache::new(1024);
        let (count, current, max) = cache.stats();
        assert_eq!(count, 0);
        assert_eq!(current, 0);
        assert_eq!(max, 1024);
    }

    #[test]
    fn test_svg_render_options() {
        let options = SvgRenderOptions::default();
        assert_eq!(options.target_width, None);
        assert_eq!(options.target_height, None);
        assert_eq!(options.dpi_scale, 1.0);
        assert_eq!(options.background_color, None);
    }

    #[test]
    fn test_svg_options_helpers() {
        let options = svg_options_with_size(100, 200);
        assert_eq!(options.target_width, Some(100));
        assert_eq!(options.target_height, Some(200));

        let options = svg_options_with_dpi(2.0);
        assert_eq!(options.dpi_scale, 2.0);

        let options = svg_options_with_background([255, 0, 0, 255]);
        assert_eq!(options.background_color, Some([255, 0, 0, 255]));
    }
}