/**
 * @file run_command.c
 * @brief Kryon Run Command Implementation
 */

#include "internal/memory.h"
#include "internal/runtime.h"
#include "internal/renderer_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

// Global for debug mode
static bool g_debug_mode = false;

// Forward declarations for proper renderer integration
static bool setup_renderer(KryonRuntime* runtime, const char* renderer_name, bool debug);
static KryonRenderer* create_raylib_renderer(KryonRuntime* runtime, bool debug);

// Forward declaration for runtime event callback function (defined in runtime.c)
extern void runtime_receive_input_event(const KryonEvent* event, void* userData);

int run_command(int argc, char *argv[]) {
    const char *krb_file_path = NULL;
    const char *renderer = "text";  // Default to text renderer
    bool debug = false;
    
    // Parse command line options
    static struct option long_options[] = {
        {"renderer", required_argument, 0, 'r'},
        {"debug", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "r:dh", long_options, NULL)) != -1) {
        switch (c) {
            case 'r':
                renderer = optarg;
                break;
            case 'd':
                debug = true;
                break;
            case 'h':
                printf("Usage: kryon run <file.krb> [options]\n");
                printf("Options:\n");
                printf("  -r, --renderer <name>  Renderer to use (text, raylib, debug-raylib)\n");
                printf("  -d, --debug           Enable debug output\n");
                printf("  -h, --help            Show this help\n");
                return 0;
            default:
                return 1;
        }
    }
    
    if (optind >= argc) {
        fprintf(stderr, "Error: No KRB file specified\n");
        return 1;
    }
    
    krb_file_path = argv[optind];
    g_debug_mode = debug;
    
    // Initialize memory manager if needed
    if (!g_kryon_memory_manager) {
        // Create default memory configuration
        KryonMemoryConfig config = {
            .initial_heap_size = 16 * 1024 * 1024,    // 16MB
            .max_heap_size = 256 * 1024 * 1024,       // 256MB  
            .enable_leak_detection = true,
            .enable_bounds_checking = false,
            .enable_statistics = false,
            .use_system_malloc = true,
            .large_alloc_threshold = 64 * 1024        // 64KB
        };
        
        g_kryon_memory_manager = kryon_memory_init(&config);
        if (!g_kryon_memory_manager) {
            fprintf(stderr, "Error: Failed to initialize memory manager\n");
            return 1;
        }
    }
    
    if (debug) {
        printf("🐛 Debug: Creating runtime system\n");
    }
    
    // Create runtime system
    KryonRuntime* runtime = kryon_runtime_create(NULL);
    if (!runtime) {
        fprintf(stderr, "❌ Failed to create runtime system\n");
        return 1;
    }
    
    if (debug) {
        printf("🐛 Debug: Loading KRB file: %s\n", krb_file_path);
    }
    
    // Load KRB file into runtime
    if (!kryon_runtime_load_file(runtime, krb_file_path)) {
        fprintf(stderr, "❌ Failed to load KRB file: %s\n", krb_file_path);
        kryon_runtime_destroy(runtime);
        return 1;
    }
    
    if (debug) {
        printf("🐛 Debug: KRB file loaded successfully into runtime\n");
    }
    
    // Setup renderer
    if (!setup_renderer(runtime, renderer, debug)) {
        fprintf(stderr, "❌ Failed to setup renderer: %s\n", renderer);
        kryon_runtime_destroy(runtime);
        return 1;
    }
    
    if (debug) {
        printf("🐛 Debug: Renderer setup successfully: %s\n", renderer);
    }
    
    // Start the runtime
    if (!kryon_runtime_start(runtime)) {
        fprintf(stderr, "❌ Failed to start runtime\n");
        kryon_runtime_destroy(runtime);
        return 1;
    }
    
    if (debug) {
        printf("🐛 Debug: Runtime started successfully\n");
    }
    
    // Execute rendering loop
    printf("🎬 Starting render loop with %s\n", renderer);
    
    // Simple render loop for now - in future this should be event-driven  
    bool running = true;
    int frame_count = 0;
    double last_time = 0.0; // TODO: Get actual time
    
    while (running) { // Run until window is closed
        double current_time = frame_count * 0.016666; // Approximate time for now
        double delta_time = current_time - last_time;
        last_time = current_time;
        
        // Update runtime (process events, update state)
        kryon_runtime_update(runtime, delta_time);
        
        // Render frame
        if (!kryon_runtime_render(runtime)) {
            fprintf(stderr, "❌ Rendering failed at frame %d\n", frame_count);
            break;
        }
        frame_count++;
        
        // Exit after a reasonable number of frames for text renderer or testing
        if (strcmp(renderer, "text") == 0 && frame_count >= 60) {
            printf("📝 Text renderer: completed %d frames\n", frame_count);
            break;
        }
        
        // For raylib, check if window should close
        if (strcmp(renderer, "raylib") == 0) {
            // The raylib renderer should set runtime->is_running = false when window closes
            if (!runtime->is_running) {
                printf("🔲 Window closed by user\n");
                break;
            }
        }
        
        // Basic frame limiting (should be handled by renderer)
        usleep(16666); // ~60fps
    }
    
    printf("✅ Rendering completed successfully with %s (%d frames)\n", renderer, frame_count);
    
    // Cleanup
    kryon_runtime_destroy(runtime);
    
    return 0;
}

