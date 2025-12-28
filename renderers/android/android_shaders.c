#include "android_renderer_internal.h"
#include "android_shaders.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "KryonShaders"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) fprintf(stdout, "[INFO] " __VA_ARGS__)
#define LOGE(...) fprintf(stderr, "[ERROR] " __VA_ARGS__)
#endif

// ============================================================================
// Shader Compilation
// ============================================================================

#ifdef __ANDROID__

static bool check_shader_compile_status(GLuint shader, const char* name) {
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status != GL_TRUE) {
        GLint log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

        if (log_length > 0) {
            char* log = (char*)malloc(log_length);
            glGetShaderInfoLog(shader, log_length, NULL, log);
            LOGE("%s shader compilation failed:\n%s\n", name, log);
            free(log);
        }

        return false;
    }

    return true;
}

static bool check_program_link_status(GLuint program) {
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (status != GL_TRUE) {
        GLint log_length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

        if (log_length > 0) {
            char* log = (char*)malloc(log_length);
            glGetProgramInfoLog(program, log_length, NULL, log);
            LOGE("Shader program linking failed:\n%s\n", log);
            free(log);
        }

        return false;
    }

    return true;
}

bool android_shader_compile(ShaderProgram* program,
                             const char* vertex_src,
                             const char* fragment_src) {
    if (!program || !vertex_src || !fragment_src) {
        LOGE("Invalid parameters for shader compilation\n");
        return false;
    }

    // Compile vertex shader
    program->vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(program->vertex_shader, 1, &vertex_src, NULL);
    glCompileShader(program->vertex_shader);

    if (!check_shader_compile_status(program->vertex_shader, "Vertex")) {
        glDeleteShader(program->vertex_shader);
        return false;
    }

    // Compile fragment shader
    program->fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(program->fragment_shader, 1, &fragment_src, NULL);
    glCompileShader(program->fragment_shader);

    if (!check_shader_compile_status(program->fragment_shader, "Fragment")) {
        glDeleteShader(program->vertex_shader);
        glDeleteShader(program->fragment_shader);
        return false;
    }

    // Link program
    program->program = glCreateProgram();
    glAttachShader(program->program, program->vertex_shader);
    glAttachShader(program->program, program->fragment_shader);
    glLinkProgram(program->program);

    if (!check_program_link_status(program->program)) {
        glDeleteShader(program->vertex_shader);
        glDeleteShader(program->fragment_shader);
        glDeleteProgram(program->program);
        return false;
    }

    // Get uniform locations
    program->u_projection = glGetUniformLocation(program->program, "uProjection");
    program->u_texture = glGetUniformLocation(program->program, "uTexture");
    program->u_color_start = glGetUniformLocation(program->program, "uColorStart");
    program->u_color_end = glGetUniformLocation(program->program, "uColorEnd");
    program->u_size = glGetUniformLocation(program->program, "uSize");
    program->u_radius = glGetUniformLocation(program->program, "uRadius");
    program->u_shadow_offset = glGetUniformLocation(program->program, "uShadowOffset");
    program->u_shadow_blur = glGetUniformLocation(program->program, "uShadowBlur");
    program->u_shadow_color = glGetUniformLocation(program->program, "uShadowColor");
    program->u_box_size = glGetUniformLocation(program->program, "uBoxSize");
    program->u_box_radius = glGetUniformLocation(program->program, "uBoxRadius");

    program->compiled = true;

    return true;
}

static void cleanup_shader_program(ShaderProgram* program) {
    if (!program || !program->compiled) return;

    if (program->program) {
        glDeleteProgram(program->program);
        program->program = 0;
    }

    if (program->vertex_shader) {
        glDeleteShader(program->vertex_shader);
        program->vertex_shader = 0;
    }

    if (program->fragment_shader) {
        glDeleteShader(program->fragment_shader);
        program->fragment_shader = 0;
    }

    program->compiled = false;
}

bool android_shader_init_all(AndroidRenderer* renderer) {
    if (!renderer) return false;

    LOGI("Initializing shaders...\n");

    bool success = true;

    // Compile each shader program
    for (int i = 0; i < SHADER_PROGRAM_COUNT; i++) {
        const char* vertex_src = get_vertex_shader_source((ShaderProgramType)i);
        const char* fragment_src = get_fragment_shader_source((ShaderProgramType)i);

        if (!vertex_src || !fragment_src) {
            LOGE("Missing shader source for program %d\n", i);
            continue;
        }

        LOGI("Compiling shader program %d...\n", i);

        if (!android_shader_compile(&renderer->shader_programs[i], vertex_src, fragment_src)) {
            LOGE("Failed to compile shader program %d\n", i);
            success = false;
            break;
        }

        LOGI("Shader program %d compiled successfully\n", i);
    }

    if (success) {
        renderer->shaders_initialized = true;
        LOGI("All shaders initialized successfully\n");
    } else {
        // Cleanup on failure
        android_shader_cleanup_all(renderer);
        LOGE("Shader initialization failed\n");
    }

    return success;
}

void android_shader_cleanup_all(AndroidRenderer* renderer) {
    if (!renderer) return;

    LOGI("Cleaning up shaders...\n");

    for (int i = 0; i < SHADER_PROGRAM_COUNT; i++) {
        cleanup_shader_program(&renderer->shader_programs[i]);
    }

    renderer->shaders_initialized = false;
    LOGI("Shaders cleaned up\n");
}

void android_shader_use(AndroidRenderer* renderer, ShaderProgramType type) {
    if (!renderer || !renderer->shaders_initialized) return;

    if (type >= SHADER_PROGRAM_COUNT) {
        LOGE("Invalid shader program type: %d\n", type);
        return;
    }

    if (renderer->current_shader == type) {
        return;  // Already using this shader
    }

    ShaderProgram* program = &renderer->shader_programs[type];
    if (!program->compiled) {
        LOGE("Shader program %d not compiled\n", type);
        return;
    }

    glUseProgram(program->program);

    // Set projection matrix (common to all shaders)
    if (program->u_projection >= 0) {
        glUniformMatrix4fv(program->u_projection, 1, GL_FALSE, renderer->projection_matrix);
    }

    renderer->current_shader = type;
    renderer->stats.shader_switches++;
}

#else // !__ANDROID__

// Stub implementations for non-Android builds
bool android_shader_compile(ShaderProgram* program,
                             const char* vertex_src,
                             const char* fragment_src) {
    (void)program;
    (void)vertex_src;
    (void)fragment_src;
    return true;
}

bool android_shader_init_all(AndroidRenderer* renderer) {
    if (!renderer) return false;
    renderer->shaders_initialized = true;
    LOGI("Shaders initialized (stub)\n");
    return true;
}

void android_shader_cleanup_all(AndroidRenderer* renderer) {
    if (!renderer) return;
    renderer->shaders_initialized = false;
    LOGI("Shaders cleaned up (stub)\n");
}

void android_shader_use(AndroidRenderer* renderer, ShaderProgramType type) {
    if (!renderer) return;

    // Only switch if different shader
    if (renderer->current_shader == type) {
        return;
    }

    renderer->current_shader = type;

    // Actually activate the shader program in OpenGL
    ShaderProgram* program = &renderer->shader_programs[type];
    glUseProgram(program->program);

    // Set projection matrix uniform (required for all shaders)
    if (program->u_projection >= 0) {
        glUniformMatrix4fv(program->u_projection, 1, GL_FALSE, renderer->projection_matrix);
    }

    renderer->stats.shader_switches++;
}

#endif // __ANDROID__
