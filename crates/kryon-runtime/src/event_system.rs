// crates/kryon-runtime/src/event_system.rs
use kryon_core::{Element, ElementId};
use std::collections::HashMap;
use anyhow::Result;

#[derive(Debug)]
pub struct EventSystem {
    event_queue: Vec<UIEvent>,
    focused_element: Option<ElementId>,
}

#[derive(Debug, Clone)]
pub enum UIEvent {
    ElementClicked(ElementId),
    ElementHovered(ElementId),
    ElementFocused(ElementId),
    ElementBlurred(ElementId),
    TextChanged(ElementId, String),
    // Input-specific events
    InputFocused(ElementId),
    InputBlurred(ElementId),
    InputTextInserted(ElementId, String),
    InputKeyPressed(ElementId, String, bool), // (element_id, key, shift_pressed)
    InputEditModeStarted(ElementId),
    InputEditModeEnded(ElementId),
    InputCursorMoved(ElementId, usize),
    InputSelectionChanged(ElementId, Option<(usize, usize)>),
}

impl EventSystem {
    pub fn new() -> Self {
        Self {
            event_queue: Vec::new(),
            focused_element: None,
        }
    }
    
    pub fn queue_event(&mut self, event: UIEvent) {
        self.event_queue.push(event);
    }
    
    pub fn update(&mut self, elements: &mut HashMap<ElementId, Element>) -> Result<()> {
        // Process all queued events
        let events: Vec<_> = self.event_queue.drain(..).collect();
        for event in events {
            self.process_event(event, elements)?;
        }
        Ok(())
    }
    
    fn process_event(&mut self, event: UIEvent, elements: &mut HashMap<ElementId, Element>) -> Result<()> {
        match event {
            UIEvent::ElementClicked(element_id) => {
                if let Some(element) = elements.get_mut(&element_id) {
                    tracing::debug!("Element clicked: {}", element.id);
                    
                    // If this is an input element that can receive focus, focus it
                    if element.can_receive_focus() {
                        self.set_focus(element_id, elements)?;
                    }
                }
            }
            UIEvent::ElementHovered(element_id) => {
                if let Some(element) = elements.get_mut(&element_id) {
                    tracing::debug!("Element hovered: {}", element.id);
                    
                    // Update hover state for input elements
                    if let Some(input_state) = element.get_input_state_mut() {
                        if input_state.input_mode == kryon_core::InputMode::Normal {
                            input_state.set_mode(kryon_core::InputMode::Hover);
                        }
                    }
                }
            }
            UIEvent::InputFocused(element_id) => {
                self.set_focus(element_id, elements)?;
            }
            UIEvent::InputBlurred(element_id) => {
                self.clear_focus(element_id, elements)?;
            }
            UIEvent::InputTextInserted(element_id, text) => {
                if let Some(element) = elements.get_mut(&element_id) {
                    if element.handle_text_input(&text) {
                        tracing::debug!("Text inserted in input {}: '{}'", element.id, text);
                        self.trigger_change_event(element_id, elements)?;
                    }
                }
            }
            UIEvent::InputKeyPressed(element_id, key, shift_pressed) => {
                if let Some(element) = elements.get_mut(&element_id) {
                    if element.handle_key_press(&key, shift_pressed) {
                        tracing::debug!("Key pressed in input {}: '{}' (shift: {})", element.id, key, shift_pressed);
                        
                        // Trigger change event for text modifications
                        if matches!(key.as_str(), "Backspace" | "Delete") {
                            self.trigger_change_event(element_id, elements)?;
                        }
                    }
                }
            }
            UIEvent::InputEditModeStarted(element_id) => {
                if let Some(element) = elements.get_mut(&element_id) {
                    if let Some(input_state) = element.get_input_state_mut() {
                        input_state.set_mode(kryon_core::InputMode::Edit);
                        tracing::debug!("Edit mode started for input: {}", element.id);
                    }
                }
            }
            UIEvent::InputEditModeEnded(element_id) => {
                if let Some(element) = elements.get_mut(&element_id) {
                    if let Some(input_state) = element.get_input_state_mut() {
                        input_state.set_mode(kryon_core::InputMode::Focused);
                        tracing::debug!("Edit mode ended for input: {}", element.id);
                        self.trigger_change_event(element_id, elements)?;
                    }
                }
            }
            UIEvent::TextChanged(element_id, new_text) => {
                if let Some(element) = elements.get_mut(&element_id) {
                    // For non-input elements, update the text directly
                    if element.get_input_state().is_none() {
                        element.text = new_text;
                        tracing::debug!("Text changed in element: {}", element.id);
                    }
                }
            }
            _ => {}
        }
        Ok(())
    }
    
    /// Set focus to a specific element, clearing focus from others
    pub fn set_focus(&mut self, element_id: ElementId, elements: &mut HashMap<ElementId, Element>) -> Result<()> {
        // Clear focus from previously focused element
        if let Some(prev_focused) = self.focused_element {
            if let Some(prev_element) = elements.get_mut(&prev_focused) {
                prev_element.set_focus(false);
                tracing::debug!("Cleared focus from element: {}", prev_element.id);
            }
        }
        
        // Set focus to new element
        if let Some(element) = elements.get_mut(&element_id) {
            if element.can_receive_focus() {
                element.set_focus(true);
                self.focused_element = Some(element_id);
                tracing::debug!("Set focus to element: {}", element.id);
                
                // For input elements, also set focused mode
                if let Some(input_state) = element.get_input_state_mut() {
                    input_state.set_mode(kryon_core::InputMode::Focused);
                }
            }
        }
        
        Ok(())
    }
    
    /// Clear focus from a specific element
    pub fn clear_focus(&mut self, element_id: ElementId, elements: &mut HashMap<ElementId, Element>) -> Result<()> {
        if self.focused_element == Some(element_id) {
            self.focused_element = None;
            
            if let Some(element) = elements.get_mut(&element_id) {
                element.set_focus(false);
                tracing::debug!("Cleared focus from element: {}", element.id);
                
                // For input elements, set normal mode
                if let Some(input_state) = element.get_input_state_mut() {
                    input_state.set_mode(kryon_core::InputMode::Normal);
                }
            }
        }
        
        Ok(())
    }
    
    /// Get the currently focused element
    pub fn get_focused_element(&self) -> Option<ElementId> {
        self.focused_element
    }
    
    /// Handle double-click to start edit mode for input elements
    pub fn handle_double_click(&mut self, element_id: ElementId, elements: &mut HashMap<ElementId, Element>) -> Result<()> {
        if let Some(element) = elements.get(&element_id) {
            if element.element_type == kryon_core::ElementType::Input {
                self.queue_event(UIEvent::InputEditModeStarted(element_id));
            }
        }
        Ok(())
    }
    
    /// Trigger change event for input elements (calls script callbacks)
    fn trigger_change_event(&self, element_id: ElementId, elements: &HashMap<ElementId, Element>) -> Result<()> {
        if let Some(element) = elements.get(&element_id) {
            if let Some(callback_name) = element.event_handlers.get(&kryon_core::EventType::Change) {
                tracing::debug!("Triggering change event callback '{}' for element: {}", callback_name, element.id);
                // TODO: Call script system to execute the callback
                // This would need to be integrated with the script execution system
            }
        }
        Ok(())
    }
}