use crate::elements::{Element, ElementId};
use crate::types::{InputEvent, PropertyValue, LayoutResult};
use crate::render::RenderCommand;
use serde::{Serialize, Deserialize};
use std::collections::HashMap;
use anyhow::Result;

/// Core trait that all Kryon widgets must implement
/// This provides a unified interface for widget lifecycle management,
/// rendering, and event handling across all widget types.
pub trait KryonWidget: Send + Sync + 'static {
    /// Unique identifier for this widget type
    const WIDGET_NAME: &'static str;
    
    /// Widget-specific state type that persists across frames
    type State: Clone + PartialEq + Default + Serialize + for<'de> Deserialize<'de>;
    
    /// Widget configuration derived from element properties
    type Config: Clone + Default + From<&PropertyBag>;
    
    /// Create initial state for a new widget instance
    fn create_state(&self) -> Self::State {
        Self::State::default()
    }
    
    /// Generate render commands for this widget
    fn render(
        &self,
        state: &Self::State,
        config: &Self::Config,
        layout: &LayoutResult,
        element_id: ElementId,
    ) -> Result<Vec<RenderCommand>>;
    
    /// Handle input events and update widget state
    fn handle_event(
        &self,
        state: &mut Self::State,
        config: &Self::Config,
        event: &InputEvent,
        layout: &LayoutResult,
    ) -> Result<EventResult>;
    
    /// Called when widget gains focus
    fn on_focus(&self, state: &mut Self::State, config: &Self::Config) -> Result<()> {
        Ok(())
    }
    
    /// Called when widget loses focus
    fn on_blur(&self, state: &mut Self::State, config: &Self::Config) -> Result<()> {
        Ok(())
    }
    
    /// Called when widget is mounted to the DOM
    fn on_mount(&self, state: &mut Self::State, config: &Self::Config) -> Result<()> {
        Ok(())
    }
    
    /// Called when widget is unmounted from the DOM
    fn on_unmount(&self, state: &mut Self::State, config: &Self::Config) -> Result<()> {
        Ok(())
    }
    
    /// Determine if widget can receive keyboard focus
    fn can_focus(&self) -> bool {
        true
    }
    
    /// Compute custom layout constraints for this widget
    fn layout_constraints(&self, config: &Self::Config) -> Option<LayoutConstraints> {
        None
    }
    
    /// Validate widget configuration
    fn validate_config(&self, config: &Self::Config) -> Result<()> {
        Ok(())
    }
}

/// Result of handling an input event
#[derive(Debug, Clone, PartialEq)]
pub enum EventResult {
    /// Event was handled and consumed
    Handled,
    /// Event was handled and should trigger a re-render
    HandledWithRedraw,
    /// Event was not handled, should propagate to parent
    NotHandled,
    /// Event was handled and emitted a custom event
    EmitEvent {
        event_name: String,
        data: serde_json::Value,
    },
}

/// Layout constraints for custom widget sizing
#[derive(Debug, Clone, PartialEq)]
pub struct LayoutConstraints {
    pub min_width: Option<f32>,
    pub max_width: Option<f32>,
    pub min_height: Option<f32>,
    pub max_height: Option<f32>,
    pub aspect_ratio: Option<f32>,
}

/// Property bag for widget configuration
#[derive(Debug, Clone, Default)]
pub struct PropertyBag {
    pub properties: HashMap<String, PropertyValue>,
}

impl PropertyBag {
    pub fn new() -> Self {
        Self {
            properties: HashMap::new(),
        }
    }
    
    pub fn get<T>(&self, key: &str) -> Option<T>
    where
        T: TryFrom<PropertyValue>,
    {
        self.properties.get(key)?.clone().try_into().ok()
    }
    
    pub fn get_string(&self, key: &str) -> Option<String> {
        if let Some(PropertyValue::String(s)) = self.properties.get(key) {
            Some(s.clone())
        } else {
            None
        }
    }
    
    pub fn get_bool(&self, key: &str) -> Option<bool> {
        if let Some(PropertyValue::Bool(b)) = self.properties.get(key) {
            Some(*b)
        } else {
            None
        }
    }
    
    pub fn get_f32(&self, key: &str) -> Option<f32> {
        match self.properties.get(key) {
            Some(PropertyValue::Float(f)) => Some(*f),
            Some(PropertyValue::Int(i)) => Some(*i as f32),
            _ => None,
        }
    }
    
    pub fn get_i32(&self, key: &str) -> Option<i32> {
        match self.properties.get(key) {
            Some(PropertyValue::Int(i)) => Some(*i),
            Some(PropertyValue::Float(f)) => Some(*f as i32),
            _ => None,
        }
    }
    
    pub fn get_vec_string(&self, key: &str) -> Vec<String> {
        if let Some(PropertyValue::Array(arr)) = self.properties.get(key) {
            arr.iter()
                .filter_map(|v| {
                    if let PropertyValue::String(s) = v {
                        Some(s.clone())
                    } else {
                        None
                    }
                })
                .collect()
        } else {
            Vec::new()
        }
    }
    
