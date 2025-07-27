// crates/kryon-runtime/src/lib.rs

use kryon_core::{
    KRBFile, Element, ElementId, InteractionState, EventType, load_krb_file,
    StyleComputer, LayoutDimension,
};
use kryon_layout::{LayoutEngine, TaffyLayoutEngine, OptimizedTaffyLayoutEngine, LayoutResult};
use kryon_render::{ElementRenderer, CommandRenderer, KeyCode};
use kryon_render::events::{InputEvent, MouseButton, KeyModifiers};
use glam::Vec2;
use std::collections::HashMap;
use std::time::{Duration, Instant};

pub mod backends;
pub mod event_system;
pub mod script;
pub mod template_engine;
pub mod shared_data;
pub mod optimized_app;

pub use backends::*;
pub use event_system::*;
pub use script::ScriptSystem;
pub use template_engine::*;
pub use shared_data::*;
pub use optimized_app::*;

pub struct KryonApp<R: CommandRenderer> {
    // Core data
    krb_file: KRBFile,
    elements: HashMap<ElementId, Element>,
    
    // Systems
    _style_computer: StyleComputer, 
    layout_engine: Box<dyn LayoutEngine>,
    renderer: ElementRenderer<R>,
    event_system: EventSystem,
    script_system: ScriptSystem,
    template_engine: TemplateEngine,
    
    // State
    layout_result: LayoutResult,
    viewport_size: Vec2,
    needs_layout: bool,
    needs_render: bool,
    current_cursor: kryon_core::CursorType,
    
    // Focus management
    focused_element: Option<ElementId>,
    tab_order: Vec<ElementId>,
    
    // Timing
    last_frame_time: Instant,
    frame_count: u64,
}

impl<R: CommandRenderer> KryonApp<R> {
    pub fn new(krb_path: &str, renderer: R) -> anyhow::Result<Self> {
        Self::new_with_layout_engine(krb_path, renderer, None)
    }
    
    pub fn new_with_layout_engine(krb_path: &str, renderer: R, layout_engine: Option<Box<dyn LayoutEngine>>) -> anyhow::Result<Self> {
        let krb_file = load_krb_file(krb_path)?;
        Self::new_with_krb(krb_file, renderer, layout_engine)
    }
    
    pub fn new_optimized(krb_path: &str, renderer: R) -> anyhow::Result<OptimizedKryonApp<R>> {
        let krb_file = load_krb_file(krb_path)?;
        OptimizedKryonApp::with_krb_and_config(krb_file, renderer, OptimizedAppConfig::default())
    }
    
    pub fn new_with_krb(krb_file: KRBFile, renderer: R, layout_engine: Option<Box<dyn LayoutEngine>>) -> anyhow::Result<Self> {
        let mut elements = krb_file.elements.clone();
        
        let style_computer = StyleComputer::new(&elements, &krb_file.styles);

        // Link parent-child relationships
        Self::link_element_hierarchy(&mut elements, &krb_file)?;
        
        
        // Use TaffyLayoutEngine as the core layout system
        let layout_engine: Box<dyn LayoutEngine> = layout_engine.unwrap_or_else(|| {
            Box::new(TaffyLayoutEngine::new())
        });
        let renderer = ElementRenderer::new(renderer, style_computer.clone());
        let viewport_size = renderer.backend().viewport_size();
        
        let event_system = EventSystem::new();
        let script_system = ScriptSystem::new()?;
        let template_engine = TemplateEngine::new(&krb_file);
        
        let mut app = Self {
            krb_file,
            elements,
            _style_computer: style_computer,
            layout_engine,
            renderer,
            event_system,
            script_system,
            template_engine,
            layout_result: LayoutResult {
                computed_positions: HashMap::new(),
                computed_sizes: HashMap::new(),
            },
            viewport_size,
            needs_layout: true,
            needs_render: true,
            current_cursor: kryon_core::CursorType::Default,
            focused_element: None,
            tab_order: Vec::new(),
            last_frame_time: Instant::now(),
            frame_count: 0,
        };
        
        // Initialize the script system with KRB file data
        app.script_system.initialize(&app.krb_file, &app.elements)?;
        
        // Load compiled scripts from KRB file
        app.script_system.load_compiled_scripts(&app.krb_file.scripts)?;
        
        // Initialize template variables in the script system
        // Always initialize template variables from KRB data to ensure script access
        if app.template_engine.has_bindings() {
            tracing::info!("🔍 [INIT_DEBUG] Template engine has bindings, using template variables");
            let template_vars = app.template_engine.get_variables().clone();
            app.script_system.initialize_template_variables(&template_vars)?;
        } else {
            tracing::info!("🔍 [INIT_DEBUG] Template engine has no bindings, extracting variables from KRB");
            // Extract template variables directly from KRB data
            let mut vars = std::collections::HashMap::new();
            for var in &app.krb_file.template_variables {
                vars.insert(var.name.clone(), var.default_value.clone());
            }
            app.script_system.initialize_template_variables(&vars)?;
        }
        
        // Apply any initial changes set by scripts during initialization
        let changes_applied = app.script_system.apply_pending_changes(&mut app.elements)?;
        if changes_applied {
            tracing::info!("Applied initial changes from scripts");
        }
        
        // Initialize template variables (apply default values to elements)
        app.initialize_template_variables()?;
        
        // Execute script initialization functions now that template variables are ready
        app.script_system.execute_init_functions()?;
        
        // Force initial layout computation
        app.update_layout()?;
        app.needs_layout = false; // Reset after initial layout
        
        Ok(app)
    }
    
