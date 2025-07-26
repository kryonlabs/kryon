//! High-performance optimized KryonApp implementation
//!
//! This module provides a production-ready KryonApp with optimized layout computation,
//! property caching, and minimal performance overhead.

use kryon_core::{
    KRBFile, Element, ElementId, InteractionState, EventType, load_krb_file,
    StyleComputer, OptimizedPropertyCache, LayoutDimension, PropertyValue,
};
use kryon_layout::{LayoutEngine, OptimizedTaffyLayoutEngine, LayoutResult, LayoutConfig, InvalidationRegion};
use kryon_render::{ElementRenderer, CommandRenderer, KeyCode};
use kryon_render::events::KeyModifiers;
use glam::Vec2;
use std::collections::HashMap;
use std::time::{Duration, Instant};

use crate::event_system::EventSystem;
use crate::script::ScriptSystem;
use crate::template_engine::TemplateEngine;

/// Theme definition for runtime switching
#[derive(Debug, Clone)]
pub struct RuntimeTheme {
    pub name: String,
    pub variables: HashMap<String, PropertyValue>,
    pub parent: Option<String>,
}

/// Theme manager for runtime theme switching
#[derive(Debug, Clone)]
pub struct ThemeManager {
    themes: HashMap<String, RuntimeTheme>,
    active_theme: Option<String>,
    variable_cache: HashMap<String, PropertyValue>,
}

impl RuntimeTheme {
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            variables: HashMap::new(),
            parent: None,
        }
    }

    pub fn with_variable(mut self, name: impl Into<String>, value: PropertyValue) -> Self {
        self.variables.insert(name.into(), value);
        self
    }

    pub fn with_parent(mut self, parent: impl Into<String>) -> Self {
        self.parent = Some(parent.into());
        self
    }
}

impl ThemeManager {
    pub fn new() -> Self {
        let mut manager = Self {
            themes: HashMap::new(),
            active_theme: None,
            variable_cache: HashMap::new(),
        };
        manager.create_default_themes();
        manager
    }

    fn create_default_themes(&mut self) {
        // Light theme
        let light_theme = RuntimeTheme::new("light")
            .with_variable("primary_color".to_string(), PropertyValue::String("#3b82f6".to_string()))
            .with_variable("secondary_color".to_string(), PropertyValue::String("#64748b".to_string()))
            .with_variable("background_color".to_string(), PropertyValue::String("#ffffff".to_string()))
            .with_variable("surface_color".to_string(), PropertyValue::String("#f8fafc".to_string()))
            .with_variable("text_color".to_string(), PropertyValue::String("#1e293b".to_string()))
            .with_variable("text_muted_color".to_string(), PropertyValue::String("#64748b".to_string()))
            .with_variable("border_color".to_string(), PropertyValue::String("#e2e8f0".to_string()))
            .with_variable("spacing".to_string(), PropertyValue::Int(16))
            .with_variable("border_radius".to_string(), PropertyValue::Int(8));

        // Dark theme
        let dark_theme = RuntimeTheme::new("dark")
            .with_variable("primary_color".to_string(), PropertyValue::String("#60a5fa".to_string()))
            .with_variable("secondary_color".to_string(), PropertyValue::String("#94a3b8".to_string()))
            .with_variable("background_color".to_string(), PropertyValue::String("#0f172a".to_string()))
            .with_variable("surface_color".to_string(), PropertyValue::String("#1e293b".to_string()))
            .with_variable("text_color".to_string(), PropertyValue::String("#f1f5f9".to_string()))
            .with_variable("text_muted_color".to_string(), PropertyValue::String("#94a3b8".to_string()))
            .with_variable("border_color".to_string(), PropertyValue::String("#334155".to_string()))
            .with_variable("spacing".to_string(), PropertyValue::Int(16))
            .with_variable("border_radius".to_string(), PropertyValue::Int(8));

        self.add_theme(light_theme);
        self.add_theme(dark_theme);
        self.set_active_theme("light");
    }

