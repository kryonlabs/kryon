/**

 * @file run_command.c
 * @brief Kryon Run Command Implementation
 */
#include "lib9.h"


#include "memory.h"
#include "runtime.h"
#include "renderer_interface.h"
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "error.h"
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>

// Global for debug mode
static bool g_debug_mode = false;

// Forward declarations for proper renderer integration
static bool setup_renderer(KryonRuntime* runtime, const char* renderer_name, bool debug, const char* output_dir);
static KryonRenderer* create_raylib_renderer(KryonRuntime* runtime, bool debug);
static KryonRenderer* create_sdl2_renderer(KryonRuntime* runtime, bool debug);
static KryonRenderer* create_web_renderer(KryonRuntime* runtime, bool debug, const char* output_dir);

// Forward declaration for runtime event callback function (defined in runtime.c)
extern void runtime_receive_input_event(const KryonEvent* event, void* userData);

// Forward declaration for compile_command (from compile_command.c)
extern KryonResult compile_command(int argc, char *argv[]);

/**
 * @brief Get file modification time
 * @return 0 on failure, modification time on success
 */
static time_t get_file_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return st.st_mtime;
}

/**
 * @brief Compute a simple file identifier (based on path and size)
 * @return String (must be freed by caller) or NULL on failure
 */
static char* compute_file_id(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return NULL;
    }

    // Use filename + size + mtime as cache key
    const char *filename = strrchr(path, '/');
    if (!filename) filename = path;
    else filename++;

    // Allocate enough space for the ID
    char *file_id = malloc(256);
    if (!file_id) {
        return NULL;
    }

    // Create a simple ID: filename_size_mtime
    snprint(file_id, 256, "%s_%ld_%ld",
             filename,
             (long)st.st_size,
             (long)st.st_mtime);

    // Sanitize the ID (replace problematic characters)
    for (char *p = file_id; *p; p++) {
        if (*p == '/' || *p == ' ') *p = '_';
    }

    return file_id;
}

/**
 * @brief Compile a .kry file to a cached .krb file
 * @param kry_path Path to the .kry source file
 * @return Path to the compiled .krb file (must be freed by caller) or NULL on failure
 */
static char* compile_kry_to_cached_krb(const char *kry_path) {
    print("🔨 Compiling %s...\n", kry_path);

    // Convert to absolute path to avoid issues with working directory
    char *real_kry_path = realpath(kry_path, NULL);
    if (!real_kry_path) {
        fprint(2, "Error: Cannot resolve path: %s\n", kry_path);
        return NULL;
    }

    // Compute file ID for caching
    char *file_id = compute_file_id(real_kry_path);
    if (!file_id) {
        fprint(2, "Error: Failed to compute file ID\n");
        return NULL;
    }

    // Create cache directory: ~/.cache/kryon/
    char cache_dir[PATH_MAX];
    const char *home = getenv("HOME");
    if (!home) {
        fprint(2, "Error: HOME environment variable not set\n");
        free(file_id);
        return NULL;
    }

    snprint(cache_dir, sizeof(cache_dir), "%s/.cache/kryon", home);

    // Create directory if it doesn't exist
    struct stat st;
    if (stat(cache_dir, &st) != 0) {
        if (mkdir(cache_dir, 0755) != 0 && errno != EEXIST) {
            fprint(2, "Error: Failed to create cache directory %s\n", cache_dir);
            free(file_id);
            return NULL;
        }
    }

    // Path to cached .krb file
    char *cached_krb_path = malloc(PATH_MAX);
    if (!cached_krb_path) {
        free(file_id);
        return NULL;
    }

    snprint(cached_krb_path, PATH_MAX, "%s/%s.krb", cache_dir, file_id);
    free(file_id);

    // Check if we need to recompile
    time_t kry_mtime = get_file_mtime(kry_path);
    time_t krb_mtime = get_file_mtime(cached_krb_path);

    if (krb_mtime > 0 && krb_mtime > kry_mtime) {
        // Cache is up to date
        print("✅ Using cached compilation: %s\n", cached_krb_path);
        return cached_krb_path;
    }

    // Need to compile - use compile_command with constructed arguments
    // compile_command expects: argv[0] = "compile", argv[1] = input, argv[2] = "-o", argv[3] = output
    char *compile_argv[] = {
        "compile",
        real_kry_path,
        "-o",
        cached_krb_path,
        NULL
    };
    int compile_argc = 4;

    // Reset getopt state before calling compile_command
    optind = 0;
    optarg = NULL;
    optopt = 0;

    // Call the compile command
    KryonResult result = compile_command(compile_argc, compile_argv);
    free(real_kry_path);

    if (result != KRYON_SUCCESS) {
        fprint(2, "❌ Compilation failed (error code: %d)\n", result);
        free(cached_krb_path);
        return NULL;
    }

    print("✅ Compilation complete: %s\n", cached_krb_path);
    return cached_krb_path;
}

