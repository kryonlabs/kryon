/**
 * @file shader_compiler.c
 * @brief Kryon Shader Compiler Implementation (Thin Abstraction Layer)
 */

#include "internal/graphics.h"
#include "internal/memory.h"
#include "internal/renderer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Include renderer-specific headers
#ifdef KRYON_RENDERER_RAYLIB
    #include <raylib.h>
    #include <rlgl.h>
#endif

#ifdef KRYON_RENDERER_SDL2
    #include <SDL2/SDL.h>
    #ifdef SDL_VIDEO_OPENGL
        #include <GL/gl.h>
    #endif
#endif

// =============================================================================
// SHADER MANAGEMENT STRUCTURES
// =============================================================================

typedef struct KryonShaderCache {
    uint32_t id;
    char* name;
    KryonShaderType type;
    void* renderer_shader; // Renderer-specific shader object
    KryonShader kryon_shader; // Our abstracted shader
    struct KryonShaderCache* next;
} KryonShaderCache;

typedef struct {
    KryonShaderCache* cached_shaders;
    uint32_t next_shader_id;
    size_t shader_count;
    
    // Built-in shaders
    uint32_t default_vertex_shader;
    uint32_t default_fragment_shader;
    uint32_t default_program;
    
} KryonShaderCompiler;

static KryonShaderCompiler g_shader_compiler = {0};

// =============================================================================
// BUILT-IN SHADER SOURCES
// =============================================================================

// Default vertex shader (compatible across renderers)
static const char* default_vertex_shader_source = 
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec2 aTexCoord;\n"
    "layout (location = 2) in vec4 aColor;\n"
    "\n"
    "uniform mat4 mvp;\n"
    "\n"
    "out vec2 fragTexCoord;\n"
    "out vec4 fragColor;\n"
    "\n"
    "void main() {\n"
    "    fragTexCoord = aTexCoord;\n"
    "    fragColor = aColor;\n"
    "    gl_Position = mvp * vec4(aPos, 1.0);\n"
    "}\n";

// Default fragment shader
static const char* default_fragment_shader_source = 
    "#version 330 core\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "\n"
    "out vec4 finalColor;\n"
    "\n"
    "void main() {\n"
    "    vec4 texelColor = texture(texture0, fragTexCoord);\n"
    "    finalColor = texelColor * colDiffuse * fragColor;\n"
    "}\n";

// Software renderer "shaders" (just function pointers)
typedef struct {
    void (*vertex_func)(const float* vertex, float* output);
    void (*fragment_func)(const float* input, float* output);
} SoftwareShaderProgram;

// =============================================================================
// RENDERER-SPECIFIC IMPLEMENTATIONS
// =============================================================================

#ifdef KRYON_RENDERER_RAYLIB
static uint32_t raylib_compile_shader(const char* source, KryylShaderType type) {
    int shader_type = (type == KRYON_SHADER_VERTEX) ? RL_VERTEX_SHADER : RL_FRAGMENT_SHADER;
    
    unsigned int shader = rlCompileShader(source, shader_type);
    if (shader == 0) {
        printf("Raylib: Failed to compile shader\n");
        return 0;
    }
    
    return shader;
}

static uint32_t raylib_create_program(uint32_t vertex_shader, uint32_t fragment_shader) {
    unsigned int program = rlLoadShaderProgram(vertex_shader, fragment_shader);
    if (program == 0) {
        printf("Raylib: Failed to create shader program\n");
        return 0;
    }
    
    return program;
}

static void raylib_destroy_shader(uint32_t shader_id) {
    rlUnloadShaderProgram(shader_id);
}

static bool raylib_use_shader(uint32_t program_id) {
    // Raylib handles this internally
    return true;
}

static int raylib_get_uniform_location(uint32_t program_id, const char* name) {
    return rlGetLocationUniform(program_id, name);
}

static void raylib_set_uniform_matrix4fv(int location, const float* matrix) {
    rlSetUniformMatrix(location, *(Matrix*)matrix);
}

static void raylib_set_uniform_1i(int location, int value) {
    rlSetUniform(location, &value, SHADER_UNIFORM_INT, 1);
}

static void raylib_set_uniform_1f(int location, float value) {
    rlSetUniform(location, &value, SHADER_UNIFORM_FLOAT, 1);
}

static void raylib_set_uniform_4f(int location, float x, float y, float z, float w) {
    float values[4] = {x, y, z, w};
    rlSetUniform(location, values, SHADER_UNIFORM_VEC4, 1);
}
#endif

