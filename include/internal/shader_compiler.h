/**
 * @file shader_compiler.h
 * @brief Kryon Shader Compiler - Cross-platform shader compilation and management
 * 
 * Provides shader compilation, cross-compilation between different shader languages,
 * and runtime shader management with caching and validation.
 * 
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_INTERNAL_SHADER_COMPILER_H
#define KRYON_INTERNAL_SHADER_COMPILER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonShaderCompiler KryonShaderCompiler;
typedef struct KryonShaderModule KryonShaderModule;
typedef struct KryonShaderCache KryonShaderCache;
typedef struct KryonShaderReflection KryonShaderReflection;

// =============================================================================
// ENUMERATIONS
// =============================================================================

/**
 * @brief Shader types
 */
typedef enum {
    KRYON_SHADER_VERTEX,
    KRYON_SHADER_FRAGMENT,
    KRYON_SHADER_PROGRAM
} KryonShaderType;

/**
 * @brief Shader source languages
 */
typedef enum {
    KRYON_SHADER_LANG_GLSL = 0,     // OpenGL Shading Language
    KRYON_SHADER_LANG_HLSL,         // High Level Shading Language (DirectX)
    KRYON_SHADER_LANG_MSL,          // Metal Shading Language
    KRYON_SHADER_LANG_SPIRV,        // SPIR-V bytecode
    KRYON_SHADER_LANG_WGSL,         // WebGPU Shading Language
    KRYON_SHADER_LANG_KRYON         // Kryon Shader Language (custom)
} KryonShaderLanguage;

/**
 * @brief Shader compilation targets
 */
typedef enum {
    KRYON_SHADER_TARGET_GLSL_ES_300 = 0,    // GLSL ES 3.00 (WebGL 2.0)
    KRYON_SHADER_TARGET_GLSL_330,           // GLSL 3.30 (OpenGL 3.3)
    KRYON_SHADER_TARGET_GLSL_450,           // GLSL 4.50 (OpenGL 4.5)
    KRYON_SHADER_TARGET_HLSL_50,            // HLSL Shader Model 5.0
    KRYON_SHADER_TARGET_HLSL_60,            // HLSL Shader Model 6.0
    KRYON_SHADER_TARGET_MSL_20,             // Metal 2.0
    KRYON_SHADER_TARGET_SPIRV_10,           // SPIR-V 1.0
    KRYON_SHADER_TARGET_SPIRV_15,           // SPIR-V 1.5
    KRYON_SHADER_TARGET_WGSL_10             // WGSL 1.0
} KryonShaderTarget;

/**
 * @brief Shader optimization levels
 */
typedef enum {
    KRYON_SHADER_OPT_NONE = 0,      // No optimization
    KRYON_SHADER_OPT_SIZE,          // Optimize for size
    KRYON_SHADER_OPT_SPEED,         // Optimize for performance
    KRYON_SHADER_OPT_DEBUG          // Debug build (no optimization)
} KryonShaderOptimization;

/**
 * @brief Shader variable types for reflection
 */
typedef enum {
    KRYON_SHADER_VAR_FLOAT = 0,
    KRYON_SHADER_VAR_VEC2,
    KRYON_SHADER_VAR_VEC3,
    KRYON_SHADER_VAR_VEC4,
    KRYON_SHADER_VAR_INT,
    KRYON_SHADER_VAR_IVEC2,
    KRYON_SHADER_VAR_IVEC3,
    KRYON_SHADER_VAR_IVEC4,
    KRYON_SHADER_VAR_UINT,
    KRYON_SHADER_VAR_UVEC2,
    KRYON_SHADER_VAR_UVEC3,
    KRYON_SHADER_VAR_UVEC4,
    KRYON_SHADER_VAR_BOOL,
    KRYON_SHADER_VAR_BVEC2,
    KRYON_SHADER_VAR_BVEC3,
    KRYON_SHADER_VAR_BVEC4,
    KRYON_SHADER_VAR_MAT2,
    KRYON_SHADER_VAR_MAT3,
    KRYON_SHADER_VAR_MAT4,
    KRYON_SHADER_VAR_SAMPLER2D,
    KRYON_SHADER_VAR_SAMPLER3D,
    KRYON_SHADER_VAR_SAMPLER_CUBE,
    KRYON_SHADER_VAR_SAMPLER2D_ARRAY
} KryonShaderVariableType;