    pub fn add_theme(&mut self, theme: RuntimeTheme) {
        self.themes.insert(theme.name.clone(), theme);
        self.rebuild_variable_cache();
    }

    pub fn set_active_theme(&mut self, theme_name: &str) -> bool {
        if self.themes.contains_key(theme_name) {
            self.active_theme = Some(theme_name.to_string());
            self.rebuild_variable_cache();
            true
        } else {
            false
        }
    }

    pub fn get_active_theme(&self) -> Option<&str> {
        self.active_theme.as_deref()
    }

    pub fn get_variable(&self, variable_name: &str) -> Option<&PropertyValue> {
        self.variable_cache.get(variable_name)
    }

    fn rebuild_variable_cache(&mut self) {
        self.variable_cache.clear();
        
        if let Some(active_theme_name) = self.active_theme.clone() {
            self.resolve_theme_variables(&active_theme_name);
        }
    }

    fn resolve_theme_variables(&mut self, theme_name: &str) {
        let mut visited = std::collections::HashSet::new();
        self.resolve_theme_variables_recursive(theme_name, &mut visited);
    }

    fn resolve_theme_variables_recursive(&mut self, theme_name: &str, visited: &mut std::collections::HashSet<String>) {
        if visited.contains(theme_name) {
            // Circular dependency detected, skip
            return;
        }
        
        visited.insert(theme_name.to_string());
        
        if let Some(theme) = self.themes.get(theme_name).cloned() {
            // First resolve parent theme variables
            if let Some(parent_name) = &theme.parent {
                self.resolve_theme_variables_recursive(parent_name, visited);
            }
            
            // Then add/override with current theme variables
            for (var_name, var_value) in theme.variables {
                self.variable_cache.insert(var_name, var_value);
            }
        }
    }

    pub fn interpolate_string(&self, input: &str) -> String {
        let mut result = input.to_string();
        
        // Simple variable interpolation for ${theme.variable_name}
        let re = regex::Regex::new(r"\$\{theme\.([^}]+)\}").unwrap();
        
        result = re.replace_all(&result, |caps: &regex::Captures| {
            let var_name = &caps[1];
            if let Some(value) = self.get_variable(var_name) {
                match value {
                    PropertyValue::String(s) => s.clone(),
                    PropertyValue::Int(i) => i.to_string(),
                    PropertyValue::Float(f) => f.to_string(),
                    PropertyValue::Bool(b) => b.to_string(),
                    _ => format!("{:?}", value),
                }
            } else {
                caps[0].to_string() // Return original if variable not found
            }
        }).to_string();
        
        result
    }

    pub fn list_themes(&self) -> Vec<&str> {
        self.themes.keys().map(|s| s.as_str()).collect()
    }
}

/// Performance configuration for optimized app
#[derive(Debug, Clone)]
pub struct OptimizedAppConfig {
    /// Layout engine configuration
    pub layout_config: LayoutConfig,
    /// Enable performance monitoring
    pub enable_profiling: bool,
    /// Target frame rate (for pacing)
    pub target_fps: u32,
    /// Enable async script execution
    pub async_scripts: bool,
    /// Maximum script execution time per frame (ms)
    pub max_script_time_ms: u32,
}

impl Default for OptimizedAppConfig {
    fn default() -> Self {
        Self {
            layout_config: LayoutConfig::default(),
            enable_profiling: false,
            target_fps: 60,
            async_scripts: false,
            max_script_time_ms: 5,
        }
    }
}

/// High-performance KryonApp with optimized systems
pub struct OptimizedKryonApp<R: CommandRenderer> {
    // Core data
    krb_file: KRBFile,
    elements: HashMap<ElementId, Element>,
    
    // Optimized systems
    #[allow(dead_code)]
    style_computer: StyleComputer,
    layout_engine: OptimizedTaffyLayoutEngine,
    property_cache: OptimizedPropertyCache,
    renderer: ElementRenderer<R>,
    event_system: EventSystem,
    script_system: ScriptSystem,
    template_engine: TemplateEngine,
    theme_manager: ThemeManager,
    