    fn link_element_hierarchy(
        _elements: &mut HashMap<ElementId, Element>,
        _krb_file: &KRBFile,
    ) -> anyhow::Result<()> {
        // TODO: Implement proper parent-child relationship parsing from KRB format
        // For now, since we don't have actual child data from KRB,
        // just leave the hierarchy as parsed (empty children lists)
        Ok(())
    }
    
    pub fn update(&mut self, _delta_time: Duration) -> anyhow::Result<()> {
        // Get pending changes from scripts and apply both DOM and template variable changes
        let pending_changes = self.script_system.get_pending_changes()?;
        
        // Apply template variable changes first
        if let Some(template_changes) = pending_changes.get("template_variables") {
            for (name, value) in &template_changes.data {
                self.set_template_variable(name, value)?;
            }
            tracing::debug!("Applied {} template variable changes", template_changes.data.len());
        }
        
        // Apply DOM changes from the same change set
        let changes_applied = self.script_system.apply_pending_dom_changes(&mut self.elements, &pending_changes)?;
        
        // Clear changes after applying them
        self.script_system.clear_pending_changes()?;
        
        if changes_applied {
            self.needs_render = true;
        }
        
        // Process events
        self.event_system.update(&mut self.elements)?;
        
        // Update layout if needed
        if self.needs_layout {
            self.update_layout()?;
            self.needs_layout = false;
            self.needs_render = true;
        }
        
        Ok(())
    }
    
    pub fn render(&mut self) -> anyhow::Result<()> {
        if !self.needs_render {
            return Ok(());
        }
        
        if let Some(root_id) = self.krb_file.root_element_id {
            let clear_color = glam::Vec4::new(0.1, 0.1, 0.1, 1.0); // Dark gray
            
            self.renderer.render_frame(
                &self.elements,
                &self.layout_result,
                root_id,
                clear_color,
            )?;
        }
        
        self.needs_render = false;
        self.frame_count += 1;
        
        // Note: Forced hover test removed - hover system confirmed working
        
        // Update timing
        let now = Instant::now();
        let frame_time = now.duration_since(self.last_frame_time);
        self.last_frame_time = now;
        
        // Log FPS occasionally
        if self.frame_count % 60 == 0 {
            let fps = 1.0 / frame_time.as_secs_f32();
            tracing::debug!("FPS: {:.1}", fps);
        }
        
        Ok(())
    }
    