// =============================================================================
// STRUCTURES
// =============================================================================

/**
 * @brief Shader compilation configuration
 */
typedef struct {
    KryonShaderLanguage source_language;
    KryonShaderTarget target;
    KryonShaderOptimization optimization;
    bool enable_debug_info;
    bool enable_validation;
    bool enable_warnings;
    bool treat_warnings_as_errors;
    
    // Preprocessor defines
    const char **defines;
    size_t define_count;
    
    // Include paths
    const char **include_paths;
    size_t include_path_count;
    
    // Entry point function name
    const char *entry_point;
} KryonShaderCompileConfig;

/**
 * @brief Shader variable descriptor (for reflection)
 */
typedef struct {
    const char *name;
    KryonShaderVariableType type;
    uint32_t location;              // Attribute/uniform location
    uint32_t binding;               // Buffer/texture binding
    uint32_t offset;                // Offset in uniform buffer
    uint32_t size;                  // Size in bytes
    uint32_t array_size;            // Array size (1 for non-arrays)
    bool is_array;
} KryonShaderVariable;

/**
 * @brief Shader reflection information
 */
struct KryonShaderReflection {
    // Input attributes (vertex shaders)
    KryonShaderVariable *inputs;
    uint32_t input_count;
    
    // Output attributes
    KryonShaderVariable *outputs;
    uint32_t output_count;
    
    // Uniform variables
    KryonShaderVariable *uniforms;
    uint32_t uniform_count;
    
    // Uniform buffers
    KryonShaderVariable *uniform_buffers;
    uint32_t uniform_buffer_count;
    
    // Textures/samplers
    KryonShaderVariable *textures;
    uint32_t texture_count;
    
    // Shader statistics
    uint32_t instruction_count;
    uint32_t temp_register_count;
    uint32_t constant_buffer_size;
    
    // Workgroup size (compute shaders)
    uint32_t workgroup_size[3];
};

/**
 * @brief Compiled shader module
 */
struct KryonShaderModule {
    KryonShaderType type;
    KryonShaderLanguage source_language;
    KryonShaderTarget target;
    uint32_t hash;                  // Source code hash for caching
    
    // Compiled bytecode
    void *bytecode;
    size_t bytecode_size;
    
    // Reflection information
    KryonShaderReflection reflection;
    
    // Source code (for debugging)
    char *source_code;
    size_t source_size;
    
    // Compilation metadata
    const char *entry_point;
    KryonShaderCompileConfig compile_config;
    double compile_time;
    
    // Error information
    char *error_log;
    char *warning_log;
};

/**
 * @brief Shader compiler instance
 */
struct KryonShaderCompiler {
    // Supported targets
    bool supports_glsl;
    bool supports_hlsl;
    bool supports_msl;
    bool supports_spirv;
    bool supports_wgsl;
    
    // Cross-compilation capabilities
    bool can_cross_compile;
    KryonShaderTarget *supported_targets;
    size_t target_count;
    
    // Compiler backend (e.g., glslang, DXC, spirv-cross)
    void *backend_data;
    
    // Statistics
    uint64_t shaders_compiled;
    uint64_t cache_hits;
    uint64_t cache_misses;
    double total_compile_time;
};

/**
 * @brief Shader cache for compiled shaders
 */
struct KryonShaderCache {
    // Cache storage
    KryonShaderModule **modules;
    size_t module_count;
    size_t module_capacity;
    
    // Cache configuration
    size_t max_cache_size;          // Maximum number of cached shaders
    size_t max_memory_usage;        // Maximum memory usage in bytes
    bool enable_disk_cache;         // Enable persistent disk cache
    const char *cache_directory;    // Directory for disk cache
    
    // Statistics
    uint64_t cache_hits;
    uint64_t cache_misses;
    size_t memory_usage;
};