#ifdef KRYON_RENDERER_SDL2
#ifdef SDL_VIDEO_OPENGL
static uint32_t sdl2_compile_shader(const char* source, KryonShaderType type) {
    GLenum shader_type = (type == KRYON_SHADER_VERTEX) ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
    
    GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    // Check compilation status
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        printf("SDL2/OpenGL: Shader compilation failed: %s\n", info_log);
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

static uint32_t sdl2_create_program(uint32_t vertex_shader, uint32_t fragment_shader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    
    // Check linking status
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, 512, NULL, info_log);
        printf("SDL2/OpenGL: Program linking failed: %s\n", info_log);
        glDeleteProgram(program);
        return 0;
    }
    
    return program;
}

static void sdl2_destroy_shader(uint32_t shader_id) {
    if (glIsProgram(shader_id)) {
        glDeleteProgram(shader_id);
    } else if (glIsShader(shader_id)) {
        glDeleteShader(shader_id);
    }
}

static bool sdl2_use_shader(uint32_t program_id) {
    glUseProgram(program_id);
    return glGetError() == GL_NO_ERROR;
}

static int sdl2_get_uniform_location(uint32_t program_id, const char* name) {
    return glGetUniformLocation(program_id, name);
}

static void sdl2_set_uniform_matrix4fv(int location, const float* matrix) {
    glUniformMatrix4fv(location, 1, GL_FALSE, matrix);
}

static void sdl2_set_uniform_1i(int location, int value) {
    glUniform1i(location, value);
}

static void sdl2_set_uniform_1f(int location, float value) {
    glUniform1f(location, value);
}

static void sdl2_set_uniform_4f(int location, float x, float y, float z, float w) {
    glUniform4f(location, x, y, z, w);
}
#endif
#endif

// Software renderer "shader" implementation
static void software_default_vertex_func(const float* vertex, float* output) {
    // Simple pass-through vertex "shader"
    output[0] = vertex[0]; // x
    output[1] = vertex[1]; // y
    output[2] = vertex[2]; // z
    output[3] = 1.0f;      // w
}

static void software_default_fragment_func(const float* input, float* output) {
    // Simple white color fragment "shader"
    output[0] = 1.0f; // r
    output[1] = 1.0f; // g
    output[2] = 1.0f; // b
    output[3] = 1.0f; // a
}

static uint32_t software_create_shader_program(void) {
    SoftwareShaderProgram* program = kryon_alloc(sizeof(SoftwareShaderProgram));
    if (!program) return 0;
    
    program->vertex_func = software_default_vertex_func;
    program->fragment_func = software_default_fragment_func;
    
    return (uint32_t)(uintptr_t)program;
}

static void software_destroy_shader(uint32_t program_id) {
    SoftwareShaderProgram* program = (SoftwareShaderProgram*)(uintptr_t)program_id;
    kryon_free(program);
}

// =============================================================================
// SHADER CACHE MANAGEMENT
// =============================================================================

static KryonShaderCache* find_cached_shader(uint32_t shader_id) {
    KryonShaderCache* cache = g_shader_compiler.cached_shaders;
    while (cache) {
        if (cache->id == shader_id) {
            return cache;
        }
        cache = cache->next;
    }
    return NULL;
}

static uint32_t add_to_cache(const char* name, KryonShaderType type, void* renderer_shader) {
    KryonShaderCache* cache = kryon_alloc(sizeof(KryonShaderCache));
    if (!cache) return 0;
    
    memset(cache, 0, sizeof(KryonShaderCache));
    
    cache->id = ++g_shader_compiler.next_shader_id;
    cache->type = type;
    cache->renderer_shader = renderer_shader;
    
    if (name) {
        cache->name = kryon_alloc(strlen(name) + 1);
        if (cache->name) {
            strcpy(cache->name, name);
        }
    }
    
    // Set up abstracted shader
    cache->kryon_shader.id = cache->id;
    cache->kryon_shader.type = type;
    cache->kryon_shader.renderer_data = renderer_shader;
    
    cache->next = g_shader_compiler.cached_shaders;
    g_shader_compiler.cached_shaders = cache;
    g_shader_compiler.shader_count++;
    
    return cache->id;
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_shader_compiler_init(void) {
    memset(&g_shader_compiler, 0, sizeof(g_shader_compiler));
    g_shader_compiler.next_shader_id = 1;
    
    // Create default shaders
    g_shader_compiler.default_vertex_shader = kryon_shader_compile(
        default_vertex_shader_source, KRYON_SHADER_VERTEX, "default_vertex"
    );
    
    g_shader_compiler.default_fragment_shader = kryon_shader_compile(
        default_fragment_shader_source, KRYON_SHADER_FRAGMENT, "default_fragment"
    );
    
    if (g_shader_compiler.default_vertex_shader && g_shader_compiler.default_fragment_shader) {
        g_shader_compiler.default_program = kryon_shader_create_program(
            g_shader_compiler.default_vertex_shader,
            g_shader_compiler.default_fragment_shader,
            "default_program"
        );
    }
    
    return g_shader_compiler.default_program != 0;
}

void kryon_shader_compiler_shutdown(void) {
    // Clean up all cached shaders
    KryonShaderCache* cache = g_shader_compiler.cached_shaders;
    while (cache) {
        KryonShaderCache* next = cache->next;
        
        // Destroy renderer-specific shader
        KryonRendererType renderer = kryon_renderer_get_type();
        switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
            case KRYON_RENDERER_RAYLIB:
                raylib_destroy_shader((uint32_t)(uintptr_t)cache->renderer_shader);
                break;
#endif
                
#ifdef KRYON_RENDERER_SDL2
            case KRYON_RENDERER_SDL2:
                sdl2_destroy_shader((uint32_t)(uintptr_t)cache->renderer_shader);
                break;
#endif
                
            default:
                software_destroy_shader((uint32_t)(uintptr_t)cache->renderer_shader);
                break;
        }
        
        kryon_free(cache->name);
        kryon_free(cache);
        cache = next;
    }
    
    memset(&g_shader_compiler, 0, sizeof(g_shader_compiler));
}