int run_command(int argc, char *argv[]) {
    const char *krb_file_path = NULL;
    const char *renderer = "text";  // Default to text renderer
    const char *output_dir = "./web_output"; // Default web output directory
    bool debug = false;

    // Check for KRYON_RENDERER environment variable
    const char *env_renderer = getenv("KRYON_RENDERER");
    if (env_renderer) {
        renderer = env_renderer;
    }

    // Parse command line options
    static struct option long_options[] = {
        {"renderer", required_argument, 0, 'r'},
        {"output", required_argument, 0, 'o'},
        {"debug", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "r:o:dh", long_options, NULL)) != -1) {
        switch (c) {
            case 'r':
                renderer = optarg;
                break;
            case 'o':
                output_dir = optarg;
                break;
            case 'd':
                debug = true;
                break;
            case 'h':
                print("Usage: kryon run <file.krb> [options]\n");
                print("Options:\n");
                print("  -r, --renderer <name>  Renderer to use (text, raylib, web)\n");
                print("  -o, --output <dir>     Output directory for web renderer\n");
                print("  -d, --debug            Enable debug output\n");
                print("  -h, --help             Show this help\n");
                return 0;
            default:
                return 1;
        }
    }
    
    if (optind >= argc) {
        fprint(2, "Error: No KRB file specified\n");
        return 1;
    }

    krb_file_path = argv[optind];

    // Auto-compile .kry files
    char *temp_krb = NULL;
    if (strstr(krb_file_path, ".kry") != NULL) {
        temp_krb = compile_kry_to_cached_krb(krb_file_path);
        if (!temp_krb) {
            fprint(2, "❌ Failed to compile %s\n", krb_file_path);
            return 1;
        }
        krb_file_path = temp_krb;
    }

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
            fprint(2, "Error: Failed to initialize memory manager\n");
            return 1;
        }
    }
    
    if (debug) {
        print("🐛 Debug: Creating runtime system\n");
    }
    
    // Create runtime system
    KryonRuntime* runtime = kryon_runtime_create(NULL);
    if (!runtime) {
        fprint(2, "❌ Failed to create runtime system\n");
        return 1;
    }
    
    if (debug) {
        print("🐛 Debug: Loading KRB file: %s\n", krb_file_path);
    }
    
    // Save current working directory
    char original_cwd[PATH_MAX];
    if (getcwd(original_cwd, sizeof(original_cwd)) == NULL) {
        fprint(2, "❌ Failed to get current working directory\n");
        kryon_runtime_destroy(runtime);
        return 1;
    }
    
    // Extract directory from krb file path and change to it
    char *krb_path_copy = strdup(krb_file_path);
    char *krb_dir = dirname(krb_path_copy);
    char *krb_filename_copy = strdup(krb_file_path);
    char *krb_filename = basename(krb_filename_copy);
    
    if (debug) {
        print("🐛 Debug: Changing to directory: %s\n", krb_dir);
        print("🐛 Debug: Loading filename: %s\n", krb_filename);
    }
    
    // Change to the KRB file's directory
    if (chdir(krb_dir) != 0) {
        fprint(2, "❌ Failed to change to directory: %s\n", krb_dir);
        free(krb_path_copy);
        free(krb_filename_copy);
        kryon_runtime_destroy(runtime);
        return 1;
    }
    
    // Load KRB file into runtime (now using just the filename since we're in the right directory)
    if (!kryon_runtime_load_file(runtime, krb_filename)) {
        fprint(2, "❌ Failed to load KRB file: %s\n", krb_filename);
        // Restore original directory before cleanup
        chdir(original_cwd);
        free(krb_path_copy);
        free(krb_filename_copy);
        kryon_runtime_destroy(runtime);
        return 1;
    }
    
    // Override the runtime's current file path with the original full path for navigation
    if (runtime->current_file_path) {
        kryon_free(runtime->current_file_path);
    }
    runtime->current_file_path = kryon_strdup(krb_file_path);
    print("🧭 Override runtime file path for navigation: %s\n", krb_file_path);
    
    // Clean up path strings
    free(krb_path_copy);
    free(krb_filename_copy);
    
    // Restore original working directory for navigation to work correctly
    if (chdir(original_cwd) != 0) {
        fprint(2, "⚠️ Warning: Failed to restore original working directory\n");
    }
    
    if (debug) {
        print("🐛 Debug: KRB file loaded successfully into runtime\n");
    }
    
    // Setup renderer
    if (!setup_renderer(runtime, renderer, debug, output_dir)) {
        fprint(2, "❌ Failed to setup renderer: %s\n", renderer);
        kryon_runtime_destroy(runtime);
        return 1;
    }
    
    if (debug) {
        print("🐛 Debug: Renderer setup successfully: %s\n", renderer);
    }
    
    // Start the runtime
    if (!kryon_runtime_start(runtime)) {
        fprint(2, "❌ Failed to start runtime\n");
        kryon_runtime_destroy(runtime);
        return 1;
    }
    
    if (debug) {
        print("🐛 Debug: Runtime started successfully\n");
    }
    
    // Execute rendering loop
    print("🎬 Starting render loop with %s\n", renderer);
    
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

        // Poll events for renderers that need it (like SDL2)
        if (runtime->renderer && ((KryonRenderer*)runtime->renderer)->vtable->poll_events) {
            ((KryonRenderer*)runtime->renderer)->vtable->poll_events();
        }

        // For web renderer, skip actual rendering and just generate output once
