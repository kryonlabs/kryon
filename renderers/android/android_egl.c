#include "android_renderer.h"
#include "android_renderer_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <android/log.h>

#define LOG_TAG "KryonRenderer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) fprintf(stdout, "[INFO] " __VA_ARGS__)
#define LOGE(...) fprintf(stderr, "[ERROR] " __VA_ARGS__)
#define LOGD(...) fprintf(stdout, "[DEBUG] " __VA_ARGS__)
#endif

// ============================================================================
// EGL Context Initialization
// ============================================================================

#ifdef __ANDROID__
static bool check_egl_error(const char* operation) {
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        LOGE("%s failed: EGL error 0x%x\n", operation, error);
        return false;
    }
    return true;
}

bool android_egl_init(AndroidRenderer* renderer, ANativeWindow* window) {
    if (!renderer || !window) {
        LOGE("Invalid parameters for EGL initialization\n");
        return false;
    }

    LOGI("Initializing EGL context...\n");

    // Get EGL display
    renderer->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (renderer->egl_display == EGL_NO_DISPLAY) {
        LOGE("Failed to get EGL display\n");
        return false;
    }

    // Initialize EGL
    EGLint major, minor;
    if (!eglInitialize(renderer->egl_display, &major, &minor)) {
        LOGE("Failed to initialize EGL\n");
        check_egl_error("eglInitialize");
        return false;
    }

    LOGI("EGL initialized: version %d.%d\n", major, minor);

    // Configure EGL attributes for OpenGL ES 3.0
    const EGLint config_attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_STENCIL_SIZE, 0,
        EGL_SAMPLE_BUFFERS, 0,
        EGL_SAMPLES, 0,
        EGL_NONE
    };

    // Choose EGL configuration
    EGLint num_configs;
    if (!eglChooseConfig(renderer->egl_display, config_attribs,
                         &renderer->egl_config, 1, &num_configs)) {
        LOGE("Failed to choose EGL config\n");
        check_egl_error("eglChooseConfig");
        return false;
    }

    if (num_configs == 0) {
        LOGE("No suitable EGL config found\n");
        return false;
    }

    LOGI("EGL config chosen: %d configs available\n", num_configs);

    // Get config attributes
    EGLint r, g, b, a, d;
    eglGetConfigAttrib(renderer->egl_display, renderer->egl_config, EGL_RED_SIZE, &r);
    eglGetConfigAttrib(renderer->egl_display, renderer->egl_config, EGL_GREEN_SIZE, &g);
    eglGetConfigAttrib(renderer->egl_display, renderer->egl_config, EGL_BLUE_SIZE, &b);
    eglGetConfigAttrib(renderer->egl_display, renderer->egl_config, EGL_ALPHA_SIZE, &a);
    eglGetConfigAttrib(renderer->egl_display, renderer->egl_config, EGL_DEPTH_SIZE, &d);
    LOGI("EGL config: R%dG%dB%dA%d D%d\n", r, g, b, a, d);

    // Set native window format
    EGLint format;
    eglGetConfigAttrib(renderer->egl_display, renderer->egl_config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(window, 0, 0, format);

    // Create window surface
    renderer->egl_surface = eglCreateWindowSurface(renderer->egl_display,
                                                    renderer->egl_config,
                                                    window, NULL);
    if (renderer->egl_surface == EGL_NO_SURFACE) {
        LOGE("Failed to create EGL surface\n");
        check_egl_error("eglCreateWindowSurface");
        return false;
    }

    LOGI("EGL surface created\n");

    // Create OpenGL ES 3.0 context
    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    renderer->egl_context = eglCreateContext(renderer->egl_display,
                                              renderer->egl_config,
                                              EGL_NO_CONTEXT,
                                              context_attribs);
    if (renderer->egl_context == EGL_NO_CONTEXT) {
        LOGE("Failed to create EGL context\n");
        check_egl_error("eglCreateContext");
        return false;
    }

    LOGI("EGL context created (OpenGL ES 3.0)\n");

    // Make context current
    if (!eglMakeCurrent(renderer->egl_display,
                        renderer->egl_surface,
                        renderer->egl_surface,
                        renderer->egl_context)) {
        LOGE("Failed to make EGL context current\n");
        check_egl_error("eglMakeCurrent");
        return false;
    }

    LOGI("EGL context made current\n");

    // Query surface dimensions
    EGLint width, height;
    eglQuerySurface(renderer->egl_display, renderer->egl_surface, EGL_WIDTH, &width);
    eglQuerySurface(renderer->egl_display, renderer->egl_surface, EGL_HEIGHT, &height);
    renderer->window_width = width;
    renderer->window_height = height;

    LOGI("Surface dimensions: %dx%d\n", width, height);

    // Set VSync (swap interval)
    if (renderer->config.vsync_enabled) {
        eglSwapInterval(renderer->egl_display, 1);
        LOGI("VSync enabled\n");
    } else {
        eglSwapInterval(renderer->egl_display, 0);
        LOGI("VSync disabled\n");
    }

    // Query OpenGL ES information
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    const char* renderer_name = (const char*)glGetString(GL_RENDERER);
    const char* version = (const char*)glGetString(GL_VERSION);
    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);

    LOGI("OpenGL ES Vendor: %s\n", vendor);
    LOGI("OpenGL ES Renderer: %s\n", renderer_name);
    LOGI("OpenGL ES Version: %s\n", version);

    // Store capabilities
    renderer->capabilities.vendor = vendor;
    renderer->capabilities.renderer = renderer_name;
    renderer->capabilities.version = version;
    renderer->capabilities.extensions = extensions;

    // Query OpenGL ES limits
    GLint max_texture_size, max_vertex_attribs, max_texture_units, max_renderbuffer_size;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attribs);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_texture_units);
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer_size);

    renderer->capabilities.max_texture_size = max_texture_size;
    renderer->capabilities.max_vertex_attribs = max_vertex_attribs;
    renderer->capabilities.max_texture_units = max_texture_units;
    renderer->capabilities.max_renderbuffer_size = max_renderbuffer_size;

    LOGI("OpenGL ES Capabilities:\n");
    LOGI("  Max texture size: %d\n", max_texture_size);
    LOGI("  Max vertex attribs: %d\n", max_vertex_attribs);
    LOGI("  Max texture units: %d\n", max_texture_units);
    LOGI("  Max renderbuffer size: %d\n", max_renderbuffer_size);

    // Detect OpenGL ES version
    renderer->capabilities.has_gles3 = true;
    renderer->capabilities.has_gles31 = (strstr(version, "3.1") != NULL || strstr(version, "3.2") != NULL);
    renderer->capabilities.has_gles32 = (strstr(version, "3.2") != NULL);

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Disable depth test (2D rendering)
    glDisable(GL_DEPTH_TEST);

    // Disable face culling
    glDisable(GL_CULL_FACE);

    // Set clear color
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    LOGI("EGL initialization complete\n");
    return true;
}