    pub fn handle_input(&mut self, event: InputEvent) -> anyhow::Result<()> {
        match event {
            InputEvent::Resize { size } => {
                self.viewport_size = size;
                self.renderer.resize(size)?;
                self.needs_layout = true;
            }
            InputEvent::MouseMove { position } => {
                self.handle_mouse_move(position)?;
            }
            InputEvent::MousePress { position, button } => {
                self.handle_mouse_press(position, button)?;
            }
            InputEvent::MouseRelease { position, button } => {
                self.handle_mouse_release(position, button)?;
            }
            InputEvent::KeyPress { key, modifiers } => {
                self.handle_key_press(key, modifiers)?;
            }
            InputEvent::TextInput { text } => {
                self.handle_text_input(&text)?;
            }
            _ => {}
        }
        
        Ok(())
    }
    
    // In fn update_layout(&mut self) -> anyhow::Result<()>

fn update_layout(&mut self) -> anyhow::Result<()> {
    if let Some(root_id) = self.krb_file.root_element_id {
        self.layout_result = self.layout_engine.compute_layout(
            &self.elements,
            root_id,
            self.viewport_size,
        );
        
        // Apply computed layout results back to element layout positions and sizes
        for (&element_id, computed_position) in &self.layout_result.computed_positions {
            if let Some(element) = self.elements.get_mut(&element_id) {
                element.layout_position.x = LayoutDimension::Pixels(computed_position.x);
                element.layout_position.y = LayoutDimension::Pixels(computed_position.y);
            }
        }
        
        for (&element_id, computed_size) in &self.layout_result.computed_sizes {
            if let Some(element) = self.elements.get_mut(&element_id) {
                element.layout_size.width = LayoutDimension::Pixels(computed_size.x);
                element.layout_size.height = LayoutDimension::Pixels(computed_size.y);
            }
        }
    }
    Ok(())
}

    fn handle_mouse_move(&mut self, position: Vec2) -> anyhow::Result<()> {
        let hovered_element = self.find_element_at_position(position);
        
        // Determine the cursor type for the hovered element
        let cursor_type = if let Some(element_id) = hovered_element {
            if let Some(element) = self.elements.get(&element_id) {
                element.cursor
            } else {
                kryon_core::CursorType::Default
            }
        } else {
            kryon_core::CursorType::Default
        };
        
        // Update the cursor if it changed
        self.update_cursor(cursor_type);
        
        // Update hover states (but preserve checked state)
        for (element_id, element) in self.elements.iter_mut() {
            let should_hover = Some(*element_id) == hovered_element;
            let was_hovering = element.current_state == InteractionState::Hover;
            let is_checked = element.current_state == InteractionState::Checked;
            
            if should_hover && !was_hovering && !is_checked {
                // Only set hover if not already in checked state
                element.current_state = InteractionState::Hover;
                self.needs_render = true;
                
                // Trigger hover event
                if let Some(handler) = element.event_handlers.get(&EventType::Hover) {
                    self.script_system.call_function(handler, vec![])?;
                }
            } else if !should_hover && was_hovering && !is_checked {
                // Only reset to normal if not in checked state
                element.current_state = InteractionState::Normal;
                self.needs_render = true;
            }
            // If element is checked, preserve the checked state regardless of hover
        }
        
        Ok(())
    }
    
    fn handle_mouse_press(&mut self, position: Vec2, button: MouseButton) -> anyhow::Result<()> {
        if button == MouseButton::Left {
            if let Some(element_id) = self.find_element_at_position(position) {
                // Check if element can receive focus before getting mutable reference
                let can_receive_focus = self.elements.get(&element_id)
                    .map(|e| e.can_receive_focus())
                    .unwrap_or(false);
                
                if can_receive_focus {
                    self.set_focus(element_id)?;
                    
                    // For text inputs, start editing mode
                    if let Some(element) = self.elements.get_mut(&element_id) {
                        if let Some(input_state) = element.get_input_state_mut() {
                            input_state.set_mode(kryon_core::InputMode::Edit);
                        }
                    }
                } else {
                    // Clear focus if clicking on non-focusable element
                    self.clear_focus()?;
                }
                
                // Only set Active if not already checked
                if let Some(element) = self.elements.get_mut(&element_id) {
                    if element.current_state != InteractionState::Checked {
                        element.current_state = InteractionState::Active;
                        self.needs_render = true;
                        eprintln!("🎯 [INTERACTION_FIX] Element {} pressed - set to Active", element_id);
                    }
                }
            } else {
                // Clear focus if clicking on empty space
                self.clear_focus()?;
                
                // Close any open dropdowns when clicking outside
                self.close_all_dropdowns()?;
            }
        }
        Ok(())
    }
    
