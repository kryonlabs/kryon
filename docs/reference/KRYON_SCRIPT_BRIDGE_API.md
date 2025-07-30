# Kryon Script Bridge API Specification

This document defines the complete API for script engine integration, including function signatures, type conversions, and security requirements.

## Table of Contents

- [Script Engine Integration](#script-engine-integration)
- [Type System](#type-system)
- [Function Registration](#function-registration)
- [Context Management](#context-management)
- [Security Sandbox](#security-sandbox)
- [Error Handling](#error-handling)

## Script Engine Integration

### Supported Script Languages

```rust
/// Supported script languages with their specific requirements
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ScriptLanguage {
    Lua {
        version: LuaVersion,
        jit_enabled: bool,
    },
    JavaScript {
        engine: JSEngine,
        es_version: ESVersion,
    },
    Python {
        version: PythonVersion,
        restricted_mode: bool,
    },
    Wren {
        version: WrenVersion,
    },
}

#[derive(Debug, Clone, Copy)]
pub enum LuaVersion {
    Lua51,
    Lua52,
    Lua53,
    Lua54,
    LuaJIT21,
}

#[derive(Debug, Clone, Copy)]
pub enum JSEngine {
    V8,
    QuickJs,
    Hermes,
}

/// Engine-specific configuration
pub struct ScriptEngineConfig {
    pub language: ScriptLanguage,
    pub max_memory: usize,           // 16MB default
    pub max_execution_time: Duration, // 100ms default
    pub sandbox_enabled: bool,       // true for security
    pub debug_enabled: bool,         // false in production
    pub jit_enabled: bool,          // true for performance
}
```

### Core Script Bridge API

```rust
/// Complete script bridge implementation requirements
pub trait ScriptBridge: Send + Sync + 'static {
    type Value: ScriptValue;
    type Error: ScriptError;
    type Context: ScriptContext;
    
    /// Initialize script engine with configuration
    /// 
    /// # Parameters
    /// - `config`: Engine configuration and limits
    /// 
    /// # Returns
    /// - `Ok(())`: Engine initialized successfully
    /// - `Err(InitError)`: Failed to initialize engine
    /// 
    /// # Thread Safety
    /// - Must be called once per engine instance
    /// - Can be called from any thread
    /// - Must set up thread-local storage if needed
    fn initialize(&mut self, config: ScriptEngineConfig) -> Result<(), Self::Error>;
    
    /// Create a new script execution context
    /// 
    /// # Parameters
    /// - `name`: Context identifier for debugging
    /// - `parent`: Optional parent context for inheritance
    /// 
    /// # Returns
    /// - Context handle for script execution
    /// 
    /// # Memory Management
    /// - Context must be isolated from others
    /// - Must track memory usage per context
    /// - Should support context cleanup
    fn create_context(
        &mut self,
        name: &str,
        parent: Option<&Self::Context>,
    ) -> Result<Self::Context, Self::Error>;
    
    /// Execute script code in given context
    /// 
    /// # Parameters
    /// - `script`: Script source code (UTF-8)
    /// - `context`: Execution context with variables
    /// - `timeout`: Maximum execution time
    /// - `filename`: Optional filename for debugging
    /// 
    /// # Returns
    /// - `Ok(Value)`: Script execution result
    /// - `Err(Error)`: Compilation or runtime error
    /// 
    /// # Security Requirements
    /// - Must enforce sandbox restrictions
    /// - Should limit system resource access
    /// - Must prevent infinite loops
    fn execute_script(
        &mut self,
        script: &str,
        context: &mut Self::Context,
        timeout: Duration,
        filename: Option<&str>,
    ) -> Result<Self::Value, Self::Error>;
    
    /// Register native function callable from scripts
    /// 
    /// # Parameters
    /// - `name`: Function name in script namespace
    /// - `function`: Native function implementation
    /// - `signature`: Function signature for type checking
    /// 
    /// # Type Safety
    /// - Must validate argument types at runtime
    /// - Should provide clear error messages  
    /// - Must handle type conversion errors
    fn register_function<F>(
        &mut self,
        name: &str,
        function: F,
        signature: FunctionSignature,
    ) -> Result<(), Self::Error>
    where
        F: Fn(&[Self::Value]) -> Result<Self::Value, Self::Error> + Send + Sync + 'static;
    
    /// Register native type for script access
    /// 
    /// # Parameters
    /// - `type_name`: Type name in script namespace
    /// - `methods`: Available methods on the type
    /// - `properties`: Available properties
    /// 
    /// # Requirements
    /// - Must support method calls from scripts
    /// - Should handle property access safely
    /// - Must prevent memory safety violations
    fn register_type<T>(
        &mut self,
        type_name: &str,
        methods: Vec<TypeMethod<T, Self::Value, Self::Error>>,
        properties: Vec<TypeProperty<T, Self::Value>>,
    ) -> Result<(), Self::Error>
    where
        T: Send + Sync + 'static;
    
    /// Set global variable in script context
    fn set_global(
        &mut self,
        context: &mut Self::Context,
        name: &str,
        value: Self::Value,
    ) -> Result<(), Self::Error>;
    
    /// Get global variable from script context
    fn get_global(
        &self,
        context: &Self::Context,
        name: &str,
    ) -> Result<Option<Self::Value>, Self::Error>;
    
    /// Compile script to bytecode for reuse
    /// 
    /// # Performance
    /// - Should cache compiled scripts
    /// - Must be faster than repeated parsing
    /// - Should support script invalidation
    fn compile_script(
        &mut self,
        script: &str,
        filename: Option<&str>,
    ) -> Result<CompiledScript, Self::Error>;
    
    /// Execute pre-compiled script
    fn execute_compiled(
        &mut self,
        compiled: &CompiledScript,
        context: &mut Self::Context,
        timeout: Duration,
    ) -> Result<Self::Value, Self::Error>;
    
    /// Garbage collect script engine memory
    /// 
    /// # Performance
    /// - Should not block for more than 2ms
    /// - Must be safe to call during execution
    /// - Should track memory pressure
    fn collect_garbage(&mut self) -> GCStats;
    
    /// Get current memory usage statistics
    fn get_memory_stats(&self) -> MemoryStats;
}
```

## Type System

### Script Value Types

```rust
/// Universal script value type supporting all script languages
#[derive(Debug, Clone, PartialEq)]
pub enum ScriptValue {
    /// Null/nil/undefined value
    Null,
    /// Boolean true/false
    Bool(bool),
    /// 64-bit signed integer
    Integer(i64),
    /// 64-bit floating point number
    Float(f64),
    /// UTF-8 string
    String(String),
    /// Byte array for binary data
    Bytes(Vec<u8>),
    /// Ordered array of values
    Array(Vec<ScriptValue>),
    /// Key-value map (object/table/dict)
    Object(HashMap<String, ScriptValue>),
    /// Function reference (callable from script)
    Function(FunctionRef),
    /// Native type wrapper
    Native(Box<dyn NativeValue>),
    /// Custom user data
    UserData(Box<dyn UserData>),
}

/// Trait for native types exposed to scripts
pub trait NativeValue: Send + Sync + std::fmt::Debug {
    /// Get the native type name
    fn type_name(&self) -> &'static str;
    
    /// Convert to script value
    fn to_script_value(&self) -> ScriptValue;
    
    /// Call method on native object
    fn call_method(
        &mut self,
        method: &str,
        args: &[ScriptValue],
    ) -> Result<ScriptValue, Box<dyn ScriptError>>;
    
    /// Get property value
    fn get_property(&self, name: &str) -> Option<ScriptValue>;
    
    /// Set property value  
    fn set_property(&mut self, name: &str, value: ScriptValue) -> Result<(), Box<dyn ScriptError>>;
}

/// Type conversion utilities
pub trait TypeConversion {
    /// Convert Kryon types to script values
    fn to_script_value(&self, value: &dyn Any) -> Result<ScriptValue, ConversionError>;
    
    /// Convert script values to Kryon types
    fn from_script_value<T: 'static>(&self, value: &ScriptValue) -> Result<T, ConversionError>;
    
    /// Register custom type converter
    fn register_converter<T, F>(&mut self, converter: F)
    where
        T: 'static,
        F: Fn(&T) -> ScriptValue + Send + Sync + 'static;
}

/// Built-in type conversions for Kryon types
impl TypeConversion for ScriptBridge {
    /// Convert common Kryon types to script values
    fn convert_kryon_types(&self, value: &dyn Any) -> Result<ScriptValue, ConversionError> {
        // Element -> Object with properties
        if let Some(element) = value.downcast_ref::<Element>() {
            return Ok(self.element_to_script_value(element));
        }
        
        // Color -> Object {r, g, b, a}
        if let Some(color) = value.downcast_ref::<Color>() {
            return Ok(ScriptValue::Object(HashMap::from([
                ("r".to_string(), ScriptValue::Float(color.r as f64)),
                ("g".to_string(), ScriptValue::Float(color.g as f64)),
                ("b".to_string(), ScriptValue::Float(color.b as f64)),
                ("a".to_string(), ScriptValue::Float(color.a as f64)),
            ])));
        }
        
        // Point -> Object {x, y}
        if let Some(point) = value.downcast_ref::<Point>() {
            return Ok(ScriptValue::Object(HashMap::from([
                ("x".to_string(), ScriptValue::Float(point.x as f64)),
                ("y".to_string(), ScriptValue::Float(point.y as f64)),
            ])));
        }
        
        // Rect -> Object {x, y, width, height}
        if let Some(rect) = value.downcast_ref::<Rect>() {
            return Ok(ScriptValue::Object(HashMap::from([
                ("x".to_string(), ScriptValue::Float(rect.x as f64)),
                ("y".to_string(), ScriptValue::Float(rect.y as f64)),
                ("width".to_string(), ScriptValue::Float(rect.width as f64)),
                ("height".to_string(), ScriptValue::Float(rect.height as f64)),
            ])));
        }
        
        Err(ConversionError::UnsupportedType)
    }
}
```

### Function Signatures

```rust
/// Function signature for type checking and documentation
#[derive(Debug, Clone)]
pub struct FunctionSignature {
    /// Function name
    pub name: String,
    /// Parameter specifications
    pub parameters: Vec<ParameterSpec>,
    /// Return type specification
    pub return_type: TypeSpec,
    /// Function documentation
    pub documentation: Option<String>,
}

#[derive(Debug, Clone)]
pub struct ParameterSpec {
    /// Parameter name
    pub name: String,
    /// Expected type
    pub type_spec: TypeSpec,
    /// Whether parameter is optional
    pub optional: bool,
    /// Default value if optional
    pub default_value: Option<ScriptValue>,
    /// Parameter documentation
    pub description: Option<String>,
}

#[derive(Debug, Clone, PartialEq)]
pub enum TypeSpec {
    /// Any type accepted
    Any,
    /// Null/nil/undefined
    Null,
    /// Boolean
    Bool,
    /// Integer number
    Integer,
    /// Floating point number
    Float,
    /// Any number (int or float)
    Number,
    /// String
    String,
    /// Byte array
    Bytes,
    /// Array of specific type
    Array(Box<TypeSpec>),
    /// Object with optional property types
    Object(HashMap<String, TypeSpec>),
    /// Function with signature
    Function(Box<FunctionSignature>),
    /// Specific native type
    Native(String),
    /// Union of multiple types
    Union(Vec<TypeSpec>),
    /// Optional type (can be null)
    Optional(Box<TypeSpec>),
}

impl TypeSpec {
    /// Check if script value matches type specification
    pub fn matches(&self, value: &ScriptValue) -> bool {
        match (self, value) {
            (TypeSpec::Any, _) => true,
            (TypeSpec::Null, ScriptValue::Null) => true,
            (TypeSpec::Bool, ScriptValue::Bool(_)) => true,
            (TypeSpec::Integer, ScriptValue::Integer(_)) => true,
            (TypeSpec::Float, ScriptValue::Float(_)) => true,
            (TypeSpec::Number, ScriptValue::Integer(_) | ScriptValue::Float(_)) => true,
            (TypeSpec::String, ScriptValue::String(_)) => true,
            (TypeSpec::Bytes, ScriptValue::Bytes(_)) => true,
            (TypeSpec::Array(inner), ScriptValue::Array(arr)) => {
                arr.iter().all(|v| inner.matches(v))
            }
            (TypeSpec::Function(_), ScriptValue::Function(_)) => true,
            (TypeSpec::Union(types), value) => {
                types.iter().any(|t| t.matches(value))
            }
            (TypeSpec::Optional(inner), ScriptValue::Null) => true,
            (TypeSpec::Optional(inner), value) => inner.matches(value),
            _ => false,
        }
    }
    
    /// Generate helpful error message for type mismatch
    pub fn mismatch_error(&self, value: &ScriptValue) -> String {
        format!(
            "Expected type '{}', but got '{}'",
            self.type_name(),
            value.type_name()
        )
    }
    
    /// Get human-readable type name
    pub fn type_name(&self) -> String {
        match self {
            TypeSpec::Any => "any".to_string(),
            TypeSpec::Null => "null".to_string(),
            TypeSpec::Bool => "boolean".to_string(),
            TypeSpec::Integer => "integer".to_string(),
            TypeSpec::Float => "float".to_string(),
            TypeSpec::Number => "number".to_string(),
            TypeSpec::String => "string".to_string(),
            TypeSpec::Bytes => "bytes".to_string(),
            TypeSpec::Array(inner) => format!("array<{}>", inner.type_name()),
            TypeSpec::Object(_) => "object".to_string(),
            TypeSpec::Function(_) => "function".to_string(),
            TypeSpec::Native(name) => name.clone(),
            TypeSpec::Union(types) => {
                let names: Vec<_> = types.iter().map(|t| t.type_name()).collect();
                names.join(" | ")
            }
            TypeSpec::Optional(inner) => format!("{}?", inner.type_name()),
        }
    }
}
```

## Function Registration

### Native Function Registration

```rust
/// Macro for easy function registration with type checking
macro_rules! register_function {
    ($bridge:expr, $name:expr, $func:expr, ($($param:ident: $param_type:ty),*) -> $ret_type:ty) => {
        {
            let signature = FunctionSignature {
                name: $name.to_string(),
                parameters: vec![
                    $(ParameterSpec {
                        name: stringify!($param).to_string(),
                        type_spec: <$param_type>::type_spec(),
                        optional: false,
                        default_value: None,
                        description: None,
                    }),*
                ],
                return_type: <$ret_type>::type_spec(),
                documentation: None,
            };
            
            $bridge.register_function($name, move |args| {
                // Type checking and conversion
                if args.len() != signature.parameters.len() {
                    return Err(ScriptError::ArgumentCount {
                        expected: signature.parameters.len(),
                        actual: args.len(),
                    });
                }
                
                // Convert arguments
                let mut arg_iter = args.iter();
                $(
                    let $param: $param_type = arg_iter.next()
                        .ok_or(ScriptError::MissingArgument)?
                        .try_into()?;
                )*
                
                // Call function and convert result
                let result: $ret_type = $func($($param),*)?;
                Ok(result.into())
            }, signature)
        }
    };
}

/// Example usage of function registration
impl KryonScriptBridge {
    pub fn register_kryon_functions(&mut self) -> Result<(), Box<dyn ScriptError>> {
        // Element manipulation functions
        register_function!(
            self,
            "createElement",
            create_element,
            (element_type: String, properties: ScriptValue) -> ElementId
        )?;
        
        register_function!(
            self,
            "setProperty",
            set_element_property,
            (element_id: ElementId, property: String, value: ScriptValue) -> bool
        )?;
        
        register_function!(
            self,
            "getProperty",
            get_element_property,
            (element_id: ElementId, property: String) -> ScriptValue
        )?;
        
        // Layout functions
        register_function!(
            self,
            "getElementRect",
            get_element_rect,
            (element_id: ElementId) -> ScriptValue
        )?;
        
        register_function!(
            self,
            "invalidateLayout",
            invalidate_layout,
            (element_ids: Vec<ElementId>) -> bool
        )?;
        
        // Event functions
        register_function!(
            self,
            "addEventListener",
            add_event_listener,
            (element_id: ElementId, event_type: String, callback: ScriptValue) -> bool
        )?;
        
        register_function!(
            self,
            "removeEventListener",
            remove_event_listener,
            (element_id: ElementId, event_type: String) -> bool
        )?;
        
        // Utility functions
        register_function!(
            self,
            "log",
            script_log,
            (level: String, message: String) -> bool
        )?;
        
        register_function!(
            self,
            "setTimeout",
            set_timeout,
            (callback: ScriptValue, delay_ms: u32) -> TimerId
        )?;
        
        Ok(())
    }
}
```

### Type Registration

```rust
/// Register native types for script access
pub trait TypeRegistry {
    /// Register a native type with methods and properties
    fn register_type<T>(&mut self, registration: TypeRegistration<T>) -> Result<(), Box<dyn ScriptError>>
    where
        T: Send + Sync + 'static;
}

#[derive(Debug)]
pub struct TypeRegistration<T> {
    pub type_name: String,
    pub methods: Vec<TypeMethod<T>>,
    pub properties: Vec<TypeProperty<T>>,
    pub constructor: Option<TypeConstructor<T>>,
    pub destructor: Option<TypeDestructor<T>>,
}

pub struct TypeMethod<T> {
    pub name: String,
    pub signature: FunctionSignature,
    pub implementation: Box<dyn Fn(&mut T, &[ScriptValue]) -> Result<ScriptValue, Box<dyn ScriptError>> + Send + Sync>,
}

pub struct TypeProperty<T> {
    pub name: String,
    pub type_spec: TypeSpec,
    pub getter: Box<dyn Fn(&T) -> ScriptValue + Send + Sync>,
    pub setter: Option<Box<dyn Fn(&mut T, ScriptValue) -> Result<(), Box<dyn ScriptError>> + Send + Sync>>,
}

/// Example: Register Element type
impl KryonScriptBridge {
    pub fn register_element_type(&mut self) -> Result<(), Box<dyn ScriptError>> {
        let registration = TypeRegistration::<Element> {
            type_name: "Element".to_string(),
            methods: vec![
                TypeMethod {
                    name: "appendChild".to_string(),
                    signature: FunctionSignature {
                        name: "appendChild".to_string(),
                        parameters: vec![
                            ParameterSpec {
                                name: "child".to_string(),
                                type_spec: TypeSpec::Native("Element".to_string()),
                                optional: false,
                                default_value: None,
                                description: Some("Child element to append".to_string()),
                            }
                        ],
                        return_type: TypeSpec::Bool,
                        documentation: Some("Append child element".to_string()),
                    },
                    implementation: Box::new(|element, args| {
                        let child = args[0].try_into::<Element>()?;
                        element.add_child(child);
                        Ok(ScriptValue::Bool(true))
                    }),
                },
            ],
            properties: vec![
                TypeProperty {
                    name: "id".to_string(),
                    type_spec: TypeSpec::String,
                    getter: Box::new(|element| ScriptValue::String(element.id.to_string())),
                    setter: None, // ID is read-only
                },
                TypeProperty {
                    name: "visible".to_string(),
                    type_spec: TypeSpec::Bool,
                    getter: Box::new(|element| ScriptValue::Bool(element.visible)),
                    setter: Some(Box::new(|element, value| {
                        element.visible = value.try_into::<bool>()?;
                        Ok(())
                    })),
                },
            ],
            constructor: None,
            destructor: None,
        };
        
        self.register_type(registration)
    }
}
```

## Context Management

### Script Context

```rust
/// Script execution context with variable scoping
pub struct ScriptContext {
    /// Context identifier
    pub name: String,
    /// Global variables accessible to scripts
    pub globals: HashMap<String, ScriptValue>,
    /// Parent context for variable inheritance
    pub parent: Option<Box<ScriptContext>>,
    /// Registered functions in this context
    pub functions: HashMap<String, RegisteredFunction>,
    /// Security sandbox configuration
    pub sandbox: SandboxConfig,
    /// Memory usage tracking
    pub memory_tracker: MemoryTracker,
}

impl ScriptContext {
    /// Create new context with optional parent
    pub fn new(name: &str, parent: Option<Box<ScriptContext>>) -> Self {
        Self {
            name: name.to_string(),
            globals: HashMap::new(),
            parent,
            functions: HashMap::new(),
            sandbox: SandboxConfig::default(),
            memory_tracker: MemoryTracker::new(),
        }
    }
    
    /// Set variable in current context
    pub fn set_variable(&mut self, name: &str, value: ScriptValue) {
        self.globals.insert(name.to_string(), value);
    }
    
    /// Get variable from current or parent contexts
    pub fn get_variable(&self, name: &str) -> Option<&ScriptValue> {
        self.globals.get(name).or_else(|| {
            self.parent.as_ref().and_then(|p| p.get_variable(name))
        })
    }
    
    /// Create child context inheriting from this one
    pub fn create_child(&self, name: &str) -> ScriptContext {
        ScriptContext::new(name, Some(Box::new(self.clone())))
    }
    
    /// Clear all variables and functions
    pub fn clear(&mut self) {
        self.globals.clear();
        self.functions.clear();
        self.memory_tracker.reset();
    }
}

/// Kryon-specific context variables
impl ScriptContext {
    /// Add standard Kryon context variables
    pub fn add_kryon_context(&mut self, app: &KryonApp) {
        // Current element context
        self.set_variable("$element", ScriptValue::Native(Box::new(app.current_element.clone())));
        
        // Parent element context
        if let Some(parent) = app.current_element.parent() {
            self.set_variable("$parent", ScriptValue::Native(Box::new(parent.clone())));
        }
        
        // Root element context
        self.set_variable("$root", ScriptValue::Native(Box::new(app.root_element.clone())));
        
        // Application state
        self.set_variable("$app", ScriptValue::Object(HashMap::from([
            ("width".to_string(), ScriptValue::Float(app.viewport.width as f64)),
            ("height".to_string(), ScriptValue::Float(app.viewport.height as f64)),
            ("scale".to_string(), ScriptValue::Float(app.viewport.scale_factor as f64)),
        ])));
        
        // Template variables
        for (name, value) in &app.template_variables {
            self.set_variable(&format!("${}", name), value.clone());
        }
    }
}
```

## Security Sandbox

### Sandbox Configuration

```rust
/// Security sandbox configuration for script execution
#[derive(Debug, Clone)]
pub struct SandboxConfig {
    /// Enable/disable sandbox (should always be true in production)
    pub enabled: bool,
    
    /// Allowed system functions
    pub allowed_functions: HashSet<String>,
    
    /// Blocked system functions (takes precedence over allowed)
    pub blocked_functions: HashSet<String>,
    
    /// File system access permissions
    pub filesystem_access: FilesystemAccess,
    
    /// Network access permissions
    pub network_access: NetworkAccess,
    
    /// Memory limits
    pub memory_limits: MemoryLimits,
    
    /// Execution time limits
    pub execution_limits: ExecutionLimits,
}

#[derive(Debug, Clone)]
pub enum FilesystemAccess {
    /// No file system access
    None,
    /// Read-only access to specific directories
    ReadOnly(Vec<PathBuf>),
    /// Read-write access to specific directories
    ReadWrite(Vec<PathBuf>),
}

#[derive(Debug, Clone)]
pub enum NetworkAccess {
    /// No network access
    None,
    /// HTTP/HTTPS requests to specific domains
    HttpOnly(Vec<String>),
    /// Full network access (dangerous)
    Full,
}

#[derive(Debug, Clone)]
pub struct MemoryLimits {
    /// Maximum heap size for scripts
    pub max_heap_size: usize,
    /// Maximum stack depth
    pub max_stack_depth: usize,
    /// Maximum string length
    pub max_string_length: usize,
    /// Maximum array/object size
    pub max_collection_size: usize,
}

#[derive(Debug, Clone)]
pub struct ExecutionLimits {
    /// Maximum execution time per script
    pub max_execution_time: Duration,
    /// Maximum number of instructions
    pub max_instructions: u64,
    /// Maximum recursion depth
    pub max_recursion_depth: usize,
}

impl Default for SandboxConfig {
    fn default() -> Self {
        Self {
            enabled: true,
            allowed_functions: HashSet::from([
                // Math functions
                "math.sin".to_string(),
                "math.cos".to_string(),
                "math.sqrt".to_string(),
                // String functions
                "string.len".to_string(),
                "string.sub".to_string(),
                // Table/array functions
                "table.insert".to_string(),
                "table.remove".to_string(),
            ]),
            blocked_functions: HashSet::from([
                // Dangerous functions
                "os.execute".to_string(),
                "io.open".to_string(),
                "require".to_string(),
                "dofile".to_string(),
                "loadfile".to_string(),
                // Debug functions
                "debug.getfenv".to_string(),
                "debug.setfenv".to_string(),
            ]),
            filesystem_access: FilesystemAccess::None,
            network_access: NetworkAccess::None,
            memory_limits: MemoryLimits {
                max_heap_size: 16 * 1024 * 1024, // 16MB
                max_stack_depth: 1000,
                max_string_length: 1024 * 1024, // 1MB
                max_collection_size: 10000,
            },
            execution_limits: ExecutionLimits {
                max_execution_time: Duration::from_millis(100),
                max_instructions: 1_000_000,
                max_recursion_depth: 100,
            },
        }
    }
}
```

### Sandbox Enforcement

```rust
/// Sandbox enforcement implementation
pub trait SandboxEnforcer {
    /// Check if function call is allowed
    fn check_function_call(&self, function_name: &str) -> Result<(), SecurityError>;
    
    /// Check memory allocation
    fn check_memory_allocation(&self, size: usize) -> Result<(), SecurityError>;
    
    /// Check execution time
    fn check_execution_time(&self, elapsed: Duration) -> Result<(), SecurityError>;
    
    /// Check file system access
    fn check_file_access(&self, path: &Path, access_type: AccessType) -> Result<(), SecurityError>;
    
    /// Check network access
    fn check_network_access(&self, url: &str) -> Result<(), SecurityError>;
}

#[derive(Debug, Clone)]
pub enum AccessType {
    Read,
    Write,
    Execute,
}

#[derive(Debug, Clone)]
pub enum SecurityError {
    FunctionBlocked(String),
    MemoryLimitExceeded { requested: usize, limit: usize },
    ExecutionTimeExceeded { elapsed: Duration, limit: Duration },
    FileAccessDenied { path: PathBuf, access: AccessType },
    NetworkAccessDenied(String),
    InstructionLimitExceeded { count: u64, limit: u64 },
    StackOverflow { depth: usize, limit: usize },
}
```

## Error Handling

### Script Error Types

```rust
/// Comprehensive script error handling
#[derive(Debug, Clone)]
pub enum ScriptError {
    /// Compilation/syntax errors
    CompilationError {
        message: String,
        line: Option<usize>,
        column: Option<usize>,
        filename: Option<String>,
    },
    
    /// Runtime execution errors
    RuntimeError {
        message: String,
        stack_trace: Vec<StackFrame>,
    },
    
    /// Type conversion errors
    TypeError {
        expected: TypeSpec,
        actual: String,
        context: String,
    },
    
    /// Function call errors
    FunctionError {
        function_name: String,
        error: Box<dyn std::error::Error + Send + Sync>,
    },
    
    /// Security/sandbox violations
    SecurityError(SecurityError),
    
    /// Resource/memory errors
    ResourceError {
        resource_type: String,
        message: String,
    },
    
    /// Engine-specific errors
    EngineError {
        engine: String,
        code: i32,
        message: String,
    },
}

#[derive(Debug, Clone)]
pub struct StackFrame {
    pub function_name: Option<String>,
    pub filename: Option<String>,
    pub line: Option<usize>,
    pub column: Option<usize>,
}

impl ScriptError {
    /// Create user-friendly error message
    pub fn user_message(&self) -> String {
        match self {
            ScriptError::CompilationError { message, line, column, filename } => {
                let location = match (filename, line, column) {
                    (Some(f), Some(l), Some(c)) => format!(" at {}:{}:{}", f, l, c),
                    (Some(f), Some(l), None) => format!(" at {}:{}", f, l),
                    (None, Some(l), Some(c)) => format!(" at line {}, column {}", l, c),
                    (None, Some(l), None) => format!(" at line {}", l),
                    _ => String::new(),
                };
                format!("Compilation error{}: {}", location, message)
            }
            
            ScriptError::RuntimeError { message, stack_trace } => {
                let mut result = format!("Runtime error: {}", message);
                if !stack_trace.is_empty() {
                    result.push_str("\nStack trace:");
                    for frame in stack_trace {
                        let func = frame.function_name.as_deref().unwrap_or("<anonymous>");
                        let location = match (&frame.filename, frame.line) {
                            (Some(f), Some(l)) => format!(" ({}:{})", f, l),
                            (None, Some(l)) => format!(" (line {})", l),
                            _ => String::new(),
                        };
                        result.push_str(&format!("\n  at {}{}", func, location));
                    }
                }
                result
            }
            
            ScriptError::TypeError { expected, actual, context } => {
                format!("Type error in {}: expected {}, got {}", context, expected.type_name(), actual)
            }
            
            _ => format!("{:?}", self),
        }
    }
    
    /// Check if error is recoverable
    pub fn is_recoverable(&self) -> bool {
        match self {
            ScriptError::CompilationError { .. } => false,
            ScriptError::SecurityError(_) => false,
            ScriptError::RuntimeError { .. } => true,
            ScriptError::TypeError { .. } => true,
            ScriptError::FunctionError { .. } => true,
            ScriptError::ResourceError { .. } => true,
            ScriptError::EngineError { .. } => false,
        }
    }
}
```

## Related Documentation

- [KRYON_TRAIT_SPECIFICATIONS.md](KRYON_TRAIT_SPECIFICATIONS.md) - Core trait definitions
- [KRYON_THREAD_SAFETY_SPECIFICATIONS.md](KRYON_THREAD_SAFETY_SPECIFICATIONS.md) - Thread safety requirements
- [KRYON_SCRIPT_SYSTEM.md](../runtime/KRYON_SCRIPT_SYSTEM.md) - Script system overview