// =============================================================================
// COMPILER INTERFACE
// =============================================================================

/**
 * @brief Create shader compiler
 * @return Compiler instance or NULL on failure
 */
KryonShaderCompiler *kryon_shader_compiler_create(void);

/**
 * @brief Destroy shader compiler
 * @param compiler Compiler to destroy
 */
void kryon_shader_compiler_destroy(KryonShaderCompiler *compiler);

/**
 * @brief Check if target is supported
 * @param compiler Compiler instance
 * @param target Compilation target
 * @return true if supported
 */
bool kryon_shader_compiler_supports_target(const KryonShaderCompiler *compiler, 
                                          KryonShaderTarget target);

/**
 * @brief Compile shader from source
 * @param compiler Compiler instance
 * @param type Shader type
 * @param source Source code
 * @param config Compilation configuration
 * @return Compiled shader module or NULL on failure
 */
KryonShaderModule *kryon_shader_compiler_compile(KryonShaderCompiler *compiler,
                                                KryonShaderType type,
                                                const char *source,
                                                const KryonShaderCompileConfig *config);

/**
 * @brief Compile shader from file
 * @param compiler Compiler instance
 * @param type Shader type
 * @param filename Source file path
 * @param config Compilation configuration
 * @return Compiled shader module or NULL on failure
 */
KryonShaderModule *kryon_shader_compiler_compile_file(KryonShaderCompiler *compiler,
                                                     KryonShaderType type,
                                                     const char *filename,
                                                     const KryonShaderCompileConfig *config);

/**
 * @brief Cross-compile shader to different target
 * @param compiler Compiler instance
 * @param module Source shader module
 * @param target Target language/version
 * @return Cross-compiled module or NULL on failure
 */
KryonShaderModule *kryon_shader_compiler_cross_compile(KryonShaderCompiler *compiler,
                                                      const KryonShaderModule *module,
                                                      KryonShaderTarget target);

/**
 * @brief Validate shader module
 * @param compiler Compiler instance
 * @param module Shader module to validate
 * @return true if valid
 */
bool kryon_shader_compiler_validate(KryonShaderCompiler *compiler,
                                   const KryonShaderModule *module);

/**
 * @brief Optimize shader module
 * @param compiler Compiler instance
 * @param module Shader module to optimize
 * @param optimization Optimization level
 * @return Optimized module or NULL on failure
 */
KryonShaderModule *kryon_shader_compiler_optimize(KryonShaderCompiler *compiler,
                                                 const KryonShaderModule *module,
                                                 KryonShaderOptimization optimization);

// =============================================================================
// SHADER MODULE MANAGEMENT
// =============================================================================

/**
 * @brief Destroy shader module
 * @param module Module to destroy
 */
void kryon_shader_module_destroy(KryonShaderModule *module);

/**
 * @brief Get shader reflection information
 * @param module Shader module
 * @return Reflection data
 */
const KryonShaderReflection *kryon_shader_module_get_reflection(const KryonShaderModule *module);

/**
 * @brief Get compiled bytecode
 * @param module Shader module
 * @param size Output for bytecode size
 * @return Bytecode data
 */
const void *kryon_shader_module_get_bytecode(const KryonShaderModule *module, size_t *size);

/**
 * @brief Get source code
 * @param module Shader module
 * @return Source code string
 */
const char *kryon_shader_module_get_source(const KryonShaderModule *module);

/**
 * @brief Get compilation errors
 * @param module Shader module
 * @return Error log string (can be NULL)
 */
const char *kryon_shader_module_get_errors(const KryonShaderModule *module);

/**
 * @brief Get compilation warnings
 * @param module Shader module
 * @return Warning log string (can be NULL)
 */
const char *kryon_shader_module_get_warnings(const KryonShaderModule *module);

/**
 * @brief Clone shader module
 * @param module Module to clone
 * @return Cloned module or NULL on failure
 */
KryonShaderModule *kryon_shader_module_clone(const KryonShaderModule *module);

