/**
 * @file gpu_renderer.h
 * @brief Kryon GPU Renderer - Hardware-accelerated rendering interface
 * 
 * Unified GPU rendering interface supporting multiple backends:
 * - WebGL (for web/Emscripten)
 * - OpenGL (desktop/mobile)
 * - Vulkan (modern desktop/mobile)
 * - WGPU (cross-platform modern API)
 * 
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_INTERNAL_GPU_RENDERER_H
#define KRYON_INTERNAL_GPU_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "internal/runtime.h"
#include "internal/software_renderer.h" // For shared types

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonGPURenderer KryonGPURenderer;
typedef struct KryonGPUShader KryonGPUShader;
typedef struct KryonGPUBuffer KryonGPUBuffer;
typedef struct KryonGPUTexture KryonGPUTexture;
typedef struct KryonGPUFramebuffer KryonGPUFramebuffer;
typedef struct KryonGPURenderPass KryonGPURenderPass;
typedef struct KryonGPUPipeline KryonGPUPipeline;
typedef struct KryonGPUCommandBuffer KryonGPUCommandBuffer;

// =============================================================================
// ENUMERATIONS
// =============================================================================

/**
 * @brief GPU renderer backend types
 */
typedef enum {
    KRYON_GPU_BACKEND_WEBGL = 0,    // WebGL 2.0 (web)
    KRYON_GPU_BACKEND_OPENGL,       // OpenGL 3.3+ (desktop/mobile)
    KRYON_GPU_BACKEND_OPENGLES,     // OpenGL ES 3.0+ (mobile/embedded)
    KRYON_GPU_BACKEND_VULKAN,       // Vulkan 1.0+ (modern)
    KRYON_GPU_BACKEND_WGPU,         // WebGPU/WGPU (cross-platform)
    KRYON_GPU_BACKEND_METAL,        // Metal (macOS/iOS)
    KRYON_GPU_BACKEND_D3D11,        // Direct3D 11 (Windows)
    KRYON_GPU_BACKEND_D3D12         // Direct3D 12 (Windows)
} KryonGPUBackend;

/**
 * @brief Shader types
 */
typedef enum {
    KRYON_SHADER_VERTEX = 0,
    KRYON_SHADER_FRAGMENT,
    KRYON_SHADER_GEOMETRY,
    KRYON_SHADER_COMPUTE,
    KRYON_SHADER_TESSELLATION_CONTROL,
    KRYON_SHADER_TESSELLATION_EVALUATION
} KryonShaderType;

/**
 * @brief Buffer types
 */
typedef enum {
    KRYON_BUFFER_VERTEX = 0,
    KRYON_BUFFER_INDEX,
    KRYON_BUFFER_UNIFORM,
    KRYON_BUFFER_STORAGE,
    KRYON_BUFFER_TEXTURE
} KryonBufferType;

/**
 * @brief Buffer usage patterns
 */
typedef enum {
    KRYON_BUFFER_USAGE_STATIC = 0,  // Written once, used many times
    KRYON_BUFFER_USAGE_DYNAMIC,     // Written occasionally, used many times
    KRYON_BUFFER_USAGE_STREAM       // Written every frame
} KryonBufferUsage;

/**
 * @brief Texture formats
 */
typedef enum {
    KRYON_TEXTURE_RGBA8 = 0,
    KRYON_TEXTURE_RGB8,
    KRYON_TEXTURE_RG8,
    KRYON_TEXTURE_R8,
    KRYON_TEXTURE_RGBA16F,
    KRYON_TEXTURE_RGBA32F,
    KRYON_TEXTURE_DEPTH24_STENCIL8,
    KRYON_TEXTURE_DEPTH32F
} KryonTextureFormat;

/**
 * @brief Texture filtering modes
 */
typedef enum {
    KRYON_TEXTURE_FILTER_NEAREST = 0,
    KRYON_TEXTURE_FILTER_LINEAR,
    KRYON_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST,
    KRYON_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST,
    KRYON_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR,
    KRYON_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR
} KryonTextureFilter;

/**
 * @brief Texture wrap modes
 */
typedef enum {
    KRYON_TEXTURE_WRAP_REPEAT = 0,
    KRYON_TEXTURE_WRAP_CLAMP_TO_EDGE,
    KRYON_TEXTURE_WRAP_CLAMP_TO_BORDER,
    KRYON_TEXTURE_WRAP_MIRRORED_REPEAT
} KryonTextureWrap;