    fn handle_mouse_release(&mut self, position: Vec2, button: MouseButton) -> anyhow::Result<()> {
        if button == MouseButton::Left {
            if let Some(element_id) = self.find_element_at_position(position) {
                // First, handle click events and state transitions
                let mut should_set_hover = true;
                
                if let Some(element) = self.elements.get(&element_id) {
                    // If element is checked, don't change its state
                    if element.current_state == InteractionState::Checked {
                        should_set_hover = false;
                    }
                    
                    // Process click handler if it exists
                    if let Some(handler) = element.event_handlers.get(&EventType::Click) {
                        eprintln!("🎯 [INTERACTION_FIX] Element {} click handler found, executing", element_id);
                        
                        // Call the click handler function
                        self.script_system.call_function(handler, vec![])?;
                        
                        // Apply any pending changes from scripts
                        let changes_applied = self.script_system.apply_pending_changes(&mut self.elements)?;
                        
                        // Apply template variable changes from scripts
                        let pending_changes = self.script_system.get_pending_changes()?;
                        let template_variable_changes = if let Some(template_changes) = pending_changes.get("template_variables") {
                            for (name, value) in &template_changes.data {
                                self.set_template_variable(name, value)?;
                            }
                            !template_changes.data.is_empty()
                        } else {
                            false
                        };
                        
                        if changes_applied || template_variable_changes {
                            tracing::info!("Changes applied, triggering re-render");
                            self.needs_render = true;
                            
                            // Force layout update for visibility changes and template variable changes
                            if template_variable_changes {
                                self.update_layout()?;
                                self.needs_layout = false;
                                tracing::info!("🚀 [SCRIPT_IMMEDIATE] Immediate layout update applied for template changes");
                            }
                        }
                    }
                }
                
                // Handle Select element specific click behavior
                if let Some(element) = self.elements.get(&element_id) {
                    if element.element_type == kryon_core::ElementType::Select {
                        // Check if this is a click on the dropdown menu vs the button (without mutable borrow)
                        let clicked_dropdown_option = if let Some(select_state) = element.get_select_state() {
                            if select_state.is_open {
                                self.find_dropdown_option_at_position(element_id, position)
                            } else {
                                None
                            }
                        } else {
                            None
                        };
                        
                        // Now get mutable reference to handle the action
                        if let Some(element) = self.elements.get_mut(&element_id) {
                            if let Some(select_state) = element.get_select_state_mut() {
                                if let Some(option_index) = clicked_dropdown_option {
                                    // Select the clicked option
                                    if select_state.select_option(option_index) {
                                        self.needs_render = true;
                                        eprintln!("🎯 [SELECT] Selected option {} at index {}", 
                                            select_state.options[option_index].text, option_index);
                                    }
                                } else {
                                    // Toggle dropdown open/closed for button clicks
                                    select_state.is_open = !select_state.is_open;
                                    self.needs_render = true;
                                    eprintln!("🎯 [SELECT] Toggled dropdown state to: {}", select_state.is_open);
                                }
                            }
                        }
                    }
                }
                
                // Now set the appropriate state after all script processing is done
                if should_set_hover {
                    if let Some(element) = self.elements.get_mut(&element_id) {
                        if element.current_state != InteractionState::Checked {
                            element.current_state = InteractionState::Hover;
                            self.needs_render = true;
                            eprintln!("🎯 [INTERACTION_FIX] Element {} released - set to Hover", element_id);
                        }
                    }
                }
            }
        }
        Ok(())
    }
    