// Setup renderer for the runtime using internal interface
static bool setup_renderer(KryonRuntime* runtime, const char* renderer_name, bool debug) {
    if (!runtime || !renderer_name) {
        return false;
    }
    
    KryonRenderer* renderer = NULL;
    
    // Create renderer based on name
    if (strcmp(renderer_name, "raylib") == 0) {
#ifdef KRYON_RENDERER_RAYLIB
        renderer = create_raylib_renderer(runtime, debug);
#else
        fprintf(stderr, "❌ RAYLIB NOT AVAILABLE\n");
        fprintf(stderr, "❌ This build was compiled without raylib support\n");
        fprintf(stderr, "💡 Available renderers: text\n");
        fprintf(stderr, "💡 To use raylib, install raylib-dev and recompile\n");
        return false;
#endif
    } else if (strcmp(renderer_name, "text") == 0) {
        // For text mode, we don't need a real renderer
        if (debug) {
            printf("🐛 Debug: Using text mode (no renderer needed)\n");
        }
        return true;
    } else {
        fprintf(stderr, "❌ Unknown renderer: %s\n", renderer_name);
        fprintf(stderr, "Available renderers: raylib, text\n");
        return false;
    }
    
    if (!renderer) {
        fprintf(stderr, "❌ Failed to create %s renderer\n", renderer_name);
        return false;
    }
    
    // Store renderer in runtime
    runtime->renderer = (void*)renderer;
    
    if (debug) {
        printf("🐛 Debug: Renderer '%s' setup successfully\n", renderer_name);
    }
    
    return true;
}

// Create raylib renderer using internal interface
static KryonRenderer* create_raylib_renderer(KryonRuntime* runtime, bool debug) {
#ifdef KRYON_RENDERER_RAYLIB
    if (debug) {
        printf("🐛 Debug: Creating raylib renderer\n");
    }
    
    // Create surface with default values, then override from App element metadata
    typedef struct {
        int width;
        int height;
        const char* title;
        bool fullscreen;
        int target_fps;
    } RaylibSurface;
    
    RaylibSurface surface = {
        .width = 800,
        .height = 600,
        .title = "Kryon Application",
        .fullscreen = false,
        .target_fps = 60
    };
    
    // Extract surface properties from App element metadata if available
    if (runtime && runtime->root) {
        KryonElement* app_element = runtime->root;
        
        printf("🔍 DEBUG: App element pointer: %p\n", (void*)app_element);
        printf("🔍 DEBUG: App element type: %u (%s)\n", app_element->type, app_element->type_name ? app_element->type_name : "NULL");
        printf("🔍 DEBUG: App element properties pointer: %p\n", (void*)app_element->properties);
        printf("🔍 DEBUG: App element has %zu properties:\n", app_element->property_count);
        for (size_t j = 0; j < app_element->property_count; j++) {
            if (app_element->properties[j] && app_element->properties[j]->name) {
                printf("  - %s (type=%d)\n", app_element->properties[j]->name, app_element->properties[j]->type);
            }
        }
        
        // Look for title property in App element
        for (size_t i = 0; i < app_element->property_count; i++) {
            if (app_element->properties[i] && app_element->properties[i]->name) {
                if (strcmp(app_element->properties[i]->name, "title") == 0 && 
                    app_element->properties[i]->value.string_value) {
                    surface.title = app_element->properties[i]->value.string_value;
                    printf("🐛 Debug: Using title from App metadata: %s\n", surface.title);
                }
                else if (strcmp(app_element->properties[i]->name, "windowWidth") == 0 ||
                         strcmp(app_element->properties[i]->name, "winWidth") == 0) {
                    surface.width = (int)app_element->properties[i]->value.float_value;
                    if (debug) {
                        printf("🐛 Debug: Using windowWidth from App metadata: %d\n", surface.width);
                    }
                }
                else if (strcmp(app_element->properties[i]->name, "windowHeight") == 0 ||
                         strcmp(app_element->properties[i]->name, "winHeight") == 0) {
                    surface.height = (int)app_element->properties[i]->value.float_value;
                    if (debug) {
                        printf("🐛 Debug: Using windowHeight from App metadata: %d\n", surface.height);
                    }
                }
                else if (strcmp(app_element->properties[i]->name, "windowTitle") == 0 ||
                         strcmp(app_element->properties[i]->name, "winTitle") == 0) {
                    if (app_element->properties[i]->value.string_value) {
                        surface.title = app_element->properties[i]->value.string_value;
                        if (debug) {
                            printf("🐛 Debug: Using windowTitle from App metadata: %s\n", surface.title);
                        }
                    }
                }
            }
        }
    }
    
    if (debug) {
        printf("🐛 Debug: Creating raylib surface: %dx%d '%s'\n", 
               surface.width, surface.height, surface.title);
    }
    
    // Create renderer configuration with event callback
    KryonRendererConfig config = {
        .event_callback = runtime_receive_input_event,
        .callback_data = runtime,
        .platform_context = &surface
    };
    
    KryonRenderer* renderer = kryon_raylib_renderer_create(&config);
    
    if (!renderer) {
        fprintf(stderr, "❌ Failed to create raylib renderer\n");
        if (debug) {
            printf("🐛 Debug: Raylib renderer creation failed\n");
        }
        return NULL;
    }
    
    if (debug) {
        printf("🐛 Debug: Raylib renderer created successfully\n");
    }
    
    return renderer;
#else
    fprintf(stderr, "❌ RAYLIB NOT AVAILABLE\n");
    fprintf(stderr, "❌ This build was compiled without raylib support\n");
    fprintf(stderr, "💡 Available renderers: text\n");
    fprintf(stderr, "💡 To use raylib, install raylib-dev and recompile\n");
    
    return NULL;
#endif
}