#ifdef KRYON_RENDERER_WEB
        if (strcmp(renderer, "web") == 0) {
            print("🌐 Web renderer: Generating HTML/CSS/JS output\n");

            // Call finalize to write files - pass runtime for element tree access
            extern bool kryon_web_renderer_finalize_with_runtime(KryonRenderer* renderer, KryonRuntime* runtime);
            if (runtime->renderer) {
                kryon_web_renderer_finalize_with_runtime((KryonRenderer*)runtime->renderer, runtime);
            }
            frame_count++;
            break;
        }
#endif

        // Render frame (for non-web renderers)
        if (!kryon_runtime_render(runtime)) {
            fprint(2, "❌ Rendering failed at frame %d\n", frame_count);
            break;
        }
        frame_count++;

        // Exit after a reasonable number of frames for text renderer or testing
        if (strcmp(renderer, "text") == 0 && frame_count >= 60) {
            print("📝 Text renderer: completed %d frames\n", frame_count);
            break;
        }

        // For raylib and SDL2, check if window should close
        if (strcmp(renderer, "raylib") == 0 || strcmp(renderer, "sdl2") == 0) {
            // The renderer should set runtime->is_running = false when window closes
            if (!runtime->is_running) {
                print("🔲 Window closed by user\n");
                break;
            }
        }

        // Basic frame limiting (should be handled by renderer)
        usleep(16666); // ~60fps
    }
    
    print("✅ Rendering completed successfully with %s (%d frames)\n", renderer, frame_count);
    
    // Restore original working directory
    if (chdir(original_cwd) != 0) {
        fprint(2, "⚠️  Warning: Failed to restore original directory: %s\n", original_cwd);
    }
    
    // Cleanup
    kryon_runtime_destroy(runtime);

    // Free temp krb file if we auto-compiled
    if (temp_krb) {
        free(temp_krb);
    }

    return 0;
}

// Setup renderer for the runtime using internal interface
static bool setup_renderer(KryonRuntime* runtime, const char* renderer_name, bool debug, const char* output_dir) {
    if (!runtime || !renderer_name) {
        return false;
    }

    KryonRenderer* renderer = NULL;

    // Create renderer based on name
    if (strcmp(renderer_name, "web") == 0) {
#ifdef KRYON_RENDERER_WEB
        renderer = create_web_renderer(runtime, debug, output_dir);
#else
        fprint(2, "❌ WEB RENDERER NOT AVAILABLE\n");
        fprint(2, "❌ This build was compiled without web renderer support\n");
        return false;
#endif
    } else if (strcmp(renderer_name, "raylib") == 0) {
#ifdef KRYON_RENDERER_RAYLIB
        renderer = create_raylib_renderer(runtime, debug);
#else
        fprint(2, "❌ RAYLIB NOT AVAILABLE\n");
        fprint(2, "❌ This build was compiled without raylib support\n");
        fprint(2, "💡 Available renderers: text, web\n");
        fprint(2, "💡 To use raylib, install raylib-dev and recompile\n");
        return false;
#endif
    } else if (strcmp(renderer_name, "sdl2") == 0) {
#ifdef KRYON_RENDERER_SDL2
        renderer = create_sdl2_renderer(runtime, debug);
#else
        fprint(2, "❌ SDL2 NOT AVAILABLE\n");
        fprint(2, "❌ This build was compiled without SDL2 support\n");
        fprint(2, "💡 Available renderers: text, web, raylib\n");
        fprint(2, "💡 To use SDL2, install SDL2-dev and recompile\n");
        return false;
#endif
    } else if (strcmp(renderer_name, "text") == 0) {
        // For text mode, we don't need a real renderer
        if (debug) {
            print("🐛 Debug: Using text mode (no renderer needed)\n");
        }
        return true;
    } else {
        fprint(2, "❌ Unknown renderer: %s\n", renderer_name);
        fprint(2, "Available renderers: web, raylib, sdl2, text\n");
        return false;
    }
    
    if (!renderer) {
        fprint(2, "❌ Failed to create %s renderer\n", renderer_name);
        return false;
    }
    
    // Store renderer in runtime
    runtime->renderer = (void*)renderer;
    
    if (debug) {
        print("🐛 Debug: Renderer '%s' setup successfully\n", renderer_name);
    }
    
    return true;
}