// =============================================================================
// SHADER CACHE MANAGEMENT
// =============================================================================

/**
 * @brief Create shader cache
 * @param max_size Maximum number of cached shaders
 * @param max_memory Maximum memory usage in bytes
 * @param cache_dir Directory for persistent cache (can be NULL)
 * @return Cache instance or NULL on failure
 */
KryonShaderCache *kryon_shader_cache_create(size_t max_size, 
                                           size_t max_memory,
                                           const char *cache_dir);

/**
 * @brief Destroy shader cache
 * @param cache Cache to destroy
 */
void kryon_shader_cache_destroy(KryonShaderCache *cache);

/**
 * @brief Get shader from cache
 * @param cache Cache instance
 * @param hash Shader source hash
 * @param config Compilation configuration
 * @return Cached module or NULL if not found
 */
KryonShaderModule *kryon_shader_cache_get(KryonShaderCache *cache,
                                         uint32_t hash,
                                         const KryonShaderCompileConfig *config);

/**
 * @brief Store shader in cache
 * @param cache Cache instance
 * @param module Shader module to cache
 */
void kryon_shader_cache_store(KryonShaderCache *cache, KryonShaderModule *module);

/**
 * @brief Clear cache
 * @param cache Cache instance
 */
void kryon_shader_cache_clear(KryonShaderCache *cache);

/**
 * @brief Save cache to disk
 * @param cache Cache instance
 * @return true on success
 */
bool kryon_shader_cache_save(KryonShaderCache *cache);

/**
 * @brief Load cache from disk
 * @param cache Cache instance
 * @return true on success
 */
bool kryon_shader_cache_load(KryonShaderCache *cache);

// =============================================================================
// BUILT-IN SHADERS
// =============================================================================

/**
 * @brief Get built-in UI vertex shader
 * @param target Compilation target
 * @return Shader source code
 */
const char *kryon_shader_builtin_ui_vertex(KryonShaderTarget target);

/**
 * @brief Get built-in UI fragment shader
 * @param target Compilation target
 * @return Shader source code
 */
const char *kryon_shader_builtin_ui_fragment(KryonShaderTarget target);

/**
 * @brief Get built-in text rendering vertex shader
 * @param target Compilation target
 * @return Shader source code
 */
const char *kryon_shader_builtin_text_vertex(KryonShaderTarget target);

/**
 * @brief Get built-in text rendering fragment shader
 * @param target Compilation target
 * @return Shader source code
 */
const char *kryon_shader_builtin_text_fragment(KryonShaderTarget target);

/**
 * @brief Get built-in post-processing vertex shader
 * @param target Compilation target
 * @return Shader source code
 */
const char *kryon_shader_builtin_postprocess_vertex(KryonShaderTarget target);

/**
 * @brief Get built-in blur fragment shader
 * @param target Compilation target
 * @return Shader source code
 */
const char *kryon_shader_builtin_blur_fragment(KryonShaderTarget target);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Get default compilation configuration
 * @param target Compilation target
 * @return Default configuration
 */
KryonShaderCompileConfig kryon_shader_compile_config_default(KryonShaderTarget target);

/**
 * @brief Calculate hash of shader source
 * @param source Source code
 * @param config Compilation configuration
 * @return Hash value
 */
uint32_t kryon_shader_calculate_hash(const char *source, 
                                    const KryonShaderCompileConfig *config);

/**
 * @brief Get shader language name
 * @param language Shader language
 * @return Language name string
 */
const char *kryon_shader_language_name(KryonShaderLanguage language);

/**
 * @brief Get shader target name
 * @param target Shader target
 * @return Target name string
 */
const char *kryon_shader_target_name(KryonShaderTarget target);

/**
 * @brief Get variable type size in bytes
 * @param type Variable type
 * @return Size in bytes
 */
size_t kryon_shader_variable_type_size(KryonShaderVariableType type);

/**
 * @brief Get variable type component count
 * @param type Variable type
 * @return Number of components
 */
uint32_t kryon_shader_variable_type_components(KryonShaderVariableType type);

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_SHADER_COMPILER_H