    // State
    layout_result: LayoutResult,
    viewport_size: Vec2,
    needs_layout: bool,
    needs_render: bool,
    
    // Performance tracking
    last_frame_time: Instant,
    frame_count: u64,
    frame_times: Vec<Duration>,
    config: OptimizedAppConfig,
    
    // Layout invalidation
    invalidation_regions: Vec<InvalidationRegion>,
}

impl<R: CommandRenderer> OptimizedKryonApp<R> {
    /// Create new optimized app
    pub fn new(krb_path: &str, renderer: R) -> anyhow::Result<Self> {
        Self::with_config(krb_path, renderer, OptimizedAppConfig::default())
    }

    /// Create new optimized app with custom configuration
    pub fn with_config(krb_path: &str, renderer: R, config: OptimizedAppConfig) -> anyhow::Result<Self> {
        let krb_file = load_krb_file(krb_path)?;
        Self::with_krb_and_config(krb_file, renderer, config)
    }

    /// Create new optimized app with KRB data and configuration
    pub fn with_krb_and_config(krb_file: KRBFile, renderer: R, config: OptimizedAppConfig) -> anyhow::Result<Self> {
        let mut elements = krb_file.elements.clone();
        
        let style_computer = StyleComputer::new(&elements, &krb_file.styles);
        
        // Link parent-child relationships
        Self::link_element_hierarchy(&mut elements, &krb_file)?;
        
        // Create optimized layout engine
        let layout_engine = OptimizedTaffyLayoutEngine::with_config(config.layout_config.clone());
        
        // Create optimized property cache
        let mut property_cache = OptimizedPropertyCache::with_capacity(elements.len());
        property_cache.update_from_elements(&elements);
        
        let renderer = ElementRenderer::new(renderer, style_computer.clone());
        let viewport_size = renderer.backend().viewport_size();
        
        let event_system = EventSystem::new();
        let script_system = ScriptSystem::new()?;
        let template_engine = TemplateEngine::new(&krb_file);
        let theme_manager = ThemeManager::new();
        
        let mut app = Self {
            krb_file,
            elements,
            style_computer,
            layout_engine,
            property_cache,
            renderer,
            event_system,
            script_system,
            template_engine,
            theme_manager,
            layout_result: LayoutResult {
                computed_positions: HashMap::new(),
                computed_sizes: HashMap::new(),
            },
            viewport_size,
            needs_layout: true,
            needs_render: true,
            last_frame_time: Instant::now(),
            frame_count: 0,
            frame_times: Vec::with_capacity(120), // Store 2 seconds of frame times
            config,
            invalidation_regions: Vec::new(),
        };
        
        // Initialize systems
        app.initialize_systems()?;
        
        Ok(app)
    }

    /// Initialize all systems
    fn initialize_systems(&mut self) -> anyhow::Result<()> {
        // Initialize script system with KRB file data
        self.script_system.initialize(&self.krb_file, &self.elements)?;
        
        // Initialize template engine
        if self.template_engine.has_bindings() {
            self.template_engine.update_elements(&mut self.elements, &self.krb_file);
            self.update_property_cache();
        }
        
        tracing::info!("Optimized KryonApp initialized with {} elements", self.elements.len());
        Ok(())
    }

    /// Update property cache after element changes
    fn update_property_cache(&mut self) {
        self.property_cache.update_from_elements(&self.elements);
    }

    /// Add invalidation region for partial updates
    pub fn add_invalidation_region(&mut self, region: InvalidationRegion) {
        self.invalidation_regions.push(region.clone());
        self.layout_engine.add_invalidation_region(region);
    }

    /// Mark that layout needs recomputation
    pub fn mark_needs_layout(&mut self) {
        self.needs_layout = true;
    }