/**
 * @brief Primitive topology
 */
typedef enum {
    KRYON_PRIMITIVE_TRIANGLES = 0,
    KRYON_PRIMITIVE_TRIANGLE_STRIP,
    KRYON_PRIMITIVE_TRIANGLE_FAN,
    KRYON_PRIMITIVE_LINES,
    KRYON_PRIMITIVE_LINE_STRIP,
    KRYON_PRIMITIVE_POINTS
} KryonPrimitiveTopology;

/**
 * @brief Blend factors
 */
typedef enum {
    KRYON_BLEND_ZERO = 0,
    KRYON_BLEND_ONE,
    KRYON_BLEND_SRC_COLOR,
    KRYON_BLEND_ONE_MINUS_SRC_COLOR,
    KRYON_BLEND_DST_COLOR,
    KRYON_BLEND_ONE_MINUS_DST_COLOR,
    KRYON_BLEND_SRC_ALPHA,
    KRYON_BLEND_ONE_MINUS_SRC_ALPHA,
    KRYON_BLEND_DST_ALPHA,
    KRYON_BLEND_ONE_MINUS_DST_ALPHA
} KryonBlendFactor;

/**
 * @brief Comparison functions
 */
typedef enum {
    KRYON_COMPARE_NEVER = 0,
    KRYON_COMPARE_LESS,
    KRYON_COMPARE_EQUAL,
    KRYON_COMPARE_LESS_EQUAL,
    KRYON_COMPARE_GREATER,
    KRYON_COMPARE_NOT_EQUAL,
    KRYON_COMPARE_GREATER_EQUAL,
    KRYON_COMPARE_ALWAYS
} KryonCompareFunction;

// =============================================================================
// STRUCTURES
// =============================================================================

/**
 * @brief GPU renderer capabilities
 */
typedef struct {
    // Backend info
    KryonGPUBackend backend;
    const char *backend_name;
    const char *vendor;
    const char *renderer;
    const char *version;
    
    // Feature support
    bool supports_geometry_shaders;
    bool supports_tessellation;
    bool supports_compute_shaders;
    bool supports_instancing;
    bool supports_multi_draw_indirect;
    bool supports_texture_arrays;
    bool supports_cubemap_arrays;
    bool supports_seamless_cubemaps;
    bool supports_anisotropic_filtering;
    bool supports_texture_compression;
    
    // Limits
    uint32_t max_texture_size;
    uint32_t max_texture_array_layers;
    uint32_t max_framebuffer_width;
    uint32_t max_framebuffer_height;
    uint32_t max_vertex_attributes;
    uint32_t max_uniform_buffer_bindings;
    uint32_t max_texture_units;
    float max_anisotropy;
    
    // Memory info (if available)
    size_t total_memory;
    size_t available_memory;
} KryonGPUCapabilities;

/**
 * @brief Vertex attribute description
 */
typedef struct {
    uint32_t location;          // Shader attribute location
    KryonTextureFormat format;  // Attribute format
    uint32_t offset;            // Offset in vertex structure
    const char *name;           // Attribute name (for debugging)
} KryonVertexAttribute;

/**
 * @brief Vertex layout description
 */
typedef struct {
    KryonVertexAttribute *attributes;
    uint32_t attribute_count;
    uint32_t stride;            // Size of one vertex in bytes
} KryonVertexLayout;

/**
 * @brief Texture descriptor
 */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t depth;             // For 3D textures (1 for 2D)
    uint32_t mip_levels;        // Number of mip levels (1 for no mipmaps)
    KryonTextureFormat format;
    KryonTextureFilter min_filter;
    KryonTextureFilter mag_filter;
    KryonTextureWrap wrap_u;
    KryonTextureWrap wrap_v;
    KryonTextureWrap wrap_w;
    bool generate_mipmaps;
    const void *data;           // Initial texture data (can be NULL)
} KryonTextureDescriptor;

/**
 * @brief Render state
 */
