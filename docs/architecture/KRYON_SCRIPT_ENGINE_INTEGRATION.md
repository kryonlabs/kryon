# Kryon Script Engine Integration Specification

Complete specification for integrating multiple scripting languages (Lua, JavaScript, Python, Wren, Kryon Lisp) with the Kryon runtime system, including DOM bridge, performance optimization, and cross-language interoperability.

> **🔗 Related Documentation**: 
> - [KRYON_RUNTIME_ARCHITECTURE.md](KRYON_RUNTIME_ARCHITECTURE.md) - Runtime system architecture
> - [KRYON_LISP_SPECIFICATION.md](../language/KRYON_LISP_SPECIFICATION.md) - Kryon Lisp language
> - [KRY_LANGUAGE_SPECIFICATION.md](../language/KRY_LANGUAGE_SPECIFICATION.md) - Script integration syntax

## Table of Contents
- [Architecture Overview](#architecture-overview)
- [Script Engine Abstraction](#script-engine-abstraction)
- [DOM Bridge System](#dom-bridge-system)
- [Language-Specific Implementations](#language-specific-implementations)
- [Cross-Language Interoperability](#cross-language-interoperability)
- [Performance Optimization](#performance-optimization)
- [Security Sandboxing](#security-sandboxing)
- [Error Handling & Debugging](#error-handling--debugging)
- [Memory Management](#memory-management)

---

## Architecture Overview

The Kryon script engine integration system provides a unified interface for multiple scripting languages while maintaining language-specific optimizations:

```
┌─────────────────────────────────────────────────────────────┐
│                   Kryon Runtime Core                        │
└─────────────────────────┬───────────────────────────────────┘
                          │
┌─────────────────────────────────────────────────────────────┐
│                Script Manager                               │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────┐│
│  │   Engine    │ │  Function   │ │    DOM      │ │  Cross  ││
│  │  Registry   │ │  Registry   │ │   Bridge    │ │  Lang   ││
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────┘│
└─────────────────────────┬───────────────────────────────────┘
                          │
┌─────────────────────────────────────────────────────────────┐
│              Script Engine Abstraction Layer               │
└─────────┬───────┬───────┬───────┬───────┬───────────────────┘
          │       │       │       │       │
    ┌─────▼─┐ ┌───▼──┐ ┌──▼───┐ ┌─▼────┐ ┌▼──────────┐
    │  Lua  │ │  JS  │ │Python│ │ Wren │ │Kryon Lisp │
    │Engine │ │Engine│ │Engine│ │Engine│ │  Engine   │
    └───────┘ └──────┘ └──────┘ └──────┘ └───────────┘
```

### Core Design Principles

1. **Language Agnostic**: Uniform API across all supported languages
2. **Zero-Copy Bridges**: Efficient data sharing between script and runtime
3. **Sandboxed Execution**: Security isolation for untrusted code
4. **Hot Reloading**: Dynamic script reloading during development
5. **Cross-Language Calls**: Scripts can call functions in other languages

---

## Script Engine Abstraction

### ScriptEngine Trait

```rust
pub trait ScriptEngine: Send + Sync {
    /// Engine identification
    fn language(&self) -> ScriptLanguage;
    fn version(&self) -> &str;
    fn capabilities(&self) -> EngineCapabilities;
    
    /// Script lifecycle management
    fn initialize(&mut self, config: EngineConfig) -> Result<(), ScriptError>;
    fn compile(&mut self, source: &[u8]) -> Result<CompiledScript, ScriptError>;
    fn load_script(&mut self, script: CompiledScript) -> Result<ScriptId, ScriptError>;
    fn unload_script(&mut self, id: ScriptId) -> Result<(), ScriptError>;
    
    /// Function execution
    fn call_function(&mut self, function: &str, args: Vec<ScriptValue>) -> Result<ScriptValue, ScriptError>;
    fn call_function_async(&mut self, function: &str, args: Vec<ScriptValue>) -> Result<Box<dyn Future<Output = Result<ScriptValue, ScriptError>>>, ScriptError>;
    
    /// Variable management
    fn set_global(&mut self, name: &str, value: ScriptValue) -> Result<(), ScriptError>;
    fn get_global(&mut self, name: &str) -> Result<ScriptValue, ScriptError>;
    
    /// Memory management
    fn collect_garbage(&mut self) -> Result<GarbageCollectionStats, ScriptError>;
    fn get_memory_usage(&self) -> MemoryUsage;
    
    /// Debugging support
    fn set_debug_mode(&mut self, enabled: bool) -> Result<(), ScriptError>;
    fn get_stack_trace(&self) -> Vec<StackFrame>;
    fn set_breakpoint(&mut self, file: &str, line: u32) -> Result<BreakpointId, ScriptError>;
    
    /// Bridge integration
    fn setup_bridge(&mut self, bridge: &dyn ScriptBridge) -> Result<(), ScriptError>;
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ScriptLanguage {
    Lua,
    JavaScript,
    Python,
    Wren,
    KryonLisp,
}

pub struct EngineCapabilities {
    pub supports_async: bool,
    pub supports_generators: bool,
    pub supports_coroutines: bool,
    pub supports_modules: bool,
    pub supports_debugging: bool,
    pub supports_jit: bool,
    pub supports_bytecode: bool,
}

#[derive(Debug, Clone)]
pub enum ScriptValue {
    Null,
    Boolean(bool),
    Integer(i64),
    Float(f64),
    String(String),
    Array(Vec<ScriptValue>),
    Object(HashMap<String, ScriptValue>),
    Function(FunctionRef),
    UserData(Box<dyn Any + Send + Sync>),
}

pub struct CompiledScript {
    pub id: ScriptId,
    pub language: ScriptLanguage,
    pub bytecode: Vec<u8>,
    pub functions: HashMap<String, FunctionInfo>,
    pub globals: Vec<String>,
    pub dependencies: Vec<String>,
}

pub struct FunctionInfo {
    pub name: String,
    pub parameter_count: usize,
    pub parameter_names: Vec<String>,
    pub is_async: bool,
    pub is_generator: bool,
    pub bytecode_offset: usize,
    pub bytecode_length: usize,
}
```

### Script Manager Implementation

```rust
pub struct ScriptManager {
    /// Registered script engines
    engines: HashMap<ScriptLanguage, Box<dyn ScriptEngine>>,
    
    /// Loaded scripts by language
    scripts: HashMap<ScriptLanguage, HashMap<ScriptId, LoadedScript>>,
    
    /// Global function registry
    functions: HashMap<String, FunctionLocation>,
    
    /// Cross-language bridges
    bridges: HashMap<ScriptLanguage, Box<dyn ScriptBridge>>,
    
    /// Scheduled executions
    scheduler: ScriptScheduler,
    
    /// Performance monitoring
    profiler: ScriptProfiler,
    
    /// Security policies
    security: SecurityManager,
}

pub struct LoadedScript {
    pub id: ScriptId,
    pub name: String,
    pub language: ScriptLanguage,
    pub compiled: CompiledScript,
    pub load_time: std::time::Instant,
    pub execution_count: u64,
    pub total_execution_time: std::time::Duration,
}

pub struct FunctionLocation {
    pub script_id: ScriptId,
    pub language: ScriptLanguage,
    pub function_name: String,
    pub is_exported: bool,
}

impl ScriptManager {
    pub fn new() -> Self {
        let mut manager = Self {
            engines: HashMap::new(),
            scripts: HashMap::new(),
            functions: HashMap::new(),
            bridges: HashMap::new(),
            scheduler: ScriptScheduler::new(),
            profiler: ScriptProfiler::new(),
            security: SecurityManager::new(),
        };
        
        // Register default engines
        manager.register_engine(ScriptLanguage::Lua, Box::new(LuaEngine::new()));
        manager.register_engine(ScriptLanguage::JavaScript, Box::new(JSEngine::new()));
        manager.register_engine(ScriptLanguage::Python, Box::new(PythonEngine::new()));
        manager.register_engine(ScriptLanguage::Wren, Box::new(WrenEngine::new()));
        manager.register_engine(ScriptLanguage::KryonLisp, Box::new(LispEngine::new()));
        
        manager
    }
    
    pub fn register_engine(&mut self, language: ScriptLanguage, engine: Box<dyn ScriptEngine>) {
        self.engines.insert(language, engine);
        self.bridges.insert(language, self.create_bridge(language));
    }
    
    pub fn load_script_from_source(&mut self, name: String, language: ScriptLanguage, source: &str) -> Result<ScriptId, ScriptError> {
        // Apply security policies
        self.security.validate_script(language, source)?;
        
        if let Some(engine) = self.engines.get_mut(&language) {
            // Compile script
            let compiled = engine.compile(source.as_bytes())?;
            let script_id = compiled.id;
            
            // Load into engine
            engine.load_script(compiled.clone())?;
            
            // Register functions
            for (func_name, func_info) in &compiled.functions {
                self.functions.insert(func_name.clone(), FunctionLocation {
                    script_id,
                    language,
                    function_name: func_name.clone(),
                    is_exported: true, // TODO: determine from function attributes
                });
            }
            
            // Store script metadata
            let loaded_script = LoadedScript {
                id: script_id,
                name,
                language,
                compiled,
                load_time: std::time::Instant::now(),
                execution_count: 0,
                total_execution_time: std::time::Duration::default(),
            };
            
            self.scripts.entry(language).or_insert_with(HashMap::new).insert(script_id, loaded_script);
            
            Ok(script_id)
        } else {
            Err(ScriptError::EngineNotFound(language))
        }
    }
    
    pub fn call_function(&mut self, function_name: &str, args: Vec<ScriptValue>) -> Result<ScriptValue, ScriptError> {
        if let Some(location) = self.functions.get(function_name) {
            let start_time = std::time::Instant::now();
            
            let result = if let Some(engine) = self.engines.get_mut(&location.language) {
                engine.call_function(&location.function_name, args)
            } else {
                return Err(ScriptError::EngineNotFound(location.language));
            };
            
            // Update profiling information
            let execution_time = start_time.elapsed();
            self.profiler.record_function_call(function_name, execution_time, result.is_ok());
            
            // Update script statistics
            if let Some(scripts) = self.scripts.get_mut(&location.language) {
                if let Some(script) = scripts.get_mut(&location.script_id) {
                    script.execution_count += 1;
                    script.total_execution_time += execution_time;
                }
            }
            
            result
        } else {
            Err(ScriptError::FunctionNotFound(function_name.to_string()))
        }
    }
    
    pub fn setup_dom_bridge(&mut self, dom_bridge: Box<dyn DOMBridge>) -> Result<(), ScriptError> {
        for (language, engine) in &mut self.engines {
            if let Some(bridge) = self.bridges.get(language) {
                bridge.setup_dom_integration(&dom_bridge)?;
                engine.setup_bridge(bridge.as_ref())?;
            }
        }
        Ok(())
    }
    
    pub fn hot_reload_script(&mut self, script_id: ScriptId, new_source: &str) -> Result<(), ScriptError> {
        // Find the script
        let (language, script_name) = self.find_script_info(script_id)?;
        
        // Unload old script
        if let Some(engine) = self.engines.get_mut(&language) {
            engine.unload_script(script_id)?;
        }
        
        // Remove old function registrations
        self.functions.retain(|_, location| location.script_id != script_id);
        
        // Load new version
        self.load_script_from_source(script_name, language, new_source)?;
        
        Ok(())
    }
}
```

---

## DOM Bridge System

### DOM Bridge Abstraction

```rust
pub trait DOMBridge: Send + Sync {
    /// Element access
    fn get_element_by_id(&self, id: &str) -> Result<ElementHandle, DOMError>;
    fn get_elements_by_class(&self, class_name: &str) -> Result<Vec<ElementHandle>, DOMError>;
    fn get_elements_by_tag(&self, tag_name: &str) -> Result<Vec<ElementHandle>, DOMError>;
    
    /// Element manipulation
    fn set_property(&mut self, element: ElementHandle, property: &str, value: ScriptValue) -> Result<(), DOMError>;
    fn get_property(&self, element: ElementHandle, property: &str) -> Result<ScriptValue, DOMError>;
    fn set_style(&mut self, element: ElementHandle, style_name: &str) -> Result<(), DOMError>;
    fn add_class(&mut self, element: ElementHandle, class_name: &str) -> Result<(), DOMError>;
    fn remove_class(&mut self, element: ElementHandle, class_name: &str) -> Result<(), DOMError>;
    
    /// Element creation and destruction
    fn create_element(&mut self, element_type: &str, properties: HashMap<String, ScriptValue>) -> Result<ElementHandle, DOMError>;
    fn append_child(&mut self, parent: ElementHandle, child: ElementHandle) -> Result<(), DOMError>;
    fn remove_element(&mut self, element: ElementHandle) -> Result<(), DOMError>;
    
    /// Event handling
    fn add_event_listener(&mut self, element: ElementHandle, event_type: &str, callback: FunctionRef) -> Result<ListenerId, DOMError>;
    fn remove_event_listener(&mut self, listener_id: ListenerId) -> Result<(), DOMError>;
    fn emit_custom_event(&mut self, element: ElementHandle, event_name: &str, data: ScriptValue) -> Result<(), DOMError>;
    
    /// State management
    fn set_variable(&mut self, name: &str, value: ScriptValue) -> Result<(), DOMError>;
    fn get_variable(&self, name: &str) -> Result<ScriptValue, DOMError>;
    fn watch_variable(&mut self, name: &str, callback: FunctionRef) -> Result<WatcherId, DOMError>;
    
    /// Animation and timing
    fn animate(&mut self, element: ElementHandle, animation: AnimationConfig) -> Result<AnimationHandle, DOMError>;
    fn set_timeout(&mut self, callback: FunctionRef, delay_ms: u32) -> Result<TimerId, DOMError>;
    fn set_interval(&mut self, callback: FunctionRef, interval_ms: u32) -> Result<TimerId, DOMError>;
    fn clear_timer(&mut self, timer_id: TimerId) -> Result<(), DOMError>;
    
    /// Utility functions
    fn log(&self, level: LogLevel, message: &str);
    fn get_current_time(&self) -> u64;
    fn get_random(&self) -> f64;
}

#[derive(Debug, Clone, Copy)]
pub struct ElementHandle(u32);

#[derive(Debug, Clone, Copy)]
pub struct ListenerId(u32);

#[derive(Debug, Clone, Copy)]
pub struct WatcherId(u32);

#[derive(Debug, Clone, Copy)]
pub struct AnimationHandle(u32);

#[derive(Debug, Clone, Copy)]
pub struct TimerId(u32);

pub struct AnimationConfig {
    pub property: String,
    pub from_value: ScriptValue,
    pub to_value: ScriptValue,
    pub duration_ms: u32,
    pub easing: EasingFunction,
    pub on_complete: Option<FunctionRef>,
}
```

### Language-Specific Bridge Implementations

#### Lua Bridge

```rust
pub struct LuaBridge {
    lua: Rc<RefCell<Lua>>,
    dom_bridge: Option<Box<dyn DOMBridge>>,
    element_registry: HashMap<ElementHandle, u32>, // Handle to Lua userdata ID
    callback_registry: HashMap<u32, FunctionRef>,
    next_callback_id: u32,
}

impl LuaBridge {
    pub fn new(lua: Rc<RefCell<Lua>>) -> Self {
        Self {
            lua,
            dom_bridge: None,
            element_registry: HashMap::new(),
            callback_registry: HashMap::new(),
            next_callback_id: 1,
        }
    }
    
    pub fn setup_dom_integration(&mut self, dom_bridge: &Box<dyn DOMBridge>) -> Result<(), ScriptError> {
        self.dom_bridge = Some(dom_bridge.clone());
        
        let lua = self.lua.borrow_mut();
        
        // Create kryon global table
        let kryon_table = lua.create_table()?;
        
        // Element access functions
        kryon_table.set("getElementById", lua.create_function(|lua, id: String| {
            // Bridge implementation
            Ok(())
        })?)?;
        
        kryon_table.set("setProperty", lua.create_function(|lua, (element_id, property, value): (String, String, mlua::Value)| {
            // Convert Lua value to ScriptValue and call DOM bridge
            Ok(())
        })?)?;
        
        kryon_table.set("getProperty", lua.create_function(|lua, (element_id, property): (String, String)| {
            // Call DOM bridge and convert result to Lua value
            Ok(mlua::Value::Nil)
        })?)?;
        
        // State management functions
        kryon_table.set("setVariable", lua.create_function(|lua, (name, value): (String, mlua::Value)| {
            // Convert and call DOM bridge
            Ok(())
        })?)?;
        
        kryon_table.set("getVariable", lua.create_function(|lua, name: String| {
            // Call DOM bridge and convert result
            Ok(mlua::Value::Nil)
        })?)?;
        
        // Animation functions
        kryon_table.set("animate", lua.create_function(|lua, (element_id, config): (String, mlua::Table)| {
            // Parse animation config and call DOM bridge
            Ok(())
        })?)?;
        
        // Utility functions
        kryon_table.set("log", lua.create_function(|lua, message: String| {
            println!("Lua Log: {}", message);
            Ok(())
        })?)?;
        
        kryon_table.set("setTimeout", lua.create_function(|lua, (callback, delay): (mlua::Function, u32)| {
            // Store callback and call DOM bridge
            Ok(())
        })?)?;
        
        // Set global kryon table
        lua.globals().set("kryon", kryon_table)?;
        
        Ok(())
    }
    
    fn convert_lua_value_to_script_value(&self, value: mlua::Value) -> Result<ScriptValue, ScriptError> {
        match value {
            mlua::Value::Nil => Ok(ScriptValue::Null),
            mlua::Value::Boolean(b) => Ok(ScriptValue::Boolean(b)),
            mlua::Value::Integer(i) => Ok(ScriptValue::Integer(i)),
            mlua::Value::Number(n) => Ok(ScriptValue::Float(n)),
            mlua::Value::String(s) => Ok(ScriptValue::String(s.to_str()?.to_string())),
            mlua::Value::Table(t) => {
                let mut map = HashMap::new();
                for pair in t.pairs::<mlua::Value, mlua::Value>() {
                    let (key, val) = pair?;
                    if let mlua::Value::String(key_str) = key {
                        map.insert(key_str.to_str()?.to_string(), self.convert_lua_value_to_script_value(val)?);
                    }
                }
                Ok(ScriptValue::Object(map))
            },
            mlua::Value::Function(f) => {
                let callback_id = self.next_callback_id;
                // Store function reference
                Ok(ScriptValue::Function(FunctionRef::new(callback_id, ScriptLanguage::Lua)))
            },
            _ => Err(ScriptError::UnsupportedValueType),
        }
    }
    
    fn convert_script_value_to_lua_value(&self, lua: &Lua, value: ScriptValue) -> Result<mlua::Value, ScriptError> {
        match value {
            ScriptValue::Null => Ok(mlua::Value::Nil),
            ScriptValue::Boolean(b) => Ok(mlua::Value::Boolean(b)),
            ScriptValue::Integer(i) => Ok(mlua::Value::Integer(i)),
            ScriptValue::Float(f) => Ok(mlua::Value::Number(f)),
            ScriptValue::String(s) => Ok(mlua::Value::String(lua.create_string(&s)?)),
            ScriptValue::Array(arr) => {
                let table = lua.create_table()?;
                for (i, item) in arr.iter().enumerate() {
                    table.set(i + 1, self.convert_script_value_to_lua_value(lua, item.clone())?)?;
                }
                Ok(mlua::Value::Table(table))
            },
            ScriptValue::Object(obj) => {
                let table = lua.create_table()?;
                for (key, value) in obj {
                    table.set(key, self.convert_script_value_to_lua_value(lua, value)?)?;
                }
                Ok(mlua::Value::Table(table))
            },
            _ => Err(ScriptError::UnsupportedValueType),
        }
    }
}
```

#### JavaScript Bridge (using QuickJS)

```rust
pub struct JSBridge {
    context: rquickjs::Context,
    dom_bridge: Option<Box<dyn DOMBridge>>,
    callback_registry: HashMap<u32, FunctionRef>,
}

impl JSBridge {
    pub fn new(context: rquickjs::Context) -> Self {
        Self {
            context,
            dom_bridge: None,
            callback_registry: HashMap::new(),
        }
    }
    
    pub fn setup_dom_integration(&mut self, dom_bridge: &Box<dyn DOMBridge>) -> Result<(), ScriptError> {
        self.dom_bridge = Some(dom_bridge.clone());
        
        self.context.with(|ctx| {
            let global = ctx.globals();
            
            // Create kryon object
            let kryon = rquickjs::Object::new(ctx)?;
            
            // Add DOM functions
            kryon.set("getElementById", rquickjs::Function::new(ctx, |id: String| {
                // Implementation
                Ok(())
            })?)?;
            
            kryon.set("setProperty", rquickjs::Function::new(ctx, |(element_id, property, value): (String, String, rquickjs::Value)| {
                // Implementation
                Ok(())
            })?)?;
            
            // Add to global scope
            global.set("kryon", kryon)?;
            
            // Also add as window.kryon for browser compatibility
            let window = rquickjs::Object::new(ctx)?;
            window.set("kryon", global.get::<_, rquickjs::Object>("kryon")?)?;
            global.set("window", window)?;
            
            Ok::<(), rquickjs::Error>(())
        })?;
        
        Ok(())
    }
}
```

#### Python Bridge (using PyO3)

```rust
pub struct PythonBridge {
    py: Python<'static>,
    dom_bridge: Option<Box<dyn DOMBridge>>,
    kryon_module: PyObject,
}

impl PythonBridge {
    pub fn new(py: Python<'static>) -> Result<Self, ScriptError> {
        let kryon_module = PyModule::new(py, "kryon")?;
        
        Ok(Self {
            py,
            dom_bridge: None,
            kryon_module: kryon_module.into(),
        })
    }
    
    pub fn setup_dom_integration(&mut self, dom_bridge: &Box<dyn DOMBridge>) -> Result<(), ScriptError> {
        self.dom_bridge = Some(dom_bridge.clone());
        
        // Add functions to kryon module
        let module = self.kryon_module.as_ref(self.py).downcast::<PyModule>()?;
        
        // Element access
        module.add_function(wrap_pyfunction!(py_get_element_by_id, module)?)?;
        module.add_function(wrap_pyfunction!(py_set_property, module)?)?;
        module.add_function(wrap_pyfunction!(py_get_property, module)?)?;
        
        // State management
        module.add_function(wrap_pyfunction!(py_set_variable, module)?)?;
        module.add_function(wrap_pyfunction!(py_get_variable, module)?)?;
        
        // Utility functions
        module.add_function(wrap_pyfunction!(py_log, module)?)?;
        
        // Add to sys.modules
        let sys = PyModule::import(self.py, "sys")?;
        let modules = sys.getattr("modules")?;
        modules.set_item("kryon", module)?;
        
        Ok(())
    }
}

// Python function implementations
#[pyfunction]
fn py_get_element_by_id(id: String) -> PyResult<u32> {
    // Bridge implementation
    Ok(0)
}

#[pyfunction]
fn py_set_property(element_id: String, property: String, value: PyObject) -> PyResult<()> {
    // Bridge implementation
    Ok(())
}

#[pyfunction]
fn py_log(message: String) -> PyResult<()> {
    println!("Python Log: {}", message);
    Ok(())
}
```

#### Kryon Lisp Bridge

```rust
pub struct LispBridge {
    interpreter: LispInterpreter,
    dom_bridge: Option<Box<dyn DOMBridge>>,
}

impl LispBridge {
    pub fn new(interpreter: LispInterpreter) -> Self {
        Self {
            interpreter,
            dom_bridge: None,
        }
    }
    
    pub fn setup_dom_integration(&mut self, dom_bridge: &Box<dyn DOMBridge>) -> Result<(), ScriptError> {
        self.dom_bridge = Some(dom_bridge.clone());
        
        // Register built-in functions
        self.interpreter.register_builtin("get-element-by-id", |args| {
            if let Some(LispValue::String(id)) = args.first() {
                // Call DOM bridge
                Ok(LispValue::Number(0.0)) // Placeholder
            } else {
                Err(LispError::InvalidArguments)
            }
        });
        
        self.interpreter.register_builtin("set-property!", |args| {
            if args.len() >= 3 {
                // Extract arguments and call DOM bridge
                Ok(LispValue::Nil)
            } else {
                Err(LispError::InvalidArguments)
            }
        });
        
        self.interpreter.register_builtin("set-variable!", |args| {
            if args.len() >= 2 {
                // Extract arguments and call DOM bridge
                Ok(LispValue::Nil)
            } else {
                Err(LispError::InvalidArguments)
            }
        });
        
        self.interpreter.register_builtin("log", |args| {
            if let Some(LispValue::String(message)) = args.first() {
                println!("Lisp Log: {}", message);
                Ok(LispValue::Nil)
            } else {
                Err(LispError::InvalidArguments)
            }
        });
        
        Ok(())
    }
}
```

---

## Cross-Language Interoperability

### Function Call Forwarding

```rust
pub struct CrossLanguageRegistry {
    /// Functions exported from each language
    exported_functions: HashMap<ScriptLanguage, HashMap<String, FunctionMetadata>>,
    
    /// Type conversion rules between languages
    conversion_rules: HashMap<(ScriptLanguage, ScriptLanguage), TypeConverter>,
    
    /// Function call statistics
    call_stats: HashMap<String, CallStatistics>,
}

pub struct FunctionMetadata {
    pub name: String,
    pub language: ScriptLanguage,
    pub parameter_types: Vec<ValueType>,
    pub return_type: ValueType,
    pub is_async: bool,
    pub documentation: Option<String>,
}

impl CrossLanguageRegistry {
    pub fn call_cross_language_function(
        &mut self,
        source_language: ScriptLanguage,
        target_function: &str,
        args: Vec<ScriptValue>
    ) -> Result<ScriptValue, ScriptError> {
        // Find function metadata
        let (target_language, metadata) = self.find_function(target_function)?;
        
        // Convert arguments from source to target language types
        let converter = self.conversion_rules
            .get(&(source_language, target_language))
            .ok_or(ScriptError::NoConversionRule)?;
        
        let converted_args = args.into_iter()
            .map(|arg| converter.convert_value(arg))
            .collect::<Result<Vec<_>, _>>()?;
        
        // Make the actual function call
        let result = match target_language {
            ScriptLanguage::Lua => self.call_lua_function(target_function, converted_args)?,
            ScriptLanguage::JavaScript => self.call_js_function(target_function, converted_args)?,
            ScriptLanguage::Python => self.call_python_function(target_function, converted_args)?,
            ScriptLanguage::KryonLisp => self.call_lisp_function(target_function, converted_args)?,
            _ => return Err(ScriptError::UnsupportedLanguage),
        };
        
        // Convert result back to source language types
        let converted_result = converter.convert_value_reverse(result)?;
        
        // Update statistics
        self.call_stats.entry(target_function.to_string()).or_default().successful_calls += 1;
        
        Ok(converted_result)
    }
}

pub struct TypeConverter {
    /// Conversion functions for each value type
    conversions: HashMap<ValueType, Box<dyn Fn(ScriptValue) -> Result<ScriptValue, ConversionError>>>,
}

impl TypeConverter {
    pub fn convert_value(&self, value: ScriptValue) -> Result<ScriptValue, ConversionError> {
        let value_type = ValueType::from_script_value(&value);
        
        if let Some(converter) = self.conversions.get(&value_type) {
            converter(value)
        } else {
            // Default conversion - try to preserve the value as-is
            Ok(value)
        }
    }
}
```

### Shared Data Structures

```rust
pub struct SharedDataManager {
    /// Shared objects accessible across languages
    shared_objects: HashMap<String, SharedObject>,
    
    /// Access control for shared objects
    access_control: HashMap<String, AccessPolicy>,
    
    /// Change notifications
    change_listeners: HashMap<String, Vec<ChangeListener>>,
}

pub struct SharedObject {
    pub id: String,
    pub data: ScriptValue,
    pub owner_language: ScriptLanguage,
    pub created_at: std::time::Instant,
    pub last_modified: std::time::Instant,
    pub access_count: u64,
}

pub struct AccessPolicy {
    pub read_languages: HashSet<ScriptLanguage>,
    pub write_languages: HashSet<ScriptLanguage>,
    pub is_readonly: bool,
    pub max_concurrent_access: Option<usize>,
}

impl SharedDataManager {
    pub fn create_shared_object(
        &mut self,
        id: String,
        data: ScriptValue,
        owner: ScriptLanguage,
        policy: AccessPolicy
    ) -> Result<(), ScriptError> {
        let shared_obj = SharedObject {
            id: id.clone(),
            data,
            owner_language: owner,
            created_at: std::time::Instant::now(),
            last_modified: std::time::Instant::now(),
            access_count: 0,
        };
        
        self.shared_objects.insert(id.clone(), shared_obj);
        self.access_control.insert(id, policy);
        
        Ok(())
    }
    
    pub fn read_shared_object(
        &mut self,
        id: &str,
        requesting_language: ScriptLanguage
    ) -> Result<ScriptValue, ScriptError> {
        // Check access permissions
        if let Some(policy) = self.access_control.get(id) {
            if !policy.read_languages.contains(&requesting_language) {
                return Err(ScriptError::AccessDenied);
            }
        }
        
        if let Some(obj) = self.shared_objects.get_mut(id) {
            obj.access_count += 1;
            
            // Clone the data to avoid borrowing issues
            Ok(obj.data.clone())
        } else {
            Err(ScriptError::ObjectNotFound(id.to_string()))
        }
    }
    
    pub fn write_shared_object(
        &mut self,
        id: &str,
        new_data: ScriptValue,
        requesting_language: ScriptLanguage
    ) -> Result<(), ScriptError> {
        // Check access permissions
        if let Some(policy) = self.access_control.get(id) {
            if policy.is_readonly || !policy.write_languages.contains(&requesting_language) {
                return Err(ScriptError::AccessDenied);
            }
        }
        
        if let Some(obj) = self.shared_objects.get_mut(id) {
            let old_data = obj.data.clone();
            obj.data = new_data.clone();
            obj.last_modified = std::time::Instant::now();
            
            // Notify change listeners
            if let Some(listeners) = self.change_listeners.get(id) {
                for listener in listeners {
                    listener.on_change(id, &old_data, &new_data);
                }
            }
            
            Ok(())
        } else {
            Err(ScriptError::ObjectNotFound(id.to_string()))
        }
    }
}
```

---

## Performance Optimization

### Script Compilation and Caching

```rust
pub struct ScriptCompilationCache {
    /// Compiled bytecode cache
    bytecode_cache: HashMap<String, CachedBytecode>,
    
    /// Source code hash to detect changes
    source_hashes: HashMap<String, u64>,
    
    /// Cache statistics
    stats: CacheStatistics,
}

pub struct CachedBytecode {
    pub script_id: String,
    pub language: ScriptLanguage,
    pub bytecode: Vec<u8>,
    pub compilation_time: std::time::Duration,
    pub last_used: std::time::Instant,
    pub use_count: u64,
}

impl ScriptCompilationCache {
    pub fn get_or_compile(
        &mut self,
        script_id: &str,
        source: &str,
        language: ScriptLanguage,
        engine: &mut dyn ScriptEngine
    ) -> Result<CompiledScript, ScriptError> {
        let source_hash = self.calculate_hash(source);
        
        // Check if we have cached bytecode
        if let Some(cached_hash) = self.source_hashes.get(script_id) {
            if *cached_hash == source_hash {
                if let Some(cached) = self.bytecode_cache.get_mut(script_id) {
                    cached.last_used = std::time::Instant::now();
                    cached.use_count += 1;
                    self.stats.cache_hits += 1;
                    
                    return Ok(CompiledScript {
                        id: ScriptId::new(),
                        language,
                        bytecode: cached.bytecode.clone(),
                        functions: HashMap::new(), // TODO: cache function metadata
                        globals: Vec::new(),
                        dependencies: Vec::new(),
                    });
                }
            }
        }
        
        // Cache miss - compile the script
        self.stats.cache_misses += 1;
        let start_time = std::time::Instant::now();
        let compiled = engine.compile(source.as_bytes())?;
        let compilation_time = start_time.elapsed();
        
        // Store in cache
        self.bytecode_cache.insert(script_id.to_string(), CachedBytecode {
            script_id: script_id.to_string(),
            language,
            bytecode: compiled.bytecode.clone(),
            compilation_time,
            last_used: std::time::Instant::now(),
            use_count: 1,
        });
        
        self.source_hashes.insert(script_id.to_string(), source_hash);
        
        Ok(compiled)
    }
    
    pub fn cleanup_unused(&mut self, max_age: std::time::Duration) {
        let now = std::time::Instant::now();
        let mut to_remove = Vec::new();
        
        for (script_id, cached) in &self.bytecode_cache {
            if now.duration_since(cached.last_used) > max_age {
                to_remove.push(script_id.clone());
            }
        }
        
        for script_id in to_remove {
            self.bytecode_cache.remove(&script_id);
            self.source_hashes.remove(&script_id);
            self.stats.cache_evictions += 1;
        }
    }
}
```

### JIT Compilation Support

```rust
pub struct JITCompiler {
    /// Hot function detection
    hot_functions: HashMap<String, HotFunctionInfo>,
    
    /// JIT compilation threshold
    compilation_threshold: u32,
    
    /// JIT compiled code cache
    jit_cache: HashMap<String, JITCompiledFunction>,
    
    /// Performance profiler
    profiler: JITProfiler,
}

pub struct HotFunctionInfo {
    pub function_name: String,
    pub language: ScriptLanguage,
    pub call_count: u32,
    pub total_execution_time: std::time::Duration,
    pub average_execution_time: std::time::Duration,
    pub is_jit_candidate: bool,
}

pub struct JITCompiledFunction {
    pub function_name: String,
    pub native_code: Vec<u8>,
    pub compilation_time: std::time::Duration,
    pub speedup_factor: f64,
}

impl JITCompiler {
    pub fn record_function_execution(
        &mut self,
        function_name: &str,
        language: ScriptLanguage,
        execution_time: std::time::Duration
    ) {
        let info = self.hot_functions.entry(function_name.to_string()).or_insert(HotFunctionInfo {
            function_name: function_name.to_string(),
            language,
            call_count: 0,
            total_execution_time: std::time::Duration::default(),
            average_execution_time: std::time::Duration::default(),
            is_jit_candidate: false,
        });
        
        info.call_count += 1;
        info.total_execution_time += execution_time;
        info.average_execution_time = info.total_execution_time / info.call_count;
        
        // Check if function should be JIT compiled
        if info.call_count >= self.compilation_threshold && !info.is_jit_candidate {
            info.is_jit_candidate = true;
            self.schedule_jit_compilation(function_name, language);
        }
    }
    
    fn schedule_jit_compilation(&mut self, function_name: &str, language: ScriptLanguage) {
        // Schedule JIT compilation on a background thread
        match language {
            ScriptLanguage::Lua => {
                // Use LuaJIT if available
                self.compile_lua_to_native(function_name);
            },
            ScriptLanguage::JavaScript => {
                // Use V8's JIT compiler or similar
                self.compile_js_to_native(function_name);
            },
            _ => {
                // Language doesn't support JIT compilation
            }
        }
    }
}
```

This comprehensive script engine integration specification provides the foundation for a powerful, multi-language scripting system that can efficiently execute code from multiple languages while maintaining security, performance, and cross-language interoperability.