    fn handle_key_press(&mut self, key: KeyCode, modifiers: KeyModifiers) -> anyhow::Result<()> {
        // Handle focus navigation first
        match key {
            KeyCode::Tab => {
                if modifiers.shift {
                    self.focus_previous_element()?;
                } else {
                    self.focus_next_element()?;
                }
                return Ok(());
            }
            KeyCode::Escape => {
                // Close any open dropdowns first, then clear focus
                self.close_all_dropdowns()?;
                self.clear_focus()?;
                return Ok(());
            }
            _ => {}
        }
        
        // Handle Select element keyboard navigation
        if let Some(focused_id) = self.focused_element {
            if let Some(element) = self.elements.get(&focused_id) {
                if element.element_type == kryon_core::ElementType::Select {
                    // Handle dropdown-specific keys
                    let handled = match key {
                        KeyCode::ArrowDown => {
                            self.handle_select_key_down(focused_id)?
                        }
                        KeyCode::ArrowUp => {
                            self.handle_select_key_up(focused_id)?
                        }
                        KeyCode::Enter | KeyCode::Space => {
                            self.handle_select_key_enter(focused_id)?
                        }
                        _ => false,
                    };
                    
                    if handled {
                        return Ok(());
                    }
                }
            }
        }
        
        // Handle input-specific key events for focused element
        if let Some(focused_id) = self.focused_element {
            if let Some(element) = self.elements.get_mut(&focused_id) {
                // Convert KeyCode to string for element handler
                let key_string = match key {
                    KeyCode::Backspace => "Backspace",
                    KeyCode::Delete => "Delete",
                    KeyCode::ArrowLeft => "ArrowLeft",
                    KeyCode::ArrowRight => "ArrowRight",
                    KeyCode::ArrowUp => "ArrowUp",
                    KeyCode::ArrowDown => "ArrowDown",
                    KeyCode::Home => "Home",
                    KeyCode::End => "End",
                    KeyCode::Enter => "Enter",
                    _ => return Ok(()), // Other keys handled by text input
                };
                
                if element.handle_key_press(key_string, modifiers.shift) {
                    self.needs_render = true;
                }
            }
        }
        
        Ok(())
    }
    
    fn handle_text_input(&mut self, text: &str) -> anyhow::Result<()> {
        if let Some(focused_id) = self.focused_element {
            if let Some(element) = self.elements.get_mut(&focused_id) {
                if element.handle_text_input(text) {
                    self.needs_render = true;
                }
            }
        }
        Ok(())
    }
    
    fn find_element_at_position(&self, position: Vec2) -> Option<ElementId> {
        // Find the topmost element at the given position
        let mut found_elements = Vec::new();
        
        for (element_id, element) in &self.elements {
            // Debug all click positions with detailed element info
            if position.x >= 0.0 && position.y >= 0.0 {
                eprintln!("[HIT_TEST_DEBUG] Checking element {} at click position ({:.1}, {:.1}), visible: {}", 
                    element_id, position.x, position.y, element.visible);
            }
            
            if !element.visible {
                if *element_id == 6 { // Debug dropdown menu visibility
                    eprintln!("[HIT_TEST_DEBUG] Element 6 (dropdown menu) is invisible, skipping");
                }
                continue;
            }
            if *element_id == 6 { // Debug dropdown menu visibility
                eprintln!("[HIT_TEST_DEBUG] Element 6 (dropdown menu) is visible, checking hit");
            }
            
            let element_pos = self.layout_result.computed_positions
                .get(element_id)
                .copied()
                .unwrap_or(Vec2::new(element.layout_position.x.to_pixels(1.0), element.layout_position.y.to_pixels(1.0)));
            let element_size = self.layout_result.computed_sizes
                .get(element_id)
                .copied()
                .unwrap_or(Vec2::new(element.layout_size.width.to_pixels(1.0), element.layout_size.height.to_pixels(1.0)));
            
            if position.x >= element_pos.x
                && position.x <= element_pos.x + element_size.x
                && position.y >= element_pos.y
                && position.y <= element_pos.y + element_size.y
            {
                found_elements.push(*element_id);
            }
        }
        
        // Return the highest element ID (topmost)
        found_elements.into_iter().max()
    }
    