    /// Mark that render needs to happen
    pub fn mark_needs_render(&mut self) {
        self.needs_render = true;
    }

    /// Main update loop (optimized)
    pub fn update(&mut self) -> anyhow::Result<()> {
        let frame_start = Instant::now();
        
        // Handle events
        self.handle_events()?;
        
        // Update scripts (with time limits)
        self.update_scripts_optimized()?;
        
        // Update layout if needed
        if self.needs_layout {
            self.update_layout_optimized()?;
        }
        
        // Update performance metrics
        self.update_performance_metrics(frame_start);
        
        Ok(())
    }

    /// Optimized layout update
    fn update_layout_optimized(&mut self) -> anyhow::Result<()> {
        let layout_start = Instant::now();
        
        // Compute layout with optimized engine
        self.layout_result = self.layout_engine.compute_layout(
            &self.elements,
            self.krb_file.root_element_id.unwrap_or(0),
            self.viewport_size,
        );
        
        // Apply layout results to elements
        self.apply_layout_results();
        
        self.needs_layout = false;
        self.needs_render = true;
        
        let layout_time = layout_start.elapsed();
        if self.config.enable_profiling && layout_time > Duration::from_millis(5) {
            tracing::debug!("Layout computation took {:?}", layout_time);
        }
        
        Ok(())
    }

    /// Apply layout results to elements
    fn apply_layout_results(&mut self) {
        for (&element_id, position) in &self.layout_result.computed_positions {
            if let Some(element) = self.elements.get_mut(&element_id) {
                element.layout_position.x = LayoutDimension::Pixels(position.x);
                element.layout_position.y = LayoutDimension::Pixels(position.y);
            }
        }
        
        for (&element_id, size) in &self.layout_result.computed_sizes {
            if let Some(element) = self.elements.get_mut(&element_id) {
                element.layout_size.width = LayoutDimension::Pixels(size.x);
                element.layout_size.height = LayoutDimension::Pixels(size.y);
            }
        }
        
        // Update property cache
        self.update_property_cache();
    }

    /// Optimized script update with time limits
    fn update_scripts_optimized(&mut self) -> anyhow::Result<()> {
        let script_start = Instant::now();
        let max_time = Duration::from_millis(self.config.max_script_time_ms as u64);
        
        // Update script system  
        let changes_applied = self.script_system.apply_pending_changes(&mut self.elements)?;
        if changes_applied {
            self.needs_render = true;
        }
        
        // Check if we exceeded time limit
        if script_start.elapsed() > max_time && self.config.enable_profiling {
            tracing::warn!("Script execution exceeded time limit: {:?}", script_start.elapsed());
        }
        
        Ok(())
    }

    /// Handle events efficiently
    fn handle_events(&mut self) -> anyhow::Result<()> {
        // Update the event system
        self.event_system.update(&mut self.elements)?;
        
        Ok(())
    }

    /// Optimized click handling
    #[allow(dead_code)]
    fn handle_click_optimized(&mut self, element_id: ElementId, _position: Vec2) -> anyhow::Result<()> {
        // Get handler name first to avoid borrowing issues
        let handler_name = self.elements.get(&element_id)
            .and_then(|e| e.event_handlers.get(&EventType::Click))
            .cloned();
        
        // Get element efficiently from cache
        if let Some(element) = self.elements.get_mut(&element_id) {
            // Toggle checkbox state
            if element.element_type == kryon_core::ElementType::Button {
                element.current_state = match element.current_state {
                    InteractionState::Checked => InteractionState::Normal,
                    _ => InteractionState::Checked,
                };
            }
        }
        
        // Add targeted invalidation region
        self.add_invalidation_region(InvalidationRegion {
            elements: vec![element_id],
            full_tree: false,
            bounds: None,
        });
        
        // Execute click handlers from event handlers
        if let Some(handler) = handler_name {
            self.script_system.call_function(&handler, vec![])?;
        }
        
        Ok(())
    }

