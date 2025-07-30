/**
 * @file run_command.c
 * @brief Kryon Run Command Implementation
 */

#include "internal/runtime.h"
#include "internal/memory.h"
#include "internal/platform.h"
#include "internal/renderer_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>

static volatile bool g_running = false;

static void signal_handler(int sig) {
    (void)sig;
    g_running = false;
    printf("\nShutting down...\n");
}

int run_command(int argc, char *argv[]) {
    const char *input_file = NULL;
    const char *renderer = "sdl2";
    int width = 800;
    int height = 600;
    bool fullscreen = false;
    bool verbose = false;
    
    // Parse arguments
    static struct option long_options[] = {
        {"renderer", required_argument, 0, 'r'},
        {"width", required_argument, 0, 'w'},
        {"height", required_argument, 0, 'h'},
        {"fullscreen", no_argument, 0, 'f'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, '?'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "r:w:h:fv?", long_options, NULL)) != -1) {
        switch (c) {
            case 'r':
                renderer = optarg;
                break;
            case 'w':
                width = atoi(optarg);
                if (width <= 0) {
                    fprintf(stderr, "Error: Invalid width: %s\n", optarg);
                    return 1;
                }
                break;
            case 'h':
                height = atoi(optarg);
                if (height <= 0) {
                    fprintf(stderr, "Error: Invalid height: %s\n", optarg);
                    return 1;
                }
                break;
            case 'f':
                fullscreen = true;
                break;
            case 'v':
                verbose = true;
                break;
            case '?':
                printf("Usage: kryon run <file.krb> [options]\n");
                printf("Options:\n");
                printf("  -r, --renderer <name>  Renderer backend (sdl2, raylib, html, software)\n");
                printf("  -w, --width <pixels>   Window width (default: 800)\n");
                printf("  -h, --height <pixels>  Window height (default: 600)\n");
                printf("  -f, --fullscreen       Start in fullscreen mode\n");
                printf("  -v, --verbose          Verbose output\n");
                printf("  --help                 Show this help\n");
                return 0;
            default:
                return 1;
        }
    }
    
    if (optind >= argc) {
        fprintf(stderr, "Error: No input file specified\n");
        fprintf(stderr, "Usage: kryon run <file.krb> [options]\n");
        return 1;
    }
    
    input_file = argv[optind];
    
    if (verbose) {
        printf("Kryon Runtime Starting\n");
        printf("File: %s\n", input_file);
        printf("Renderer: %s\n", renderer);
        printf("Window: %dx%d%s\n", width, height, fullscreen ? " (fullscreen)" : "");
    }
    
    // Install signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize platform
    KryonPlatformConfig platform_config = {
        .window_title = "Kryon Application",
        .window_width = width,
        .window_height = height,
        .fullscreen = fullscreen,
        .vsync = true,
        .resizable = true
    };
    
    if (!kryon_platform_init(&platform_config)) {
        fprintf(stderr, "Error: Failed to initialize platform\n");
        return 1;
    }
    
    // Create runtime
    KryonRuntimeConfig runtime_config = kryon_runtime_default_config();
    runtime_config.max_update_fps = 60;
    runtime_config.enable_profiling = verbose;
    
    KryonRuntime *runtime = kryon_runtime_create(&runtime_config);
    if (!runtime) {
        fprintf(stderr, "Error: Failed to create runtime\n");
        kryon_platform_shutdown();
        return 1;
    }
    
    // Load KRB file
    if (verbose) {
        printf("Loading %s...\n", input_file);
    }
    
    if (!kryon_runtime_load_file(runtime, input_file)) {
        size_t error_count;
        const char **errors = kryon_runtime_get_errors(runtime, &error_count);
        for (size_t i = 0; i < error_count; i++) {
            fprintf(stderr, "Error: %s\n", errors[i]);
        }
        kryon_runtime_destroy(runtime);
        kryon_platform_shutdown();
        return 1;
    }
    
    // Initialize renderer
    KryonRendererInterface *renderer_interface = NULL;
    
    if (strcmp(renderer, "sdl2") == 0) {
        renderer_interface = kryon_sdl2_renderer_create(width, height);
    } else if (strcmp(renderer, "raylib") == 0) {
        renderer_interface = kryon_raylib_renderer_create(width, height);
    } else if (strcmp(renderer, "html") == 0) {
        renderer_interface = kryon_html_renderer_create(width, height);
    } else if (strcmp(renderer, "software") == 0) {
        renderer_interface = kryon_software_renderer_create(width, height);
    } else {
        fprintf(stderr, "Error: Unknown renderer: %s\n", renderer);
        fprintf(stderr, "Available renderers: sdl2, raylib, html, software\n");
        kryon_runtime_destroy(runtime);
        kryon_platform_shutdown();
        return 1;
    }
    
    if (!renderer_interface) {
        fprintf(stderr, "Error: Failed to create %s renderer\n", renderer);
        kryon_runtime_destroy(runtime);
        kryon_platform_shutdown();
        return 1;
    }
    
    // Start runtime
    if (!kryon_runtime_start(runtime)) {
        size_t error_count;
        const char **errors = kryon_runtime_get_errors(runtime, &error_count);
        for (size_t i = 0; i < error_count; i++) {
            fprintf(stderr, "Error: %s\n", errors[i]);
        }
        renderer_interface->destroy(renderer_interface);
        kryon_runtime_destroy(runtime);
        kryon_platform_shutdown();
        return 1;
    }
    
    if (verbose) {
        printf("Application started successfully\n");
        printf("Press Ctrl+C to exit\n");
    }
    
    // Main loop
    g_running = true;
    double last_time = kryon_platform_get_time();
    double frame_time = 0.0;
    int frame_count = 0;
    
    while (g_running && kryon_platform_should_continue()) {
        double current_time = kryon_platform_get_time();
        double delta_time = current_time - last_time;
        last_time = current_time;
        
        // Process platform events
        kryon_platform_poll_events();
        
        // Handle input events and forward to runtime
        KryonPlatformEvent platform_event;
        while (kryon_platform_get_event(&platform_event)) {
            // Convert platform events to runtime events
            KryonEvent runtime_event = {0};
            
            switch (platform_event.type) {
                case KRYON_PLATFORM_EVENT_QUIT:
                    g_running = false;
                    break;
                    
                case KRYON_PLATFORM_EVENT_KEY_DOWN:
                    runtime_event.type = KRYON_EVENT_KEY_DOWN;
                    runtime_event.data.key.code = platform_event.data.key.code;
                    runtime_event.data.key.modifiers = platform_event.data.key.modifiers;
                    kryon_runtime_handle_event(runtime, &runtime_event);
                    break;
                    
                case KRYON_PLATFORM_EVENT_KEY_UP:
                    runtime_event.type = KRYON_EVENT_KEY_UP;
                    runtime_event.data.key.code = platform_event.data.key.code;
                    runtime_event.data.key.modifiers = platform_event.data.key.modifiers;
                    kryon_runtime_handle_event(runtime, &runtime_event);
                    break;
                    
                case KRYON_PLATFORM_EVENT_MOUSE_MOVE:
                    runtime_event.type = KRYON_EVENT_MOUSE_MOVED;
                    runtime_event.data.mouse.x = platform_event.data.mouse.x;
                    runtime_event.data.mouse.y = platform_event.data.mouse.y;
                    kryon_runtime_handle_event(runtime, &runtime_event);
                    break;
                    
                case KRYON_PLATFORM_EVENT_MOUSE_DOWN:
                    runtime_event.type = KRYON_EVENT_MOUSE_DOWN;
                    runtime_event.data.mouse.x = platform_event.data.mouse.x;
                    runtime_event.data.mouse.y = platform_event.data.mouse.y;
                    runtime_event.data.mouse.button = platform_event.data.mouse.button;
                    kryon_runtime_handle_event(runtime, &runtime_event);
                    break;
                    
                case KRYON_PLATFORM_EVENT_MOUSE_UP:
                    runtime_event.type = KRYON_EVENT_MOUSE_UP;
                    runtime_event.data.mouse.x = platform_event.data.mouse.x;
                    runtime_event.data.mouse.y = platform_event.data.mouse.y;
                    runtime_event.data.mouse.button = platform_event.data.mouse.button;
                    kryon_runtime_handle_event(runtime, &runtime_event);
                    break;
                    
                case KRYON_PLATFORM_EVENT_WINDOW_RESIZE:
                    runtime_event.type = KRYON_EVENT_WINDOW_RESIZE;
                    runtime_event.data.window.width = platform_event.data.window.width;
                    runtime_event.data.window.height = platform_event.data.window.height;
                    kryon_runtime_handle_event(runtime, &runtime_event);
                    
                    // Resize renderer
                    if (renderer_interface->resize) {
                        renderer_interface->resize(renderer_interface, 
                                                 platform_event.data.window.width,
                                                 platform_event.data.window.height);
                    }
                    break;
                    
                default:
                    break;
            }
        }
        
        // Update runtime
        if (!kryon_runtime_update(runtime, delta_time)) {
            if (verbose) {
                size_t error_count;
                const char **errors = kryon_runtime_get_errors(runtime, &error_count);
                if (error_count > 0) {
                    printf("Runtime errors:\n");
                    for (size_t i = 0; i < error_count; i++) {
                        printf("  %s\n", errors[i]);
                    }
                }
            }
            break;
        }
        
        // Begin frame
        if (renderer_interface->begin_frame) {
            renderer_interface->begin_frame(renderer_interface);
        }
        
        // Render
        if (!kryon_runtime_render(runtime)) {
            if (verbose) {
                printf("Render failed\n");
            }
        }
        
        // End frame and present
        if (renderer_interface->end_frame) {
            renderer_interface->end_frame(renderer_interface);
        }
        
        kryon_platform_swap_buffers();
        
        // Update frame statistics
        frame_count++;
        frame_time += delta_time;
        
        if (verbose && frame_time >= 1.0) {
            double fps = frame_count / frame_time;
            printf("FPS: %.1f (%.2f ms/frame)\n", fps, frame_time * 1000.0 / frame_count);
            frame_count = 0;
            frame_time = 0.0;
        }
        
        // Cap frame rate (basic implementation)
        double target_frame_time = 1.0 / 60.0;
        double sleep_time = target_frame_time - delta_time;
        if (sleep_time > 0) {
            usleep((int)(sleep_time * 1000000));
        }
    }
    
    if (verbose) {
        printf("Shutting down application...\n");
    }
    
    // Cleanup
    kryon_runtime_stop(runtime);
    renderer_interface->destroy(renderer_interface);
    kryon_runtime_destroy(runtime);
    kryon_platform_shutdown();
    
    if (verbose) {
        printf("Shutdown complete\n");
    }
    
    return 0;
}