    pub fn set(&mut self, key: String, value: PropertyValue) {
        self.properties.insert(key, value);
    }
}

/// Registry for managing widget implementations
#[derive(Default)]
pub struct WidgetRegistry {
    widgets: HashMap<&'static str, Box<dyn WidgetFactory>>,
}

/// Factory trait for creating widget instances
pub trait WidgetFactory: Send + Sync {
    fn create(&self) -> Box<dyn KryonWidget>;
    fn widget_name(&self) -> &'static str;
}

impl WidgetRegistry {
    pub fn new() -> Self {
        let mut registry = Self::default();
        
        // Register built-in widgets based on feature flags
        #[cfg(feature = "dropdown")]
        registry.register::<crate::widgets::dropdown::DropdownWidget>();
        
        #[cfg(feature = "data-grid")]
        registry.register::<crate::widgets::data_grid::DataGridWidget>();
        
        #[cfg(feature = "date-picker")]
        registry.register::<crate::widgets::date_picker::DatePickerWidget>();
        
        #[cfg(feature = "rich-text")]
        registry.register::<crate::widgets::rich_text::RichTextWidget>();
        
        #[cfg(feature = "color-picker")]
        registry.register::<crate::widgets::color_picker::ColorPickerWidget>();
        
        #[cfg(feature = "file-upload")]
        registry.register::<crate::widgets::file_upload::FileUploadWidget>();
        
        #[cfg(feature = "number-input")]
        registry.register::<crate::widgets::number_input::NumberInputWidget>();
        
        #[cfg(feature = "range-slider")]
        registry.register::<crate::widgets::range_slider::RangeSliderWidget>();
        
        registry
    }
    
    pub fn register<W>(&mut self)
    where
        W: KryonWidget + Default + 'static,
    {
        struct Factory<W>(std::marker::PhantomData<W>);
        
        impl<W> WidgetFactory for Factory<W>
        where
            W: KryonWidget + Default + 'static,
        {
            fn create(&self) -> Box<dyn KryonWidget> {
                Box::new(W::default())
            }
            
            fn widget_name(&self) -> &'static str {
                W::WIDGET_NAME
            }
        }
        
        self.widgets.insert(
            W::WIDGET_NAME,
            Box::new(Factory::<W>(std::marker::PhantomData)),
        );
    }
    
    pub fn create_widget(&self, widget_name: &str) -> Option<Box<dyn KryonWidget>> {
        self.widgets.get(widget_name).map(|factory| factory.create())
    }
    
    pub fn get_widget_names(&self) -> Vec<&'static str> {
        self.widgets.values().map(|factory| factory.widget_name()).collect()
    }
    
    pub fn has_widget(&self, widget_name: &str) -> bool {
        self.widgets.contains_key(widget_name)
    }
}

/// Utility macro for implementing basic widget factories
#[macro_export]
macro_rules! impl_widget_factory {
    ($widget_type:ty) => {
        impl Default for $widget_type {
            fn default() -> Self {
                Self::new()
            }
        }
    };
}

// Widget modules - only included if features are enabled
#[cfg(feature = "dropdown")]
pub mod dropdown;

#[cfg(feature = "data-grid")]
pub mod data_grid;

#[cfg(feature = "date-picker")]
pub mod date_picker;

#[cfg(feature = "rich-text")]
pub mod rich_text;

#[cfg(feature = "color-picker")]
pub mod color_picker;

#[cfg(feature = "file-upload")]
pub mod file_upload;

#[cfg(feature = "number-input")]
pub mod number_input;

#[cfg(feature = "range-slider")]
pub mod range_slider;

// Re-export commonly used types
pub use self::{
    EventResult,
    KryonWidget,
    LayoutConstraints,
    PropertyBag,
    WidgetFactory,
    WidgetRegistry,
};

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_widget_registry_creation() {
        let registry = WidgetRegistry::new();
        
        // Test that registry creates without panicking
        assert!(registry.get_widget_names().len() >= 0);
    }
    
    #[test]
    fn test_property_bag() {
        let mut bag = PropertyBag::new();
        bag.set("test_string".to_string(), PropertyValue::String("hello".to_string()));
        bag.set("test_bool".to_string(), PropertyValue::Bool(true));
        bag.set("test_int".to_string(), PropertyValue::Int(42));
        bag.set("test_float".to_string(), PropertyValue::Float(3.14));
        
        assert_eq!(bag.get_string("test_string"), Some("hello".to_string()));
        assert_eq!(bag.get_bool("test_bool"), Some(true));
        assert_eq!(bag.get_i32("test_int"), Some(42));
        assert_eq!(bag.get_f32("test_float"), Some(3.14));
        
        // Test missing keys
        assert_eq!(bag.get_string("missing"), None);
    }
}