    fn find_dropdown_option_at_position(&self, select_element_id: ElementId, position: Vec2) -> Option<usize> {
        if let Some(element) = self.elements.get(&select_element_id) {
            if let Some(select_state) = element.get_select_state() {
                if !select_state.is_open || select_state.options.is_empty() {
                    return None;
                }
                
                // Calculate dropdown menu position and size
                let element_pos = self.layout_result.computed_positions
                    .get(&select_element_id)
                    .copied()
                    .unwrap_or(Vec2::new(element.layout_position.x.to_pixels(1.0), element.layout_position.y.to_pixels(1.0)));
                let element_size = self.layout_result.computed_sizes
                    .get(&select_element_id)
                    .copied()
                    .unwrap_or(Vec2::new(element.layout_size.width.to_pixels(1.0), element.layout_size.height.to_pixels(1.0)));
                
                let menu_position = Vec2::new(element_pos.x, element_pos.y + element_size.y + 2.0);
                let option_height = 32.0;
                let menu_height = (select_state.options.len() as f32 * option_height).min(select_state.size as f32 * option_height);
                let menu_size = Vec2::new(element_size.x, menu_height);
                
                // Check if click is within dropdown menu bounds
                if position.x >= menu_position.x
                    && position.x <= menu_position.x + menu_size.x
                    && position.y >= menu_position.y
                    && position.y <= menu_position.y + menu_size.y
                {
                    // Calculate which option was clicked
                    let relative_y = position.y - menu_position.y;
                    let option_index = (relative_y / option_height) as usize;
                    
                    if option_index < select_state.options.len() {
                        eprintln!("🎯 [SELECT] Clicked on dropdown option {} at position ({:.1}, {:.1})", 
                            option_index, position.x, position.y);
                        return Some(option_index);
                    }
                }
            }
        }
        None
    }
    
    pub fn get_element(&self, id: &str) -> Option<&Element> {
        self.elements.iter()
            .find(|(_, element)| element.id == id)
            .map(|(_, element)| element)
    }
    
    pub fn get_element_mut(&mut self, id: &str) -> Option<&mut Element> {
        self.elements.iter_mut()
            .find(|(_, element)| element.id == id)
            .map(|(_, element)| element)
    }
    
    pub fn viewport_size(&self) -> Vec2 {
        self.viewport_size
    }
    
    pub fn mark_needs_layout(&mut self) {
        self.needs_layout = true;
    }
    
    pub fn mark_needs_render(&mut self) {
        self.needs_render = true;
    }
    
    /// Switch to optimized layout engine
    pub fn use_optimized_layout_engine(&mut self) {
        self.layout_engine = Box::new(OptimizedTaffyLayoutEngine::new());
        self.needs_layout = true;
    }
    
    pub fn renderer(&self) -> &ElementRenderer<R> {
        &self.renderer
    }
    
    pub fn renderer_mut(&mut self) -> &mut ElementRenderer<R> {
        &mut self.renderer
    }
    
    /// Update the cursor type - returns true if the cursor changed
    fn update_cursor(&mut self, new_cursor: kryon_core::CursorType) -> bool {
        if self.current_cursor != new_cursor {
            self.current_cursor = new_cursor;
            // We'll handle the actual cursor update in the main loop
            true
        } else {
            false
        }
    }
    
    /// Get the current cursor type
    pub fn current_cursor(&self) -> kryon_core::CursorType {
        self.current_cursor
    }
    
    // Template variable methods
    
    /// Set a template variable and update affected elements
    pub fn set_template_variable(&mut self, name: &str, value: &str) -> anyhow::Result<()> {
        
        // Force update the template variable (ignore change detection for now)
        self.template_engine.set_variable(name, value);
        
        // Always update elements if we have bindings for this variable
        let bindings_for_var = self.template_engine.get_bindings_for_variable(name);
        let bindings_count = bindings_for_var.len();
        
        if !bindings_for_var.is_empty() {
            // Update the elements with new template values
            self.template_engine.update_elements(&mut self.elements, &self.krb_file);
            
            // Mark layout and render as needed
            self.mark_needs_layout();
            self.mark_needs_render();
            
            tracing::info!("Template variable '{}' updated to '{}', affected {} elements", 
                name, value, bindings_count);
        }
        
        Ok(())
    }
    
    /// Get a template variable value
    pub fn get_template_variable(&self, name: &str) -> Option<&str> {
        self.template_engine.get_variable(name)
    }
    
    /// Get all template variables
    pub fn get_template_variables(&self) -> &HashMap<String, String> {
        self.template_engine.get_variables()
    }
    