// Create raylib renderer using internal interface
static KryonRenderer* create_raylib_renderer(KryonRuntime* runtime, bool debug) {
#ifdef KRYON_RENDERER_RAYLIB
    if (debug) {
        print("🐛 Debug: Creating raylib renderer\n");
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
        
        print("🔍 DEBUG: App element pointer: %p\n", (void*)app_element);
        print("🔍 DEBUG: App element type: %u (%s)\n", app_element->type, app_element->type_name ? app_element->type_name : "NULL");
        print("🔍 DEBUG: App element properties pointer: %p\n", (void*)app_element->properties);
        print("🔍 DEBUG: App element has %zu properties:\n", app_element->property_count);
        for (size_t j = 0; j < app_element->property_count; j++) {
            if (app_element->properties[j] && app_element->properties[j]->name) {
                print("  - %s (type=%d)\n", app_element->properties[j]->name, app_element->properties[j]->type);
            }
        }
        
        // Look for title property in App element
        for (size_t i = 0; i < app_element->property_count; i++) {
            if (app_element->properties[i] && app_element->properties[i]->name) {
                if (strcmp(app_element->properties[i]->name, "title") == 0 && 
                    app_element->properties[i]->value.string_value) {
                    surface.title = app_element->properties[i]->value.string_value;
                    print("🐛 Debug: Using title from App metadata: %s\n", surface.title);
                }
                else if (strcmp(app_element->properties[i]->name, "windowWidth") == 0 ||
                         strcmp(app_element->properties[i]->name, "winWidth") == 0) {
                    surface.width = (int)app_element->properties[i]->value.float_value;
                    if (debug) {
                        print("🐛 Debug: Using windowWidth from App metadata: %d\n", surface.width);
                    }
                }
                else if (strcmp(app_element->properties[i]->name, "windowHeight") == 0 ||
                         strcmp(app_element->properties[i]->name, "winHeight") == 0) {
                    surface.height = (int)app_element->properties[i]->value.float_value;
                    if (debug) {
                        print("🐛 Debug: Using windowHeight from App metadata: %d\n", surface.height);
                    }
                }
                else if (strcmp(app_element->properties[i]->name, "windowTitle") == 0 ||
                         strcmp(app_element->properties[i]->name, "winTitle") == 0) {
                    if (app_element->properties[i]->value.string_value) {
                        surface.title = app_element->properties[i]->value.string_value;
                        if (debug) {
                            print("🐛 Debug: Using windowTitle from App metadata: %s\n", surface.title);
                        }
                    }
                }
            }
        }
    }
    
    if (debug) {
        print("🐛 Debug: Creating raylib surface: %dx%d '%s'\n", 
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
        fprint(2, "❌ Failed to create raylib renderer\n");
        if (debug) {
            print("🐛 Debug: Raylib renderer creation failed\n");
        }
        return NULL;
    }
    
    if (debug) {
        print("🐛 Debug: Raylib renderer created successfully\n");
    }
    
    return renderer;
#else
    fprint(2, "❌ RAYLIB NOT AVAILABLE\n");
    fprint(2, "❌ This build was compiled without raylib support\n");
    fprint(2, "💡 Available renderers: text\n");
    fprint(2, "💡 To use raylib, install raylib-dev and recompile\n");

    return NULL;
#endif
}

// Create SDL2 renderer using internal interface
static KryonRenderer* create_sdl2_renderer(KryonRuntime* runtime, bool debug) {
#ifdef KRYON_RENDERER_SDL2
    if (debug) {
        print("🐛 Debug: Creating SDL2 renderer\n");
    }

    // Create surface with default values, then override from App element metadata
    typedef struct {
        int width;
        int height;
        const char* title;
        bool fullscreen;
    } SDL2Surface;

    SDL2Surface surface = {
        .width = 800,
        .height = 600,
        .title = "Kryon Application",
        .fullscreen = false
    };

    // Extract surface properties from App element metadata if available
    if (runtime && runtime->root) {
        KryonElement* app_element = runtime->root;

        // Look for title property in App element
        for (size_t i = 0; i < app_element->property_count; i++) {
            if (app_element->properties[i] && app_element->properties[i]->name) {
                if (strcmp(app_element->properties[i]->name, "title") == 0 &&
                    app_element->properties[i]->value.string_value) {
                    surface.title = app_element->properties[i]->value.string_value;
                    if (debug) {
                        print("🐛 Debug: Using title from App metadata: %s\n", surface.title);
                    }
                }
                else if (strcmp(app_element->properties[i]->name, "windowWidth") == 0 ||
                         strcmp(app_element->properties[i]->name, "winWidth") == 0) {
                    surface.width = (int)app_element->properties[i]->value.float_value;
                    if (debug) {
                        print("🐛 Debug: Using windowWidth from App metadata: %d\n", surface.width);
                    }
                }
                else if (strcmp(app_element->properties[i]->name, "windowHeight") == 0 ||
                         strcmp(app_element->properties[i]->name, "winHeight") == 0) {
                    surface.height = (int)app_element->properties[i]->value.float_value;
                    if (debug) {
                        print("🐛 Debug: Using windowHeight from App metadata: %d\n", surface.height);
                    }
                }
                else if (strcmp(app_element->properties[i]->name, "windowTitle") == 0 ||
                         strcmp(app_element->properties[i]->name, "winTitle") == 0) {
                    if (app_element->properties[i]->value.string_value) {
                        surface.title = app_element->properties[i]->value.string_value;
                        if (debug) {
                            print("🐛 Debug: Using windowTitle from App metadata: %s\n", surface.title);
                        }
                    }
                }
            }
        }
    }

    if (debug) {
        print("🐛 Debug: Creating SDL2 surface: %dx%d '%s'\n",
               surface.width, surface.height, surface.title);
    }

    // Create renderer configuration with event callback
    KryonRendererConfig config = {
        .event_callback = runtime_receive_input_event,
        .callback_data = runtime,
        .platform_context = &surface
    };

    // Forward declaration from SDL2 renderer
    extern KryonRenderer* kryon_sdl2_renderer_create(const KryonRendererConfig* config);

    KryonRenderer* renderer = kryon_sdl2_renderer_create(&config);

    if (!renderer) {
        fprint(2, "❌ Failed to create SDL2 renderer\n");
        if (debug) {
            print("🐛 Debug: SDL2 renderer creation failed\n");
        }
        return NULL;
    }

    if (debug) {
        print("🐛 Debug: SDL2 renderer created successfully\n");
    }

    return renderer;
#else
    fprint(2, "❌ SDL2 NOT AVAILABLE\n");
    fprint(2, "❌ This build was compiled without SDL2 support\n");
    fprint(2, "💡 Available renderers: text\n");
    fprint(2, "💡 To use SDL2, install SDL2-dev and recompile\n");

    return NULL;
#endif
}

// Create web renderer using internal interface
static KryonRenderer* create_web_renderer(KryonRuntime* runtime, bool debug, const char* output_dir) {
#ifdef KRYON_RENDERER_WEB
    // Forward declare from web_renderer.h
    extern KryonRenderer* kryon_web_renderer_create(const KryonRendererConfig* config);

    if (debug) {
        print("🐛 Debug: Creating web renderer\n");
        print("🐛 Debug: Output directory: %s\n", output_dir);
    }

    // Create renderer configuration
    KryonRendererConfig config = {
        .event_callback = NULL,  // Web renderer doesn't need event callback
        .callback_data = runtime,
        .platform_context = (void*)output_dir  // Pass output directory as context
    };

    KryonRenderer* renderer = kryon_web_renderer_create(&config);

    if (!renderer) {
        fprint(2, "❌ Failed to create web renderer\n");
        if (debug) {
            print("🐛 Debug: Web renderer creation failed\n");
        }
        return NULL;
    }

    if (debug) {
        print("🐛 Debug: Web renderer created successfully\n");
    }

    return renderer;
#else
    fprint(2, "❌ WEB RENDERER NOT AVAILABLE\n");
    fprint(2, "❌ This build was compiled without web renderer support\n");

    return NULL;
#endif
}
