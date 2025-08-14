/**
 * @file test_gpu_renderer.c
 * @brief Unit tests for Kryon GPU renderer and shader system
 */

#include "gpu_renderer.h"
#include "shader_compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// Simple test framework
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void test_##name(void); \
    static void run_test_##name(void) { \
        printf("Running test: " #name "... "); \
        tests_run++; \
        test_##name(); \
        tests_passed++; \
        printf("PASSED\n"); \
    } \
    static void test_##name(void)

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("FAILED\n  Assertion failed: " #condition "\n"); \
            printf("  File: %s, Line: %d\n", __FILE__, __LINE__); \
            exit(1); \
        } \
    } while(0)

// Test GPU backend availability
TEST(gpu_backend_availability) {
    // Check backend availability functions
    ASSERT(kryon_gpu_backend_is_available(KRYON_GPU_BACKEND_WEBGL) == true); // Should be available in web builds
    
    // Check backend names
    const char *webgl_name = kryon_gpu_backend_get_name(KRYON_GPU_BACKEND_WEBGL);
    ASSERT(webgl_name != NULL);
    ASSERT(strcmp(webgl_name, "WebGL") == 0);
    
    const char *opengl_name = kryon_gpu_backend_get_name(KRYON_GPU_BACKEND_OPENGL);
    ASSERT(opengl_name != NULL);
    ASSERT(strcmp(opengl_name, "OpenGL") == 0);
    
    const char *unknown_name = kryon_gpu_backend_get_name((KryonGPUBackend)999);
    ASSERT(unknown_name != NULL);
    ASSERT(strcmp(unknown_name, "Unknown") == 0);
}

// Test GPU renderer configuration
TEST(gpu_renderer_config) {
    KryonGPURendererConfig config = kryon_gpu_renderer_default_config();
    
    // Check default values
    ASSERT(config.preferred_backend == KRYON_GPU_BACKEND_WEBGL);
    ASSERT(config.enable_debug_layer == false);
    ASSERT(config.enable_validation == false);
    ASSERT(config.vsync_enabled == true);
    ASSERT(config.max_frames_in_flight == 2);
    ASSERT(config.staging_buffer_size == 1024 * 1024);
}

#ifndef KRYON_PLATFORM_WEB
// Test GPU renderer creation (stub for non-web platforms)
TEST(gpu_renderer_creation_stub) {
    KryonGPURendererConfig config = kryon_gpu_renderer_default_config();
    
    // Should return NULL on non-web platforms (stub implementation)
    KryonGPURenderer *renderer = kryon_gpu_renderer_create(&config, NULL);
    ASSERT(renderer == NULL);
    
    // Test invalid parameters
    ASSERT(kryon_gpu_renderer_create(NULL, NULL) == NULL);
}
#endif

// Test shader compiler creation and basic functionality
TEST(shader_compiler_basic) {
    KryonShaderCompiler *compiler = kryon_shader_compiler_create();
    ASSERT(compiler != NULL);
    
    // Check support for GLSL targets
    ASSERT(kryon_shader_compiler_supports_target(compiler, KRYON_SHADER_TARGET_GLSL_ES_300) == true);
    ASSERT(kryon_shader_compiler_supports_target(compiler, KRYON_SHADER_TARGET_GLSL_330) == true);
    
    // Test invalid parameters
    ASSERT(kryon_shader_compiler_supports_target(NULL, KRYON_SHADER_TARGET_GLSL_ES_300) == false);
    
    kryon_shader_compiler_destroy(compiler);
    
    // Test destroying NULL compiler (should be safe)
    kryon_shader_compiler_destroy(NULL);
}

// Test shader compilation configuration
TEST(shader_compile_config) {
    KryonShaderCompileConfig config = kryon_shader_compile_config_default(KRYON_SHADER_TARGET_GLSL_ES_300);
    
    // Check default values
    ASSERT(config.source_language == KRYON_SHADER_LANG_GLSL);
    ASSERT(config.target == KRYON_SHADER_TARGET_GLSL_ES_300);
    ASSERT(config.optimization == KRYON_SHADER_OPT_SPEED);
    ASSERT(config.enable_validation == true);
    ASSERT(config.enable_warnings == true);
    ASSERT(config.treat_warnings_as_errors == false);
    ASSERT(strcmp(config.entry_point, "main") == 0);
}