uint32_t kryon_shader_compile(const char* source, KryonShaderType type, const char* name) {
    if (!source) return 0;
    
    KryonRendererType renderer = kryon_renderer_get_type();
    uint32_t shader_id = 0;
    
    switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
        case KRYON_RENDERER_RAYLIB:
            shader_id = raylib_compile_shader(source, type);
            break;
#endif
            
#ifdef KRYON_RENDERER_SDL2
        case KRYON_RENDERER_SDL2:
            shader_id = sdl2_compile_shader(source, type);
            break;
#endif
            
        case KRYON_RENDERER_SOFTWARE:
        case KRYON_RENDERER_HTML:
        default:
            // Software renderer doesn't really compile shaders
            shader_id = g_shader_compiler.next_shader_id + 1;
            break;
    }
    
    if (shader_id == 0) return 0;
    
    return add_to_cache(name, type, (void*)(uintptr_t)shader_id);
}

uint32_t kryon_shader_create_program(uint32_t vertex_shader, uint32_t fragment_shader, const char* name) {
    if (vertex_shader == 0 || fragment_shader == 0) return 0;
    
    KryonShaderCache* vs_cache = find_cached_shader(vertex_shader);
    KryonShaderCache* fs_cache = find_cached_shader(fragment_shader);
    
    if (!vs_cache || !fs_cache) return 0;
    
    KryonRendererType renderer = kryon_renderer_get_type();
    uint32_t program_id = 0;
    
    switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
        case KRYON_RENDERER_RAYLIB:
            program_id = raylib_create_program(
                (uint32_t)(uintptr_t)vs_cache->renderer_shader,
                (uint32_t)(uintptr_t)fs_cache->renderer_shader
            );
            break;
#endif
            
#ifdef KRYON_RENDERER_SDL2
        case KRYON_RENDERER_SDL2:
            program_id = sdl2_create_program(
                (uint32_t)(uintptr_t)vs_cache->renderer_shader,
                (uint32_t)(uintptr_t)fs_cache->renderer_shader
            );
            break;
#endif
            
        case KRYON_RENDERER_SOFTWARE:
        case KRYON_RENDERER_HTML:
        default:
            program_id = software_create_shader_program();
            break;
    }
    
    if (program_id == 0) return 0;
    
    return add_to_cache(name, KRYON_SHADER_PROGRAM, (void*)(uintptr_t)program_id);
}

void kryon_shader_destroy(uint32_t shader_id) {
    if (shader_id == 0) return;
    
    KryonShaderCache* cache = find_cached_shader(shader_id);
    if (!cache) return;
    
    // Remove from cache
    KryonShaderCache* current = g_shader_compiler.cached_shaders;
    KryonShaderCache* prev = NULL;
    
    while (current) {
        if (current->id == shader_id) {
            if (prev) {
                prev->next = current->next;
            } else {
                g_shader_compiler.cached_shaders = current->next;
            }
            
            // Destroy renderer-specific shader
            KryonRendererType renderer = kryon_renderer_get_type();
            switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
                case KRYON_RENDERER_RAYLIB:
                    raylib_destroy_shader((uint32_t)(uintptr_t)current->renderer_shader);
                    break;
#endif
                    
#ifdef KRYON_RENDERER_SDL2
                case KRYON_RENDERER_SDL2:
                    sdl2_destroy_shader((uint32_t)(uintptr_t)current->renderer_shader);
                    break;
#endif
                    
                default:
                    software_destroy_shader((uint32_t)(uintptr_t)current->renderer_shader);
                    break;
            }
            
            kryon_free(current->name);
            kryon_free(current);
            g_shader_compiler.shader_count--;
            return;
        }
        prev = current;
        current = current->next;
    }
}