    /// Optimized hover handling
    #[allow(dead_code)]
    fn handle_hover_optimized(&mut self, element_id: ElementId, _position: Vec2) -> anyhow::Result<()> {
        // Update hover state efficiently
        if let Some(element) = self.elements.get_mut(&element_id) {
            if element.current_state != InteractionState::Hover {
                element.current_state = InteractionState::Hover;
                
                // Add targeted invalidation
                self.add_invalidation_region(InvalidationRegion {
                    elements: vec![element_id],
                    full_tree: false,
                    bounds: None,
                });
            }
        }
        
        Ok(())
    }

    /// Optimized focus handling
    #[allow(dead_code)]
    fn handle_focus_optimized(&mut self, element_id: ElementId) -> anyhow::Result<()> {
        // Update focus state efficiently
        if let Some(element) = self.elements.get_mut(&element_id) {
            if element.current_state != InteractionState::Focus {
                element.current_state = InteractionState::Focus;
                
                // Add targeted invalidation
                self.add_invalidation_region(InvalidationRegion {
                    elements: vec![element_id],
                    full_tree: false,
                    bounds: None,
                });
            }
        }
        
        Ok(())
    }

    /// Optimized key press handling
    #[allow(dead_code)]
    fn handle_key_press_optimized(&mut self, key: KeyCode, _modifiers: KeyModifiers) -> anyhow::Result<()> {
        // Handle global key events
        match key {
            KeyCode::Escape => {
                // Handle escape key
            }
            KeyCode::Enter => {
                // Handle enter key
            }
            _ => {}
        }
        
        Ok(())
    }

    /// Render frame (optimized)
    pub fn render(&mut self) -> anyhow::Result<()> {
        if !self.needs_render {
            return Ok(());
        }
        
        let render_start = Instant::now();
        
        // Render with optimized renderer
        let clear_color = glam::Vec4::new(0.1, 0.1, 0.1, 1.0);
        if let Some(root_id) = self.krb_file.root_element_id {
            self.renderer.render_frame(&self.elements, &self.layout_result, root_id, clear_color)?;
        }
        
        self.needs_render = false;
        
        let render_time = render_start.elapsed();
        if self.config.enable_profiling && render_time > Duration::from_millis(16) {
            tracing::debug!("Render took {:?}", render_time);
        }
        
        Ok(())
    }

    /// Update performance metrics
    fn update_performance_metrics(&mut self, frame_start: Instant) {
        let frame_time = frame_start.elapsed();
        self.frame_times.push(frame_time);
        
        // Keep only recent frame times
        if self.frame_times.len() > 120 {
            self.frame_times.remove(0);
        }
        
        self.frame_count += 1;
        self.last_frame_time = Instant::now();
        
        // Log performance metrics periodically
        if self.config.enable_profiling && self.frame_count % 60 == 0 {
            let avg_frame_time = self.frame_times.iter().sum::<Duration>() / self.frame_times.len() as u32;
            let fps = 1.0 / avg_frame_time.as_secs_f64();
            
            let cache_stats = self.property_cache.get_stats();
            let layout_stats = self.layout_engine.get_cache_stats();
            
            tracing::info!(
                "Performance: {:.1} FPS, Avg frame: {:?}, Cache: {} elements, Layout cache: {}/{}",
                fps, avg_frame_time, cache_stats.element_count, layout_stats.0, layout_stats.1
            );
        }
    }

    /// Get performance statistics
    pub fn get_performance_stats(&self) -> PerformanceStats {
        let avg_frame_time = if !self.frame_times.is_empty() {
            self.frame_times.iter().sum::<Duration>() / self.frame_times.len() as u32
        } else {
            Duration::ZERO
        };
        
        let fps = if avg_frame_time > Duration::ZERO {
            1.0 / avg_frame_time.as_secs_f64()
        } else {
            0.0
        };
        
        PerformanceStats {
            frame_count: self.frame_count,
            average_frame_time: avg_frame_time,
            fps,
            property_cache_stats: self.property_cache.get_stats(),
            layout_cache_stats: self.layout_engine.get_cache_stats(),
        }
    }