typedef struct {
    // Blending
    bool blend_enabled;
    KryonBlendFactor src_blend;
    KryonBlendFactor dst_blend;
    KryonBlendFactor src_alpha_blend;
    KryonBlendFactor dst_alpha_blend;
    
    // Depth testing
    bool depth_test_enabled;
    bool depth_write_enabled;
    KryonCompareFunction depth_function;
    
    // Stencil testing
    bool stencil_test_enabled;
    
    // Face culling
    bool cull_face_enabled;
    bool cull_front_face;       // true = cull front, false = cull back
    bool front_face_ccw;        // true = CCW is front, false = CW is front
    
    // Wireframe
    bool wireframe_enabled;
    
    // Scissor test
    bool scissor_test_enabled;
    KryonRect scissor_rect;
} KryonRenderState;

/**
 * @brief GPU render statistics
 */
typedef struct {
    uint64_t frames_rendered;
    uint64_t draw_calls;
    uint64_t vertices_rendered;
    uint64_t triangles_rendered;
    uint64_t texture_switches;
    uint64_t shader_switches;
    uint64_t buffer_uploads;
    size_t gpu_memory_used;
    double render_time;
    double gpu_time;            // GPU timing (if available)
} KryonGPURenderStats;

/**
 * @brief GPU renderer configuration
 */
typedef struct {
    KryonGPUBackend preferred_backend;
    bool enable_debug_layer;
    bool enable_validation;
    bool enable_gpu_timing;
    bool enable_shader_cache;
    bool vsync_enabled;
    uint32_t max_frames_in_flight;
    size_t staging_buffer_size;
} KryonGPURendererConfig;

// =============================================================================
// MAIN RENDERER INTERFACE
// =============================================================================

/**
 * @brief Create GPU renderer
 * @param config Renderer configuration
 * @param platform_data Platform-specific data (window handle, etc.)
 * @return Renderer instance or NULL on failure
 */
KryonGPURenderer *kryon_gpu_renderer_create(const KryonGPURendererConfig *config, 
                                           void *platform_data);

/**
 * @brief Destroy GPU renderer
 * @param renderer Renderer to destroy
 */
void kryon_gpu_renderer_destroy(KryonGPURenderer *renderer);

/**
 * @brief Get renderer capabilities
 * @param renderer Renderer instance
 * @return Capabilities structure
 */
const KryonGPUCapabilities *kryon_gpu_renderer_get_capabilities(const KryonGPURenderer *renderer);

/**
 * @brief Resize render target
 * @param renderer Renderer instance
 * @param width New width
 * @param height New height
 */
void kryon_gpu_renderer_resize(KryonGPURenderer *renderer, uint32_t width, uint32_t height);

/**
 * @brief Begin frame
 * @param renderer Renderer instance
 * @return Command buffer for this frame
 */
KryonGPUCommandBuffer *kryon_gpu_renderer_begin_frame(KryonGPURenderer *renderer);

/**
 * @brief End frame and present
 * @param renderer Renderer instance
 * @param cmd_buffer Command buffer to submit
 */
void kryon_gpu_renderer_end_frame(KryonGPURenderer *renderer, KryonGPUCommandBuffer *cmd_buffer);

/**
 * @brief Wait for GPU to complete all work
 * @param renderer Renderer instance
 */
void kryon_gpu_renderer_wait_idle(KryonGPURenderer *renderer);

/**
 * @brief Get render statistics
 * @param renderer Renderer instance
 * @return Statistics structure
 */
const KryonGPURenderStats *kryon_gpu_renderer_get_stats(const KryonGPURenderer *renderer);

// =============================================================================
// SHADER MANAGEMENT
// =============================================================================

/**
 * @brief Create shader from source
 * @param renderer Renderer instance
 * @param type Shader type
 * @param source Shader source code
 * @param entry_point Entry point function name (can be NULL for main)
 * @return Shader handle or NULL on failure
 */
KryonGPUShader *kryon_gpu_shader_create(KryonGPURenderer *renderer, 
                                       KryonShaderType type,
                                       const char *source,
                                       const char *entry_point);

/**
 * @brief Create shader from bytecode
 * @param renderer Renderer instance
 * @param type Shader type
 * @param bytecode Compiled shader bytecode
 * @param size Bytecode size in bytes
 * @param entry_point Entry point function name (can be NULL for main)
 * @return Shader handle or NULL on failure
 */