// Test shader hash calculation
TEST(shader_hash_calculation) {
    const char *shader_source = "#version 300 es\nvoid main() { gl_Position = vec4(0.0); }";
    KryonShaderCompileConfig config = kryon_shader_compile_config_default(KRYON_SHADER_TARGET_GLSL_ES_300);
    
    uint32_t hash1 = kryon_shader_calculate_hash(shader_source, &config);
    uint32_t hash2 = kryon_shader_calculate_hash(shader_source, &config);
    ASSERT(hash1 == hash2); // Same input should produce same hash
    
    // Different source should produce different hash
    const char *different_source = "#version 300 es\nvoid main() { gl_Position = vec4(1.0); }";
    uint32_t hash3 = kryon_shader_calculate_hash(different_source, &config);
    ASSERT(hash1 != hash3);
    
    // Different config should produce different hash
    KryonShaderCompileConfig different_config = config;
    different_config.optimization = KRYON_SHADER_OPT_SIZE;
    uint32_t hash4 = kryon_shader_calculate_hash(shader_source, &different_config);
    ASSERT(hash1 != hash4);
    
    // Test invalid parameters
    ASSERT(kryon_shader_calculate_hash(NULL, &config) == 0);
    ASSERT(kryon_shader_calculate_hash(shader_source, NULL) == 0);
}

// Test built-in shaders
TEST(builtin_shaders) {
    // Test UI shaders
    const char *ui_vertex = kryon_shader_builtin_ui_vertex(KRYON_SHADER_TARGET_GLSL_ES_300);
    ASSERT(ui_vertex != NULL);
    ASSERT(strlen(ui_vertex) > 0);
    ASSERT(strstr(ui_vertex, "#version 300 es") != NULL);
    ASSERT(strstr(ui_vertex, "void main") != NULL);
    
    const char *ui_fragment = kryon_shader_builtin_ui_fragment(KRYON_SHADER_TARGET_GLSL_ES_300);
    ASSERT(ui_fragment != NULL);
    ASSERT(strlen(ui_fragment) > 0);
    ASSERT(strstr(ui_fragment, "#version 300 es") != NULL);
    ASSERT(strstr(ui_fragment, "void main") != NULL);
    
    // Test text rendering shaders
    const char *text_vertex = kryon_shader_builtin_text_vertex(KRYON_SHADER_TARGET_GLSL_ES_300);
    ASSERT(text_vertex != NULL);
    ASSERT(strlen(text_vertex) > 0);
    
    const char *text_fragment = kryon_shader_builtin_text_fragment(KRYON_SHADER_TARGET_GLSL_ES_300);
    ASSERT(text_fragment != NULL);
    ASSERT(strlen(text_fragment) > 0);
    
    // Test post-processing shaders
    const char *post_vertex = kryon_shader_builtin_postprocess_vertex(KRYON_SHADER_TARGET_GLSL_ES_300);
    ASSERT(post_vertex != NULL);
    ASSERT(strlen(post_vertex) > 0);
    
    const char *blur_fragment = kryon_shader_builtin_blur_fragment(KRYON_SHADER_TARGET_GLSL_ES_300);
    ASSERT(blur_fragment != NULL);
    ASSERT(strlen(blur_fragment) > 0);
}

// Test shader language and target names
TEST(shader_names) {
    // Test language names
    ASSERT(strcmp(kryon_shader_language_name(KRYON_SHADER_LANG_GLSL), "GLSL") == 0);
    ASSERT(strcmp(kryon_shader_language_name(KRYON_SHADER_LANG_HLSL), "HLSL") == 0);
    ASSERT(strcmp(kryon_shader_language_name(KRYON_SHADER_LANG_MSL), "MSL") == 0);
    ASSERT(strcmp(kryon_shader_language_name(KRYON_SHADER_LANG_SPIRV), "SPIR-V") == 0);
    ASSERT(strcmp(kryon_shader_language_name(KRYON_SHADER_LANG_WGSL), "WGSL") == 0);
    ASSERT(strcmp(kryon_shader_language_name((KryonShaderLanguage)999), "Unknown") == 0);
    
    // Test target names
    ASSERT(strcmp(kryon_shader_target_name(KRYON_SHADER_TARGET_GLSL_ES_300), "GLSL ES 3.00") == 0);
    ASSERT(strcmp(kryon_shader_target_name(KRYON_SHADER_TARGET_GLSL_330), "GLSL 3.30") == 0);
    ASSERT(strcmp(kryon_shader_target_name(KRYON_SHADER_TARGET_HLSL_50), "HLSL 5.0") == 0);
    ASSERT(strcmp(kryon_shader_target_name((KryonShaderTarget)999), "Unknown") == 0);
}

