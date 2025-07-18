//! Lua DOM API bridge implementation
//!
//! This module provides the bridge between Lua scripts and the Kryon UI system,
//! enabling scripts to manipulate DOM elements, handle events, and manage state.
//! It includes the complete DOM API implementation from the original lua_bridge.lua.

use std::collections::HashMap;
use std::rc::Rc;
use anyhow::Result;
use mlua::{Lua, Table as LuaTable, Function as LuaFunction};
use crate::script::{
    engine_trait::{BridgeData, ChangeSet, ScriptValue},
    error::ScriptError,
};
use serde_json;

/// Lua DOM API bridge
/// 
/// This component provides:
/// - DOM element manipulation API
/// - Event handling system
/// - Component property access
/// - Pending change management
pub struct LuaBridge {
    /// Reference to the Lua VM
    lua: Rc<Lua>,
}

impl LuaBridge {
    /// Create a new Lua bridge
    pub fn new(lua: Rc<Lua>) -> Result<Self> {
        let bridge = Self {
            lua,
        };
        
        // Load the bridge API into Lua
        bridge.setup_bridge_api()?;
        
        Ok(bridge)
    }
    
    /// Setup the bridge with element and style data
    pub fn setup(&mut self, bridge_data: &BridgeData) -> Result<()> {
        let globals = self.lua.globals();
        
        // Create element IDs table
        let element_ids_table = self.lua.create_table()?;
        for (element_id, numeric_id) in &bridge_data.element_ids {
            element_ids_table.set(element_id.clone(), *numeric_id)?;
        }
        globals.set("_element_ids", element_ids_table)?;
        
        // Create style IDs table
        let style_ids_table = self.lua.create_table()?;
        for (style_name, style_id) in &bridge_data.style_ids {
            style_ids_table.set(style_name.clone(), *style_id)?;
        }
        globals.set("_style_ids", style_ids_table)?;
        
        // Create component properties table
        let component_properties_table = self.lua.create_table()?;
        for (element_id, properties) in &bridge_data.component_properties {
            let props_table = self.lua.create_table()?;
            for (prop_name, prop_value) in properties {
                self.set_script_value_in_table(&props_table, prop_name, prop_value.clone())?;
            }
            component_properties_table.set(element_id.clone(), props_table)?;
        }
        globals.set("_component_properties", component_properties_table)?;
        
        // Create elements data table
        let elements_table = self.lua.create_table()?;
        for (element_id, element) in &bridge_data.elements_data {
            let element_data = self.lua.create_table()?;
            element_data.set("id", element.id.clone())?;
            element_data.set("element_type", format!("{:?}", element.element_type))?;
            element_data.set("visible", element.visible)?;
            element_data.set("text", element.text.clone())?;
            element_data.set("style_id", element.style_id)?;
            element_data.set("is_checked", element.current_state == kryon_core::InteractionState::Checked)?;
            
            // Store parent/children relationships
            if let Some(parent_id) = element.parent {
                element_data.set("parent_id", parent_id)?;
            }
            
            let children_table = self.lua.create_table()?;
            for (i, child_id) in element.children.iter().enumerate() {
                children_table.set(i + 1, *child_id)?;
            }
            element_data.set("children", children_table)?;
            
            elements_table.set(*element_id, element_data)?;
        }
        globals.set("_elements_data", elements_table)?;
        
        tracing::debug!("Lua bridge setup completed with {} elements and {} styles", 
                       bridge_data.elements_data.len(), bridge_data.style_ids.len());
        
        Ok(())
    }
    