void android_egl_shutdown(AndroidRenderer* renderer) {
    if (!renderer) return;

    LOGI("Shutting down EGL...\n");

    // Make no context current
    if (renderer->egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(renderer->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    // Destroy context
    if (renderer->egl_context != EGL_NO_CONTEXT) {
        eglDestroyContext(renderer->egl_display, renderer->egl_context);
        renderer->egl_context = EGL_NO_CONTEXT;
        LOGI("EGL context destroyed\n");
    }

    // Destroy surface
    if (renderer->egl_surface != EGL_NO_SURFACE) {
        eglDestroySurface(renderer->egl_display, renderer->egl_surface);
        renderer->egl_surface = EGL_NO_SURFACE;
        LOGI("EGL surface destroyed\n");
    }

    // Terminate EGL
    if (renderer->egl_display != EGL_NO_DISPLAY) {
        eglTerminate(renderer->egl_display);
        renderer->egl_display = EGL_NO_DISPLAY;
        LOGI("EGL terminated\n");
    }

    LOGI("EGL shutdown complete\n");
}

bool android_egl_swap_buffers(AndroidRenderer* renderer) {
    if (!renderer || renderer->egl_display == EGL_NO_DISPLAY ||
        renderer->egl_surface == EGL_NO_SURFACE) {
        return false;
    }

    if (!eglSwapBuffers(renderer->egl_display, renderer->egl_surface)) {
        LOGE("eglSwapBuffers failed\n");
        check_egl_error("eglSwapBuffers");
        return false;
    }

    return true;
}

bool android_egl_resize(AndroidRenderer* renderer, int width, int height) {
    if (!renderer) return false;

    LOGI("EGL surface resize: %dx%d\n", width, height);

    renderer->window_width = width;
    renderer->window_height = height;

    // Update viewport
    glViewport(0, 0, width, height);

    // Recreate projection matrix
    android_renderer_update_projection(renderer);

    return true;
}

#else // !__ANDROID__

// Stub implementations for non-Android builds
bool android_egl_init(AndroidRenderer* renderer, void* window) {
    (void)renderer;
    (void)window;
    LOGI("EGL initialization (stub)\n");
    return true;
}

void android_egl_shutdown(AndroidRenderer* renderer) {
    (void)renderer;
    LOGI("EGL shutdown (stub)\n");
}

bool android_egl_swap_buffers(AndroidRenderer* renderer) {
    (void)renderer;
    return true;
}

bool android_egl_resize(AndroidRenderer* renderer, int width, int height) {
    if (!renderer) return false;
    renderer->window_width = width;
    renderer->window_height = height;
    return true;
}

#endif // __ANDROID__