    /// Link element hierarchy (same as original)
    fn link_element_hierarchy(elements: &mut HashMap<ElementId, Element>, _krb_file: &KRBFile) -> anyhow::Result<()> {
        // Same implementation as original KryonApp
        for (_element_id, element) in elements.iter_mut() {
            element.parent = None;
            element.children.clear();
        }
        
        // Get child relationships first, then update parents
        let mut child_relationships = Vec::new();
        for (&element_id, element) in elements.iter() {
            for &child_id in &element.children {
                child_relationships.push((element_id, child_id));
            }
        }
        
        for (parent_id, child_id) in child_relationships {
            if let Some(child_element) = elements.get_mut(&child_id) {
                child_element.parent = Some(parent_id);
            }
        }
        
        Ok(())
    }

    /// Get renderer reference
    pub fn renderer(&self) -> &ElementRenderer<R> {
        &self.renderer
    }

    /// Get mutable renderer reference
    pub fn renderer_mut(&mut self) -> &mut ElementRenderer<R> {
        &mut self.renderer
    }

    /// Get elements reference
    pub fn elements(&self) -> &HashMap<ElementId, Element> {
        &self.elements
    }

    /// Get viewport size
    pub fn viewport_size(&self) -> Vec2 {
        self.viewport_size
    }

    /// Resize viewport
    pub fn resize(&mut self, new_size: Vec2) -> anyhow::Result<()> {
        self.viewport_size = new_size;
        self.renderer.backend_mut().resize(new_size)?;
        
        // Full layout invalidation on resize
        self.add_invalidation_region(InvalidationRegion {
            elements: Vec::new(),
            full_tree: true,
            bounds: None,
        });
        
        self.mark_needs_layout();
        Ok(())
    }

    // === Theme Management API ===

    /// Switch to a different theme
    pub fn set_theme(&mut self, theme_name: &str) -> anyhow::Result<bool> {
        if self.theme_manager.set_active_theme(theme_name) {
            // Theme changed successfully, recompute all themed properties
            self.recompute_themed_properties()?;
            self.mark_needs_layout();
            self.mark_needs_render();
            Ok(true)
        } else {
            Ok(false)
        }
    }

    /// Get the currently active theme name
    pub fn get_active_theme(&self) -> Option<&str> {
        self.theme_manager.get_active_theme()
    }

    /// Add a custom theme at runtime
    pub fn add_theme(&mut self, theme: RuntimeTheme) {
        self.theme_manager.add_theme(theme);
    }

    /// List all available themes
    pub fn list_themes(&self) -> Vec<&str> {
        self.theme_manager.list_themes()
    }

    /// Get a theme variable value
    pub fn get_theme_variable(&self, variable_name: &str) -> Option<&PropertyValue> {
        self.theme_manager.get_variable(variable_name)
    }

    /// Create a theme at runtime with builder pattern
    pub fn create_theme(&mut self, name: &str) -> RuntimeThemeBuilder {
        RuntimeThemeBuilder::new(name, &mut self.theme_manager)
    }

    /// Recompute all themed properties after theme change
    fn recompute_themed_properties(&mut self) -> anyhow::Result<()> {
        // Iterate through all elements and recompute their themed properties
        for (_element_id, element) in self.elements.iter_mut() {
            // Check each property for theme variable references
            let mut properties_to_update = Vec::new();
            
            for (prop_name, prop_value) in &element.custom_properties {
                if let PropertyValue::String(s) = prop_value {
                    if s.contains("${theme.") {
                        // This property contains theme variables, interpolate it
                        let interpolated = self.theme_manager.interpolate_string(s);
                        properties_to_update.push((prop_name.clone(), PropertyValue::String(interpolated)));
                    }
                }
            }
            
            // Apply updated properties
            for (prop_name, new_value) in properties_to_update {
                element.custom_properties.insert(prop_name, new_value);
            }
        }
        
        // Update property cache
        self.property_cache.update_from_elements(&self.elements);
        
        Ok(())
    }
}