    /// Get all template variable names
    pub fn get_template_variable_names(&self) -> Vec<String> {
        self.template_engine.get_variable_names()
    }
    
    /// Check if template variables are available
    pub fn has_template_variables(&self) -> bool {
        self.template_engine.has_bindings()
    }
    
    /// Initialize template variables (apply initial values to elements)
    pub fn initialize_template_variables(&mut self) -> anyhow::Result<()> {
        if self.template_engine.has_bindings() {
            self.template_engine.update_elements(&mut self.elements, &self.krb_file);
            self.mark_needs_layout();
            self.mark_needs_render();
            tracing::info!("Template variables initialized");
        }
        Ok(())
    }
    
    // Focus management methods
    
    /// Set focus to a specific element
    fn set_focus(&mut self, element_id: ElementId) -> anyhow::Result<()> {
        // Clear previous focus
        if let Some(prev_focused) = self.focused_element {
            if let Some(prev_element) = self.elements.get_mut(&prev_focused) {
                prev_element.set_focus(false);
            }
        }
        
        // Set new focus
        if let Some(element) = self.elements.get_mut(&element_id) {
            if element.can_receive_focus() {
                element.set_focus(true);
                self.focused_element = Some(element_id);
                self.needs_render = true;
                
                // Initialize input state if this is an input element
                if element.element_type == kryon_core::ElementType::Input && element.input_state.is_none() {
                    element.initialize_input_state(kryon_core::InputType::Text);
                }
            }
        }
        
        Ok(())
    }
    
    /// Clear focus from all elements
    fn clear_focus(&mut self) -> anyhow::Result<()> {
        if let Some(focused_id) = self.focused_element {
            if let Some(element) = self.elements.get_mut(&focused_id) {
                element.set_focus(false);
                
                // Exit editing mode for input elements
                if let Some(input_state) = element.get_input_state_mut() {
                    input_state.set_mode(kryon_core::InputMode::Normal);
                }
            }
        }
        
        self.focused_element = None;
        self.needs_render = true;
        Ok(())
    }
    
    /// Focus the next focusable element (Tab navigation)
    fn focus_next_element(&mut self) -> anyhow::Result<()> {
        self.build_tab_order();
        
        if self.tab_order.is_empty() {
            return Ok(());
        }
        
        let next_index = if let Some(current_focused) = self.focused_element {
            // Find current element in tab order and move to next
            if let Some(current_index) = self.tab_order.iter().position(|&id| id == current_focused) {
                (current_index + 1) % self.tab_order.len()
            } else {
                0 // Current focused not in tab order, start from beginning
            }
        } else {
            0 // No current focus, start from beginning
        };
        
        let next_element_id = self.tab_order[next_index];
        self.set_focus(next_element_id)?;
        
        Ok(())
    }
    
    /// Focus the previous focusable element (Shift+Tab navigation)
    fn focus_previous_element(&mut self) -> anyhow::Result<()> {
        self.build_tab_order();
        
        if self.tab_order.is_empty() {
            return Ok(());
        }
        
        let prev_index = if let Some(current_focused) = self.focused_element {
            // Find current element in tab order and move to previous
            if let Some(current_index) = self.tab_order.iter().position(|&id| id == current_focused) {
                if current_index == 0 {
                    self.tab_order.len() - 1
                } else {
                    current_index - 1
                }
            } else {
                self.tab_order.len() - 1 // Current focused not in tab order, start from end
            }
        } else {
            self.tab_order.len() - 1 // No current focus, start from end
        };
        
        let prev_element_id = self.tab_order[prev_index];
        self.set_focus(prev_element_id)?;
        
        Ok(())
    }
    