    /// Execute onReady callbacks
    pub fn execute_on_ready_callbacks(&mut self) -> Result<()> {
        let execute_ready_code = r#"
            -- Mark the document as ready
            _mark_ready()
            
            if _ready_callbacks then
                for i, callback in ipairs(_ready_callbacks) do
                    local success, error = pcall(callback)
                    if not success then
                        print("Error in onReady callback " .. i .. ": " .. tostring(error))
                    end
                end
                print("Executed " .. #_ready_callbacks .. " onReady callbacks")
                _ready_callbacks = {}
            end
        "#;
        
        self.lua.load(execute_ready_code).exec().map_err(|e| {
            ScriptError::ExecutionFailed {
                function: "execute_on_ready_callbacks".to_string(),
                error: e.to_string(),
                context: "Executing onReady callbacks".to_string(),
            }
        })?;
        
        Ok(())
    }
    
    /// Get pending changes from the bridge
    pub fn get_pending_changes(&mut self) -> Result<HashMap<String, ChangeSet>> {
        let mut changes = HashMap::new();
        
        // Get style changes
        if let Ok(get_changes_fn) = self.lua.globals().get::<_, LuaFunction>("_get_pending_style_changes") {
            if let Ok(changes_table) = get_changes_fn.call::<_, LuaTable>(()) {
                let mut style_changes = HashMap::new();
                for pair in changes_table.pairs::<u32, u8>() {
                    if let Ok((element_id, style_id)) = pair {
                        style_changes.insert(element_id.to_string(), style_id.to_string());
                    }
                }
                if !style_changes.is_empty() {
                    changes.insert("style_changes".to_string(), ChangeSet {
                        change_type: "style_changes".to_string(),
                        data: style_changes,
                    });
                }
            }
        }
        
        // Get text changes
        if let Ok(get_changes_fn) = self.lua.globals().get::<_, LuaFunction>("_get_pending_text_changes") {
            if let Ok(changes_table) = get_changes_fn.call::<_, LuaTable>(()) {
                let mut text_changes = HashMap::new();
                for pair in changes_table.pairs::<u32, String>() {
                    if let Ok((element_id, text)) = pair {
                        text_changes.insert(element_id.to_string(), text);
                    }
                }
                if !text_changes.is_empty() {
                    changes.insert("text_changes".to_string(), ChangeSet {
                        change_type: "text_changes".to_string(),
                        data: text_changes,
                    });
                }
            }
        }
        
        // Get state changes
        if let Ok(get_changes_fn) = self.lua.globals().get::<_, LuaFunction>("_get_pending_state_changes") {
            if let Ok(changes_table) = get_changes_fn.call::<_, LuaTable>(()) {
                let mut state_changes = HashMap::new();
                for pair in changes_table.pairs::<u32, bool>() {
                    if let Ok((element_id, checked)) = pair {
                        state_changes.insert(element_id.to_string(), checked.to_string());
                    }
                }
                if !state_changes.is_empty() {
                    changes.insert("state_changes".to_string(), ChangeSet {
                        change_type: "state_changes".to_string(),
                        data: state_changes,
                    });
                }
            }
        }
        
        // Get visibility changes
        if let Ok(get_changes_fn) = self.lua.globals().get::<_, LuaFunction>("_get_pending_visibility_changes") {
            if let Ok(changes_table) = get_changes_fn.call::<_, LuaTable>(()) {
                let mut visibility_changes = HashMap::new();
                for pair in changes_table.pairs::<u32, bool>() {
                    if let Ok((element_id, visible)) = pair {
                        visibility_changes.insert(element_id.to_string(), visible.to_string());
                    }
                }
                if !visibility_changes.is_empty() {
                    changes.insert("visibility_changes".to_string(), ChangeSet {
                        change_type: "visibility_changes".to_string(),
                        data: visibility_changes,
                    });
                }
            }
        }
        
        Ok(changes)
    }
    
    /// Get Canvas drawing commands from the Lua bridge
    pub fn get_canvas_commands(&mut self) -> Result<Vec<serde_json::Value>> {
        let mut commands = Vec::new();
        
        // Get canvas commands
        if let Ok(get_commands_fn) = self.lua.globals().get::<_, LuaFunction>("_get_canvas_commands") {
            if let Ok(commands_table) = get_commands_fn.call::<_, LuaTable>(()) {
                for pair in commands_table.pairs::<u32, LuaTable>() {
                    if let Ok((_, command_table)) = pair {
                        // Convert Lua table to JSON value for easier handling
                        if let Ok(json_str) = self.lua_table_to_json_string(&command_table) {
                            if let Ok(json_value) = serde_json::from_str::<serde_json::Value>(&json_str) {
                                commands.push(json_value);
                            }
                        }
                    }
                }
            }
        }
        
        Ok(commands)
    }
    
    /// Clear Canvas drawing commands
    pub fn clear_canvas_commands(&mut self) -> Result<()> {
        if let Ok(clear_fn) = self.lua.globals().get::<_, LuaFunction>("_clear_canvas_commands") {
            clear_fn.call::<_, ()>(()).map_err(|e| {
                ScriptError::BridgeSetupFailed {
                    error: e.to_string(),
                    context: "Clearing Canvas commands".to_string(),
                }
            })?;
        }
        Ok(())
    }
    
    /// Convert Lua table to JSON string
    fn lua_table_to_json_string(&self, table: &LuaTable) -> Result<String> {
        let mut json_map = serde_json::Map::new();
        
        for pair in table.clone().pairs::<mlua::Value, mlua::Value>() {
            if let Ok((key, value)) = pair {
                let key_str = match key {
                    mlua::Value::String(s) => s.to_str()?.to_string(),
                    mlua::Value::Number(n) => n.to_string(),
                    mlua::Value::Integer(i) => i.to_string(),
                    _ => continue,
                };
                
                let json_value = self.lua_value_to_json_value(value)?;
                json_map.insert(key_str, json_value);
            }
        }
        
        Ok(serde_json::to_string(&json_map)?)
    }
    
    /// Convert Lua value to JSON value
    fn lua_value_to_json_value(&self, value: mlua::Value) -> Result<serde_json::Value> {
        match value {
            mlua::Value::Nil => Ok(serde_json::Value::Null),
            mlua::Value::Boolean(b) => Ok(serde_json::Value::Bool(b)),
            mlua::Value::Integer(i) => Ok(serde_json::Value::Number(serde_json::Number::from(i))),
            mlua::Value::Number(n) => {
                if let Some(num) = serde_json::Number::from_f64(n) {
                    Ok(serde_json::Value::Number(num))
                } else {
                    Ok(serde_json::Value::Null)
                }
            },
            mlua::Value::String(s) => Ok(serde_json::Value::String(s.to_str()?.to_string())),
            mlua::Value::Table(t) => {
                let mut json_obj = serde_json::Map::new();
                for pair in t.pairs::<mlua::Value, mlua::Value>() {
                    if let Ok((key, val)) = pair {
                        let key_str = match key {
                            mlua::Value::String(s) => s.to_str()?.to_string(),
                            mlua::Value::Number(n) => n.to_string(),
                            mlua::Value::Integer(i) => i.to_string(),
                            _ => continue,
                        };
                        json_obj.insert(key_str, self.lua_value_to_json_value(val)?);
                    }
                }
                Ok(serde_json::Value::Object(json_obj))
            },
            _ => Ok(serde_json::Value::Null),
        }
    }
    
    /// Execute Canvas script function
    pub fn execute_canvas_script(&mut self, script_name: &str) -> Result<()> {
        // Clear any previous canvas commands
        self.clear_canvas_commands()?;
        
        // Execute the script function
        if let Ok(script_fn) = self.lua.globals().get::<_, LuaFunction>(script_name) {
            script_fn.call::<_, ()>(()).map_err(|e| {
                ScriptError::ExecutionFailed {
                    function: script_name.to_string(),
                    error: e.to_string(),
                    context: "Executing Canvas script".to_string(),
                }
            })?;
        } else {
            return Err(ScriptError::FunctionNotFound {
                function: script_name.to_string(),
                available: "Canvas script function not found".to_string(),
            }.into());
        }
        
        Ok(())
    }
    
    /// Execute Canvas lifecycle hook with optional delta time
    pub fn execute_canvas_lifecycle_hook(&mut self, hook_name: &str, delta_time: Option<f64>) -> Result<()> {
        // Clear any previous canvas commands
        self.clear_canvas_commands()?;
        
        // Execute the lifecycle hook function
        if let Ok(hook_fn) = self.lua.globals().get::<_, LuaFunction>(hook_name) {
            if let Some(dt) = delta_time {
                // Call with delta time (for onUpdate)
                hook_fn.call::<_, ()>(dt).map_err(|e| {
                    ScriptError::ExecutionFailed {
                        function: hook_name.to_string(),
                        error: e.to_string(),
                        context: format!("Executing Canvas lifecycle hook '{}' with delta time", hook_name),
                    }
                })?;
            } else {
                // Call without arguments (for onLoad)
                hook_fn.call::<_, ()>(()).map_err(|e| {
                    ScriptError::ExecutionFailed {
                        function: hook_name.to_string(),
                        error: e.to_string(),
                        context: format!("Executing Canvas lifecycle hook '{}'", hook_name),
                    }
                })?;
            }
        }
        // If hook doesn't exist, that's okay - it's optional
        
        Ok(())
    }
    
    /// Clear pending changes from the bridge
    pub fn clear_pending_changes(&mut self) -> Result<()> {
        // Clear the DOM API changes by calling the Lua clear function
        if let Ok(clear_fn) = self.lua.globals().get::<_, LuaFunction>("_clear_dom_changes") {
            clear_fn.call::<_, ()>(()).map_err(|e| {
                ScriptError::BridgeSetupFailed {
                    error: e.to_string(),
                    context: "Clearing DOM changes".to_string(),
                }
            })?;
        }
        
        Ok(())
    }
    
    /// Setup the complete bridge API in Lua
    fn setup_bridge_api(&self) -> Result<()> {
        // Load the embedded bridge code
        const LUA_BRIDGE_CODE: &str = include_str!("../../lua_bridge.lua");
        
        self.lua.load(LUA_BRIDGE_CODE).exec().map_err(|e| {
            ScriptError::BridgeSetupFailed {
                error: e.to_string(),
                context: "Loading bridge API code".to_string(),
            }
        })?;
        
        tracing::debug!("Lua bridge API loaded successfully");
        Ok(())
    }
    
    /// Helper to set a ScriptValue in a Lua table
    fn set_script_value_in_table(&self, table: &LuaTable, key: &str, value: ScriptValue) -> Result<()> {
        match value {
            ScriptValue::Nil => table.set(key, mlua::Value::Nil)?,
            ScriptValue::Boolean(b) => table.set(key, b)?,
            ScriptValue::Integer(i) => table.set(key, i)?,
            ScriptValue::Number(f) => table.set(key, f)?,
            ScriptValue::String(s) => table.set(key, s)?,
            ScriptValue::Array(arr) => {
                let lua_table = self.lua.create_table()?;
                for (i, item) in arr.into_iter().enumerate() {
                    self.set_script_value_in_table(&lua_table, &(i + 1).to_string(), item)?;
                }
                table.set(key, lua_table)?;
            },
            ScriptValue::Object(obj) => {
                let lua_table = self.lua.create_table()?;
                for (obj_key, obj_value) in obj {
                    self.set_script_value_in_table(&lua_table, &obj_key, obj_value)?;
                }
                table.set(key, lua_table)?;
            },
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kryon_core::{Element, ElementType, InteractionState};
    use glam::Vec4;
    
    #[test]
    fn test_lua_bridge_creation() {
        let lua = Rc::new(Lua::new());
        let bridge = LuaBridge::new(lua);
        assert!(bridge.is_ok());
    }
    
    #[test]
    fn test_bridge_api_loading() {
        let lua = Rc::new(Lua::new());
        let bridge = LuaBridge::new(lua.clone()).unwrap();
        
        // Check that bridge API functions are available
        let globals = lua.globals();
        assert!(globals.get::<_, LuaFunction>("getElementById").is_ok());
        assert!(globals.get::<_, LuaFunction>("addEventListener").is_ok());
        assert!(globals.get::<_, LuaFunction>("onReady").is_ok());
    }
    
    #[test]
    fn test_bridge_setup() {
        let lua = Rc::new(Lua::new());
        let mut bridge = LuaBridge::new(lua.clone()).unwrap();
        
        // Create test bridge data
        let mut element_ids = HashMap::new();
        element_ids.insert("test_button".to_string(), 1);
        
        let mut style_ids = HashMap::new();
        style_ids.insert("button_style".to_string(), 10);
        
        let mut elements_data = HashMap::new();
        elements_data.insert(1, Element {
            id: "test_button".to_string(),
            element_type: ElementType::Button,
            visible: true,
            text: "Click me".to_string(),
            style_id: 10,
            current_state: InteractionState::Normal,
            parent: None,
            children: vec![],
            custom_properties: HashMap::new(),
            calculated_rect: Default::default(),
            content_rect: Default::default(),
            transform: Default::default(),
            opacity: 1.0,
            background_color: Vec4::new(1.0, 1.0, 1.0, 1.0),
            border_color: Vec4::new(0.0, 0.0, 0.0, 1.0),
            border_width: 1.0,
            border_radius: 0.0,
        });
        
        let bridge_data = BridgeData {
            element_ids,
            style_ids,
            component_properties: HashMap::new(),
            elements_data,
            template_variables: HashMap::new(),
        };
        
        let result = bridge.setup(&bridge_data);
        assert!(result.is_ok());
        
        // Verify that data was set up correctly
        let globals = lua.globals();
        let element_ids_table: LuaTable = globals.get("_element_ids").unwrap();
        let test_button_id: u32 = element_ids_table.get("test_button").unwrap();
        assert_eq!(test_button_id, 1);
    }
    
    #[test]
    fn test_pending_changes() {
        let lua = Lua::new();
        let mut bridge = LuaBridge::new(&lua).unwrap();
        
        // Simulate some pending changes
        let setup_changes = r#"
            _pending_style_changes[1] = 5
            _pending_text_changes[2] = "New text"
            _pending_visibility_changes[3] = false
        "#;
        
        lua.load(setup_changes).exec().unwrap();
        
        // Get pending changes
        let changes = bridge.get_pending_changes().unwrap();
        
        // Verify changes were captured
        assert!(changes.contains_key("style_changes"));
        assert!(changes.contains_key("text_changes"));
        assert!(changes.contains_key("visibility_changes"));
        
        let style_changes = &changes["style_changes"];
        assert!(style_changes.data.contains_key("1"));
        assert_eq!(style_changes.data["1"], "5");
    }
}