/// Builder for creating themes at runtime
pub struct RuntimeThemeBuilder<'a> {
    theme: RuntimeTheme,
    theme_manager: &'a mut ThemeManager,
}

impl<'a> RuntimeThemeBuilder<'a> {
    fn new(name: &str, theme_manager: &'a mut ThemeManager) -> Self {
        Self {
            theme: RuntimeTheme::new(name),
            theme_manager,
        }
    }

    /// Add a string variable to the theme
    pub fn with_string(mut self, name: &str, value: &str) -> Self {
        self.theme = self.theme.with_variable(name.to_string(), PropertyValue::String(value.to_string()));
        self
    }

    /// Add an integer variable to the theme
    pub fn with_integer(mut self, name: &str, value: i32) -> Self {
        self.theme = self.theme.with_variable(name.to_string(), PropertyValue::Int(value));
        self
    }

    /// Add a float variable to the theme
    pub fn with_float(mut self, name: &str, value: f32) -> Self {
        self.theme = self.theme.with_variable(name.to_string(), PropertyValue::Float(value));
        self
    }

    /// Add a boolean variable to the theme
    pub fn with_boolean(mut self, name: &str, value: bool) -> Self {
        self.theme = self.theme.with_variable(name.to_string(), PropertyValue::Bool(value));
        self
    }

    /// Set parent theme for inheritance
    pub fn with_parent(mut self, parent_name: &str) -> Self {
        self.theme = self.theme.with_parent(parent_name);
        self
    }

    /// Finish building and add the theme
    pub fn build(self) {
        self.theme_manager.add_theme(self.theme);
    }
}

/// Performance statistics
#[derive(Debug, Clone)]
pub struct PerformanceStats {
    pub frame_count: u64,
    pub average_frame_time: Duration,
    pub fps: f64,
    pub property_cache_stats: kryon_core::CacheStats,
    pub layout_cache_stats: (usize, usize),
}

#[cfg(test)]
mod tests {
    use super::*;
    use kryon_render::RenderResult;
    use glam::Vec4;

    // Mock renderer for testing
    struct MockRenderer;
    
    impl kryon_render::Renderer for MockRenderer {
        type Surface = Vec2;
        type Context = ();
        
        fn initialize(_surface: Vec2) -> RenderResult<Self> {
            Ok(MockRenderer)
        }
        
        fn begin_frame(&mut self, _clear_color: Vec4) -> RenderResult<()> {
            Ok(())
        }
        
        fn end_frame(&mut self, _context: ()) -> RenderResult<()> {
            Ok(())
        }
        
        fn render_element(&mut self, _context: &mut (), _element: &Element, _layout: &LayoutResult, _element_id: ElementId) -> RenderResult<()> {
            Ok(())
        }
        
        fn resize(&mut self, _new_size: Vec2) -> RenderResult<()> {
            Ok(())
        }
        
        fn viewport_size(&self) -> Vec2 {
            Vec2::new(800.0, 600.0)
        }
    }
    
    impl CommandRenderer for MockRenderer {
        fn execute_commands(&mut self, _context: &mut (), _commands: &[kryon_render::RenderCommand]) -> RenderResult<()> {
            Ok(())
        }
    }

    #[test]
    fn test_optimized_app_creation() {
        // Create test KRB file
        let krb_file = KRBFile {
            elements: HashMap::new(),
            styles: HashMap::new(),
            strings: Vec::new(),
            root_element_id: Some(0),
        };
        
        let renderer = MockRenderer;
        let config = OptimizedAppConfig::default();
        
        let result = OptimizedKryonApp::with_krb_and_config(krb_file, renderer, config);
        assert!(result.is_ok());
    }
}