bool kryon_shader_use(uint32_t program_id) {
    if (program_id == 0) return false;
    
    KryonShaderCache* cache = find_cached_shader(program_id);
    if (!cache || cache->type != KRYON_SHADER_PROGRAM) return false;
    
    KryonRendererType renderer = kryon_renderer_get_type();
    
    switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
        case KRYON_RENDERER_RAYLIB:
            return raylib_use_shader((uint32_t)(uintptr_t)cache->renderer_shader);
#endif
            
#ifdef KRYON_RENDERER_SDL2
        case KRYON_RENDERER_SDL2:
            return sdl2_use_shader((uint32_t)(uintptr_t)cache->renderer_shader);
#endif
            
        default:
            return true; // Software renderer always "succeeds"
    }
}

int kryon_shader_get_uniform_location(uint32_t program_id, const char* name) {
    if (program_id == 0 || !name) return -1;
    
    KryonShaderCache* cache = find_cached_shader(program_id);
    if (!cache || cache->type != KRYON_SHADER_PROGRAM) return -1;
    
    KryonRendererType renderer = kryon_renderer_get_type();
    
    switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
        case KRYON_RENDERER_RAYLIB:
            return raylib_get_uniform_location((uint32_t)(uintptr_t)cache->renderer_shader, name);
#endif
            
#ifdef KRYON_RENDERER_SDL2
        case KRYON_RENDERER_SDL2:
            return sdl2_get_uniform_location((uint32_t)(uintptr_t)cache->renderer_shader, name);
#endif
            
        default:
            return 0; // Software renderer doesn't have uniforms
    }
}

void kryon_shader_set_uniform_matrix4fv(int location, const float* matrix) {
    if (location < 0 || !matrix) return;
    
    KryonRendererType renderer = kryon_renderer_get_type();
    
    switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
        case KRYON_RENDERER_RAYLIB:
            raylib_set_uniform_matrix4fv(location, matrix);
            break;
#endif
            
#ifdef KRYON_RENDERER_SDL2
        case KRYON_RENDERER_SDL2:
            sdl2_set_uniform_matrix4fv(location, matrix);
            break;
#endif
            
        default:
            break; // Software renderer doesn't support uniforms
    }
}

void kryon_shader_set_uniform_1i(int location, int value) {
    if (location < 0) return;
    
    KryonRendererType renderer = kryon_renderer_get_type();
    
    switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
        case KRYON_RENDERER_RAYLIB:
            raylib_set_uniform_1i(location, value);
            break;
#endif
            
#ifdef KRYON_RENDERER_SDL2
        case KRYON_RENDERER_SDL2:
            sdl2_set_uniform_1i(location, value);
            break;
#endif
            
        default:
            break;
    }
}

void kryon_shader_set_uniform_1f(int location, float value) {
    if (location < 0) return;
    
    KryonRendererType renderer = kryon_renderer_get_type();
    
    switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
        case KRYON_RENDERER_RAYLIB:
            raylib_set_uniform_1f(location, value);
            break;
#endif
            
#ifdef KRYON_RENDERER_SDL2
        case KRYON_RENDERER_SDL2:
            sdl2_set_uniform_1f(location, value);
            break;
#endif
            
        default:
            break;
    }
}

void kryon_shader_set_uniform_4f(int location, float x, float y, float z, float w) {
    if (location < 0) return;
    
    KryonRendererType renderer = kryon_renderer_get_type();
    
    switch (renderer) {
#ifdef KRYON_RENDERER_RAYLIB
        case KRYON_RENDERER_RAYLIB:
            raylib_set_uniform_4f(location, x, y, z, w);
            break;
#endif
            
#ifdef KRYON_RENDERER_SDL2
        case KRYON_RENDERER_SDL2:
            sdl2_set_uniform_4f(location, x, y, z, w);
            break;
#endif
            
        default:
            break;
    }
}

uint32_t kryon_shader_get_default_program(void) {
    return g_shader_compiler.default_program;
}

size_t kryon_shader_get_count(void) {
    return g_shader_compiler.shader_count;
}