KryonGPUShader *kryon_gpu_shader_create_from_bytecode(KryonGPURenderer *renderer,
                                                     KryonShaderType type,
                                                     const void *bytecode,
                                                     size_t size,
                                                     const char *entry_point);

/**
 * @brief Destroy shader
 * @param renderer Renderer instance
 * @param shader Shader to destroy
 */
void kryon_gpu_shader_destroy(KryonGPURenderer *renderer, KryonGPUShader *shader);

// =============================================================================
// BUFFER MANAGEMENT
// =============================================================================

/**
 * @brief Create buffer
 * @param renderer Renderer instance
 * @param type Buffer type
 * @param usage Usage pattern
 * @param size Size in bytes
 * @param data Initial data (can be NULL)
 * @return Buffer handle or NULL on failure
 */
KryonGPUBuffer *kryon_gpu_buffer_create(KryonGPURenderer *renderer,
                                       KryonBufferType type,
                                       KryonBufferUsage usage,
                                       size_t size,
                                       const void *data);

/**
 * @brief Update buffer data
 * @param renderer Renderer instance
 * @param buffer Buffer to update
 * @param offset Offset in bytes
 * @param size Size in bytes
 * @param data New data
 */
void kryon_gpu_buffer_update(KryonGPURenderer *renderer,
                            KryonGPUBuffer *buffer,
                            size_t offset,
                            size_t size,
                            const void *data);

/**
 * @brief Destroy buffer
 * @param renderer Renderer instance
 * @param buffer Buffer to destroy
 */
void kryon_gpu_buffer_destroy(KryonGPURenderer *renderer, KryonGPUBuffer *buffer);

// =============================================================================
// TEXTURE MANAGEMENT
// =============================================================================

/**
 * @brief Create texture
 * @param renderer Renderer instance
 * @param descriptor Texture descriptor
 * @return Texture handle or NULL on failure
 */
KryonGPUTexture *kryon_gpu_texture_create(KryonGPURenderer *renderer,
                                         const KryonTextureDescriptor *descriptor);

/**
 * @brief Update texture data
 * @param renderer Renderer instance
 * @param texture Texture to update
 * @param x X offset
 * @param y Y offset
 * @param width Width of region
 * @param height Height of region
 * @param data New texture data
 */
void kryon_gpu_texture_update(KryonGPURenderer *renderer,
                             KryonGPUTexture *texture,
                             uint32_t x, uint32_t y,
                             uint32_t width, uint32_t height,
                             const void *data);

/**
 * @brief Generate mipmaps for texture
 * @param renderer Renderer instance
 * @param texture Texture to generate mipmaps for
 */
void kryon_gpu_texture_generate_mipmaps(KryonGPURenderer *renderer, KryonGPUTexture *texture);

/**
 * @brief Destroy texture
 * @param renderer Renderer instance
 * @param texture Texture to destroy
 */
void kryon_gpu_texture_destroy(KryonGPURenderer *renderer, KryonGPUTexture *texture);

// =============================================================================
// PIPELINE MANAGEMENT
// =============================================================================

/**
 * @brief Create render pipeline
 * @param renderer Renderer instance
 * @param vertex_shader Vertex shader
 * @param fragment_shader Fragment shader
 * @param vertex_layout Vertex input layout
 * @param render_state Render state
 * @return Pipeline handle or NULL on failure
 */
KryonGPUPipeline *kryon_gpu_pipeline_create(KryonGPURenderer *renderer,
                                           KryonGPUShader *vertex_shader,
                                           KryonGPUShader *fragment_shader,
                                           const KryonVertexLayout *vertex_layout,
                                           const KryonRenderState *render_state);

/**
 * @brief Destroy pipeline
 * @param renderer Renderer instance
 * @param pipeline Pipeline to destroy
 */
void kryon_gpu_pipeline_destroy(KryonGPURenderer *renderer, KryonGPUPipeline *pipeline);

// =============================================================================
// COMMAND BUFFER INTERFACE
// =============================================================================

/**
 * @brief Begin render pass
 * @param cmd_buffer Command buffer
 * @param framebuffer Target framebuffer (NULL for default)
 * @param clear_color Clear color (can be NULL)
 * @param clear_depth Clear depth value
 * @param clear_stencil Clear stencil value
 */