    /// Build tab order based on element positions and types
    fn build_tab_order(&mut self) {
        let mut focusable_elements: Vec<(ElementId, Vec2)> = Vec::new();
        
        // Collect all focusable elements with their positions
        for (&element_id, element) in &self.elements {
            if element.can_receive_focus() {
                let position = self.layout_result.computed_positions
                    .get(&element_id)
                    .copied()
                    .unwrap_or(Vec2::new(
                        element.layout_position.x.to_pixels(1.0),
                        element.layout_position.y.to_pixels(1.0)
                    ));
                focusable_elements.push((element_id, position));
            }
        }
        
        // Sort by Y position first, then by X position (reading order)
        focusable_elements.sort_by(|a, b| {
            let y_cmp = a.1.y.partial_cmp(&b.1.y).unwrap_or(std::cmp::Ordering::Equal);
            if y_cmp == std::cmp::Ordering::Equal {
                a.1.x.partial_cmp(&b.1.x).unwrap_or(std::cmp::Ordering::Equal)
            } else {
                y_cmp
            }
        });
        
        self.tab_order = focusable_elements.into_iter().map(|(id, _)| id).collect();
    }
    
    /// Close all open dropdown menus
    fn close_all_dropdowns(&mut self) -> anyhow::Result<()> {
        let mut closed_any = false;
        
        for (_, element) in self.elements.iter_mut() {
            if element.element_type == kryon_core::ElementType::Select {
                if let Some(select_state) = element.get_select_state_mut() {
                    if select_state.is_open {
                        select_state.close_dropdown();
                        closed_any = true;
                        eprintln!("🎯 [SELECT] Closed dropdown due to outside click");
                    }
                }
            }
        }
        
        if closed_any {
            self.needs_render = true;
        }
        
        Ok(())
    }
    
    // Select element keyboard navigation methods
    
    fn handle_select_key_down(&mut self, element_id: ElementId) -> anyhow::Result<bool> {
        if let Some(element) = self.elements.get_mut(&element_id) {
            if let Some(select_state) = element.get_select_state_mut() {
                if select_state.is_open {
                    // Navigate down in open dropdown
                    select_state.move_highlight(1);
                    self.needs_render = true;
                    eprintln!("🎯 [SELECT] Arrow down - highlight moved to {:?}", select_state.highlighted_index);
                } else {
                    // Open dropdown and highlight first option
                    select_state.is_open = true;
                    if select_state.highlighted_index.is_none() {
                        select_state.highlighted_index = select_state.options.iter().position(|opt| !opt.disabled);
                    }
                    self.needs_render = true;
                    eprintln!("🎯 [SELECT] Arrow down - opened dropdown");
                }
                return Ok(true);
            }
        }
        Ok(false)
    }
    
    fn handle_select_key_up(&mut self, element_id: ElementId) -> anyhow::Result<bool> {
        if let Some(element) = self.elements.get_mut(&element_id) {
            if let Some(select_state) = element.get_select_state_mut() {
                if select_state.is_open {
                    // Navigate up in open dropdown
                    select_state.move_highlight(-1);
                    self.needs_render = true;
                    eprintln!("🎯 [SELECT] Arrow up - highlight moved to {:?}", select_state.highlighted_index);
                } else {
                    // Open dropdown and highlight last option
                    select_state.is_open = true;
                    select_state.highlighted_index = select_state.options.iter().rposition(|opt| !opt.disabled);
                    self.needs_render = true;
                    eprintln!("🎯 [SELECT] Arrow up - opened dropdown at end");
                }
                return Ok(true);
            }
        }
        Ok(false)
    }
    
    fn handle_select_key_enter(&mut self, element_id: ElementId) -> anyhow::Result<bool> {
        if let Some(element) = self.elements.get_mut(&element_id) {
            if let Some(select_state) = element.get_select_state_mut() {
                if select_state.is_open {
                    // Select highlighted option
                    if let Some(highlighted) = select_state.highlighted_index {
                        if select_state.select_option(highlighted) {
                            self.needs_render = true;
                            eprintln!("🎯 [SELECT] Enter - selected option {} ({})", 
                                highlighted, select_state.options[highlighted].text);
                        }
                    }
                } else {
                    // Open dropdown
                    select_state.is_open = true;
                    if select_state.highlighted_index.is_none() {
                        select_state.highlighted_index = select_state.options.iter().position(|opt| !opt.disabled);
                    }
                    self.needs_render = true;
                    eprintln!("🎯 [SELECT] Enter - opened dropdown");
                }
                return Ok(true);
            }
        }
        Ok(false)
    }
}