// Test shader variable type utilities
TEST(shader_variable_types) {
    // Test type sizes
    ASSERT(kryon_shader_variable_type_size(KRYON_SHADER_VAR_FLOAT) == 4);
    ASSERT(kryon_shader_variable_type_size(KRYON_SHADER_VAR_VEC2) == 8);
    ASSERT(kryon_shader_variable_type_size(KRYON_SHADER_VAR_VEC3) == 12);
    ASSERT(kryon_shader_variable_type_size(KRYON_SHADER_VAR_VEC4) == 16);
    ASSERT(kryon_shader_variable_type_size(KRYON_SHADER_VAR_MAT4) == 64);
    ASSERT(kryon_shader_variable_type_size(KRYON_SHADER_VAR_SAMPLER2D) == 4);
    
    // Test component counts
    ASSERT(kryon_shader_variable_type_components(KRYON_SHADER_VAR_FLOAT) == 1);
    ASSERT(kryon_shader_variable_type_components(KRYON_SHADER_VAR_VEC2) == 2);
    ASSERT(kryon_shader_variable_type_components(KRYON_SHADER_VAR_VEC3) == 3);
    ASSERT(kryon_shader_variable_type_components(KRYON_SHADER_VAR_VEC4) == 4);
    ASSERT(kryon_shader_variable_type_components(KRYON_SHADER_VAR_MAT2) == 4);
    ASSERT(kryon_shader_variable_type_components(KRYON_SHADER_VAR_MAT3) == 9);
    ASSERT(kryon_shader_variable_type_components(KRYON_SHADER_VAR_MAT4) == 16);
}

// Test shader compilation
TEST(shader_compilation) {
    KryonShaderCompiler *compiler = kryon_shader_compiler_create();
    ASSERT(compiler != NULL);
    
    // Simple vertex shader
    const char *vertex_source = 
        "#version 300 es\n"
        "precision highp float;\n"
        "layout(location = 0) in vec2 a_position;\n"
        "void main() {\n"
        "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "}\n";
    
    KryonShaderCompileConfig config = kryon_shader_compile_config_default(KRYON_SHADER_TARGET_GLSL_ES_300);
    
    KryonShaderModule *module = kryon_shader_compiler_compile(
        compiler,
        KRYON_SHADER_VERTEX,
        vertex_source,
        &config
    );
    
    ASSERT(module != NULL);
    ASSERT(module->type == KRYON_SHADER_VERTEX);
    ASSERT(module->source_language == KRYON_SHADER_LANG_GLSL);
    ASSERT(module->target == KRYON_SHADER_TARGET_GLSL_ES_300);
    
    // Test module functions
    const char *source = kryon_shader_module_get_source(module);
    ASSERT(source != NULL);
    ASSERT(strcmp(source, vertex_source) == 0);
    
    size_t bytecode_size;
    const void *bytecode = kryon_shader_module_get_bytecode(module, &bytecode_size);
    ASSERT(bytecode != NULL);
    ASSERT(bytecode_size > 0);
    
    const KryonShaderReflection *reflection = kryon_shader_module_get_reflection(module);
    ASSERT(reflection != NULL);
    
    // Test validation
    ASSERT(kryon_shader_compiler_validate(compiler, module) == true);
    
    // Test cloning
    KryonShaderModule *clone = kryon_shader_module_clone(module);
    ASSERT(clone != NULL);
    ASSERT(clone->type == module->type);
    ASSERT(clone->hash == module->hash);
    
    // Test optimization
    KryonShaderModule *optimized = kryon_shader_compiler_optimize(compiler, module, KRYON_SHADER_OPT_SIZE);
    ASSERT(optimized != NULL);
    ASSERT(optimized->compile_config.optimization == KRYON_SHADER_OPT_SIZE);
    
    // Cleanup
    kryon_shader_module_destroy(module);
    kryon_shader_module_destroy(clone);
    kryon_shader_module_destroy(optimized);
    kryon_shader_compiler_destroy(compiler);
    
    // Test invalid parameters
    ASSERT(kryon_shader_compiler_compile(NULL, KRYON_SHADER_VERTEX, vertex_source, &config) == NULL);
    ASSERT(kryon_shader_compiler_compile(compiler, KRYON_SHADER_VERTEX, NULL, &config) == NULL);
}