void kryon_gpu_cmd_begin_render_pass(KryonGPUCommandBuffer *cmd_buffer,
                                    KryonGPUFramebuffer *framebuffer,
                                    const KryonColor *clear_color,
                                    float clear_depth,
                                    uint32_t clear_stencil);

/**
 * @brief End render pass
 * @param cmd_buffer Command buffer
 */
void kryon_gpu_cmd_end_render_pass(KryonGPUCommandBuffer *cmd_buffer);

/**
 * @brief Set viewport
 * @param cmd_buffer Command buffer
 * @param x X offset
 * @param y Y offset
 * @param width Width
 * @param height Height
 * @param min_depth Minimum depth
 * @param max_depth Maximum depth
 */
void kryon_gpu_cmd_set_viewport(KryonGPUCommandBuffer *cmd_buffer,
                               float x, float y,
                               float width, float height,
                               float min_depth, float max_depth);

/**
 * @brief Bind pipeline
 * @param cmd_buffer Command buffer
 * @param pipeline Pipeline to bind
 */
void kryon_gpu_cmd_bind_pipeline(KryonGPUCommandBuffer *cmd_buffer, KryonGPUPipeline *pipeline);

/**
 * @brief Bind vertex buffer
 * @param cmd_buffer Command buffer
 * @param buffer Vertex buffer
 * @param offset Offset in buffer
 */
void kryon_gpu_cmd_bind_vertex_buffer(KryonGPUCommandBuffer *cmd_buffer,
                                     KryonGPUBuffer *buffer,
                                     size_t offset);

/**
 * @brief Bind index buffer
 * @param cmd_buffer Command buffer
 * @param buffer Index buffer
 * @param offset Offset in buffer
 */
void kryon_gpu_cmd_bind_index_buffer(KryonGPUCommandBuffer *cmd_buffer,
                                    KryonGPUBuffer *buffer,
                                    size_t offset);

/**
 * @brief Bind texture
 * @param cmd_buffer Command buffer
 * @param slot Texture slot
 * @param texture Texture to bind
 */
void kryon_gpu_cmd_bind_texture(KryonGPUCommandBuffer *cmd_buffer,
                               uint32_t slot,
                               KryonGPUTexture *texture);

/**
 * @brief Draw primitives
 * @param cmd_buffer Command buffer
 * @param vertex_count Number of vertices
 * @param instance_count Number of instances
 * @param first_vertex First vertex index
 * @param first_instance First instance index
 */
void kryon_gpu_cmd_draw(KryonGPUCommandBuffer *cmd_buffer,
                       uint32_t vertex_count,
                       uint32_t instance_count,
                       uint32_t first_vertex,
                       uint32_t first_instance);

/**
 * @brief Draw indexed primitives
 * @param cmd_buffer Command buffer
 * @param index_count Number of indices
 * @param instance_count Number of instances
 * @param first_index First index
 * @param vertex_offset Vertex offset
 * @param first_instance First instance index
 */
void kryon_gpu_cmd_draw_indexed(KryonGPUCommandBuffer *cmd_buffer,
                               uint32_t index_count,
                               uint32_t instance_count,
                               uint32_t first_index,
                               int32_t vertex_offset,
                               uint32_t first_instance);

// =============================================================================
// HIGH-LEVEL RENDERING INTERFACE
// =============================================================================

/**
 * @brief Render element tree using GPU acceleration
 * @param renderer GPU renderer instance
 * @param runtime Runtime with element tree
 */
void kryon_gpu_renderer_render_elements(KryonGPURenderer *renderer, KryonRuntime *runtime);

/**
 * @brief Render single element using GPU acceleration
 * @param renderer GPU renderer instance
 * @param element Element to render
 */
void kryon_gpu_renderer_render_element(KryonGPURenderer *renderer, const KryonElement *element);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Get default renderer configuration
 * @return Default configuration
 */
KryonGPURendererConfig kryon_gpu_renderer_default_config(void);

/**
 * @brief Check if GPU backend is available
 * @param backend Backend to check
 * @return true if available
 */
bool kryon_gpu_backend_is_available(KryonGPUBackend backend);

/**
 * @brief Get backend name string
 * @param backend Backend type
 * @return Backend name
 */
const char *kryon_gpu_backend_get_name(KryonGPUBackend backend);

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_GPU_RENDERER_H