// Test shader cache
TEST(shader_cache) {
    KryonShaderCache *cache = kryon_shader_cache_create(10, 1024 * 1024, NULL);
    ASSERT(cache != NULL);
    
    // Create a shader to cache
    KryonShaderCompiler *compiler = kryon_shader_compiler_create();
    ASSERT(compiler != NULL);
    
    const char *shader_source = "#version 300 es\nvoid main() { gl_Position = vec4(0.0); }";
    KryonShaderCompileConfig config = kryon_shader_compile_config_default(KRYON_SHADER_TARGET_GLSL_ES_300);
    
    KryonShaderModule *module = kryon_shader_compiler_compile(
        compiler,
        KRYON_SHADER_VERTEX,
        shader_source,
        &config
    );
    ASSERT(module != NULL);
    
    uint32_t hash = kryon_shader_calculate_hash(shader_source, &config);
    
    // Test cache miss
    KryonShaderModule *cached = kryon_shader_cache_get(cache, hash, &config);
    ASSERT(cached == NULL);
    
    // Store in cache
    kryon_shader_cache_store(cache, module);
    
    // Test cache hit
    cached = kryon_shader_cache_get(cache, hash, &config);
    ASSERT(cached != NULL);
    ASSERT(cached->hash == module->hash);
    
    // Test cache clear
    kryon_shader_cache_clear(cache);
    
    cached = kryon_shader_cache_get(cache, hash, &config);
    ASSERT(cached == NULL);
    
    // Cleanup
    kryon_shader_module_destroy(module);
    kryon_shader_module_destroy(cached);
    kryon_shader_cache_destroy(cache);
    kryon_shader_compiler_destroy(compiler);
    
    // Test invalid parameters
    ASSERT(kryon_shader_cache_create(0, 0, NULL) != NULL); // Should handle zero limits
    kryon_shader_cache_destroy(NULL); // Should be safe
}

// Test cross-compilation
TEST(shader_cross_compilation) {
    KryonShaderCompiler *compiler = kryon_shader_compiler_create();
    ASSERT(compiler != NULL);
    
    const char *vertex_source = 
        "#version 300 es\n"
        "precision highp float;\n"
        "layout(location = 0) in vec2 a_position;\n"
        "void main() {\n"
        "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "}\n";
    
    KryonShaderCompileConfig config = kryon_shader_compile_config_default(KRYON_SHADER_TARGET_GLSL_ES_300);
    
    KryonShaderModule *original = kryon_shader_compiler_compile(
        compiler,
        KRYON_SHADER_VERTEX,
        vertex_source,
        &config
    );
    ASSERT(original != NULL);
    
    // Cross-compile to GLSL 3.30
    KryonShaderModule *cross_compiled = kryon_shader_compiler_cross_compile(
        compiler,
        original,
        KRYON_SHADER_TARGET_GLSL_330
    );
    ASSERT(cross_compiled != NULL);
    ASSERT(cross_compiled->target == KRYON_SHADER_TARGET_GLSL_330);
    
    // Test unsupported target
    KryonShaderModule *unsupported = kryon_shader_compiler_cross_compile(
        compiler,
        original,
        KRYON_SHADER_TARGET_HLSL_50
    );
    ASSERT(unsupported == NULL); // Should fail for unsupported target
    
    // Cleanup
    kryon_shader_module_destroy(original);
    kryon_shader_module_destroy(cross_compiled);
    kryon_shader_compiler_destroy(compiler);
}

// Test error handling
TEST(shader_error_handling) {
    KryonShaderCompiler *compiler = kryon_shader_compiler_create();
    ASSERT(compiler != NULL);
    
    // Invalid shader source (missing version)
    const char *invalid_source = "void main() { gl_Position = vec4(0.0); }";
    KryonShaderCompileConfig config = kryon_shader_compile_config_default(KRYON_SHADER_TARGET_GLSL_ES_300);
    
    KryonShaderModule *module = kryon_shader_compiler_compile(
        compiler,
        KRYON_SHADER_VERTEX,
        invalid_source,
        &config
    );
    
    // Should still create module but validation should fail
    if (module) {
        ASSERT(kryon_shader_compiler_validate(compiler, module) == false);
        kryon_shader_module_destroy(module);
    }
    
    kryon_shader_compiler_destroy(compiler);
}

// Fix typo in shader cache test
static void fix_shader_cache_test(void) {
    // This was a typo in the shader cache test - should be KryonShaderCompiler
    // The test above has been fixed
}

// Main test runner
int main(void) {
    printf("=== Kryon GPU Renderer and Shader System Unit Tests ===\n\n");
    
    run_test_gpu_backend_availability();
    run_test_gpu_renderer_config();
#ifndef KRYON_PLATFORM_WEB
    run_test_gpu_renderer_creation_stub();
#endif
    run_test_shader_compiler_basic();
    run_test_shader_compile_config();
    run_test_shader_hash_calculation();
    run_test_builtin_shaders();
    run_test_shader_names();
    run_test_shader_variable_types();
    run_test_shader_compilation();
    run_test_shader_cache();
    run_test_shader_cross_compilation();
    run_test_shader_error_handling();
    
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    
    if (tests_passed == tests_run) {
        printf("All tests PASSED! ✅\n");
        return 0;
    } else {
        printf("Some tests FAILED! ❌\n");
        return 1;
    }
}