/**
 * @file dev_command.c
 * @brief Kryon Development Mode Command Implementation
 */

#include "internal/runtime.h"
#include "internal/memory.h"
#include "internal/platform.h"
#include "internal/lexer.h"
#include "internal/parser.h"
#include "internal/codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

static volatile bool g_running = false;

static void signal_handler(int sig) {
    (void)sig;
    g_running = false;
    printf("\nShutting down development server...\n");
}

static time_t get_file_mtime(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

static bool compile_kry_to_krb(const char *kry_file, const char *krb_file, bool verbose) {
    // Read KRY file
    FILE *file = fopen(kry_file, "r");
    if (!file) {
        if (verbose) fprintf(stderr, "Cannot open %s\n", kry_file);
        return false;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *source = kryon_alloc(file_size + 1);
    if (!source) {
        fclose(file);
        return false;
    }
    
    fread(source, 1, file_size, file);
    source[file_size] = '\0';
    fclose(file);
    
    // Compile
    KryonLexer *lexer = kryon_lexer_create(source, kry_file);
    if (!lexer) {
        kryon_free(source);
        return false;
    }
    
    KryonParser *parser = kryon_parser_create(lexer);
    if (!parser) {
        kryon_lexer_destroy(lexer);
        kryon_free(source);
        return false;
    }
    
    KryonASTNode *ast = kryon_parser_parse(parser);
    if (!ast || kryon_parser_had_error(parser)) {
        if (verbose) {
            fprintf(stderr, "Parse error: %s\n", kryon_parser_error_message(parser));
        }
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_free(source);
        return false;
    }
    
    KryonCodeGenConfig config = kryon_codegen_debug_config();
    KryonCodeGenerator *codegen = kryon_codegen_create(&config);
    if (!codegen) {
        kryon_ast_destroy(ast);
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_free(source);
        return false;
    }
    
    if (!kryon_codegen_generate(codegen, ast)) {
        if (verbose) {
            size_t error_count;
            const char **errors = kryon_codegen_get_errors(codegen, &error_count);
            for (size_t i = 0; i < error_count; i++) {
                fprintf(stderr, "Codegen error: %s\n", errors[i]);
            }
        }
        kryon_codegen_destroy(codegen);
        kryon_ast_destroy(ast);
        kryon_parser_destroy(parser);
        kryon_lexer_destroy(lexer);
        kryon_free(source);
        return false;
    }
    
    bool success = kryon_codegen_write_file(codegen, krb_file);
    
    // Cleanup
    kryon_codegen_destroy(codegen);
    kryon_ast_destroy(ast);
    kryon_parser_destroy(parser);
    kryon_lexer_destroy(lexer);
    kryon_free(source);
    
    if (verbose && success) {
        printf("✓ Compiled %s -> %s\n", kry_file, krb_file);
    }
    
    return success;
}

int dev_command(int argc, char *argv[]) {
    const char *input_file = NULL;
    const char *renderer = "sdl2";
    int width = 800;
    int height = 600;
    bool verbose = true;
    int reload_delay = 100; // milliseconds
    
    // Parse arguments
    static struct option long_options[] = {
        {"renderer", required_argument, 0, 'r'},
        {"width", required_argument, 0, 'w'},
        {"height", required_argument, 0, 'h'},
        {"quiet", no_argument, 0, 'q'},
        {"delay", required_argument, 0, 'd'},
        {"help", no_argument, 0, '?'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "r:w:h:qd:?", long_options, NULL)) != -1) {
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
            case 'q':
                verbose = false;
                break;
            case 'd':
                reload_delay = atoi(optarg);
                if (reload_delay < 10 || reload_delay > 5000) {
                    fprintf(stderr, "Error: Invalid delay (10-5000ms): %s\n", optarg);
                    return 1;
                }
                break;
            case '?':
                printf("Usage: kryon dev <file.kry> [options]\n");
                printf("Options:\n");
                printf("  -r, --renderer <name>  Renderer backend (sdl2, raylib, html, software)\n");
                printf("  -w, --width <pixels>   Window width (default: 800)\n");
                printf("  -h, --height <pixels>  Window height (default: 600)\n");
                printf("  -q, --quiet            Less verbose output\n");
                printf("  -d, --delay <ms>       File check delay (default: 100ms)\n");
                printf("  --help                 Show this help\n");
                return 0;
            default:
                return 1;
        }
    }
    
    if (optind >= argc) {
        fprintf(stderr, "Error: No input file specified\n");
        fprintf(stderr, "Usage: kryon dev <file.kry> [options]\n");
        return 1;
    }
    
    input_file = argv[optind];
    
    // Generate temporary KRB filename
    char krb_file[256];
    const char *dot = strrchr(input_file, '.');
    if (dot && strcmp(dot, ".kry") == 0) {
        snprintf(krb_file, sizeof(krb_file), "%.*s.dev.krb", (int)(dot - input_file), input_file);
    } else {
        snprintf(krb_file, sizeof(krb_file), "%s.dev.krb", input_file);
    }
    
    if (verbose) {
        printf("🚀 Kryon Development Server\n");
        printf("Source: %s\n", input_file);
        printf("Temporary: %s\n", krb_file);
        printf("Renderer: %s\n", renderer);
        printf("Window: %dx%d\n", width, height);
        printf("Hot reload enabled (checking every %dms)\n", reload_delay);
        printf("\n");
    }
    
    // Install signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initial compilation
    if (!compile_kry_to_krb(input_file, krb_file, verbose)) {
        fprintf(stderr, "Error: Initial compilation failed\n");
        return 1;
    }
    
    // Initialize platform
    KryonPlatformConfig platform_config = {
        .window_title = "Kryon Development",
        .window_width = width,
        .window_height = height,
        .fullscreen = false,
        .vsync = true,
        .resizable = true
    };
    
    if (!kryon_platform_init(&platform_config)) {
        fprintf(stderr, "Error: Failed to initialize platform\n");
        return 1;
    }
    
    // Create runtime
    KryonRuntimeConfig runtime_config = kryon_runtime_dev_config();
    runtime_config.enable_hot_reload = true;
    runtime_config.enable_profiling = verbose;
    
    KryonRuntime *runtime = kryon_runtime_create(&runtime_config);
    if (!runtime) {
        fprintf(stderr, "Error: Failed to create runtime\n");
        kryon_platform_shutdown();
        return 1;
    }
    
    // Load initial KRB
    if (!kryon_runtime_load_file(runtime, krb_file)) {
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
        printf("✓ Development server started\n");
        printf("Watching %s for changes...\n", input_file);
        printf("Press Ctrl+C to exit\n\n");
    }
    
    // Main development loop with hot reload
    g_running = true;
    double last_time = kryon_platform_get_time();
    time_t last_file_time = get_file_mtime(input_file);
    double last_check_time = 0.0;
    
    while (g_running && kryon_platform_should_continue()) {
        double current_time = kryon_platform_get_time();
        double delta_time = current_time - last_time;
        last_time = current_time;
        
        // Check for file changes periodically
        if (current_time - last_check_time > (reload_delay / 1000.0)) {
            last_check_time = current_time;
            
            time_t current_file_time = get_file_mtime(input_file);
            if (current_file_time > last_file_time) {
                last_file_time = current_file_time;
                
                if (verbose) {
                    printf("📁 File changed, recompiling...\n");
                }
                
                // Recompile
                if (compile_kry_to_krb(input_file, krb_file, verbose)) {
                    // Stop current runtime
                    kryon_runtime_stop(runtime);
                    
                    // Reload KRB
                    if (kryon_runtime_load_file(runtime, krb_file)) {
                        // Restart runtime
                        if (kryon_runtime_start(runtime)) {
                            if (verbose) {
                                printf("🔄 Hot reload successful\n");
                            }
                        } else {
                            fprintf(stderr, "❌ Failed to restart runtime after reload\n");
                        }
                    } else {
                        if (verbose) {
                            fprintf(stderr, "❌ Failed to reload KRB file\n");
                            size_t error_count;
                            const char **errors = kryon_runtime_get_errors(runtime, &error_count);
                            for (size_t i = 0; i < error_count; i++) {
                                fprintf(stderr, "  %s\n", errors[i]);
                            }
                        }
                    }
                } else {
                    if (verbose) {
                        printf("❌ Compilation failed, keeping current version\n");
                    }
                }
            }
        }
        
        // Process platform events
        kryon_platform_poll_events();
        
        // Handle input events
        KryonPlatformEvent platform_event;
        while (kryon_platform_get_event(&platform_event)) {
            KryonEvent runtime_event = {0};
            
            switch (platform_event.type) {
                case KRYON_PLATFORM_EVENT_QUIT:
                    g_running = false;
                    break;
                    
                case KRYON_PLATFORM_EVENT_KEY_DOWN:
                    // F5 for manual reload
                    if (platform_event.data.key.code == KRYON_KEY_F5) {
                        if (verbose) printf("🔄 Manual reload triggered\n");
                        compile_kry_to_krb(input_file, krb_file, verbose);
                        kryon_runtime_stop(runtime);
                        kryon_runtime_load_file(runtime, krb_file);
                        kryon_runtime_start(runtime);
                        break;
                    }
                    
                    runtime_event.type = KRYON_EVENT_KEY_DOWN;
                    runtime_event.data.key.code = platform_event.data.key.code;
                    runtime_event.data.key.modifiers = platform_event.data.key.modifiers;
                    kryon_runtime_handle_event(runtime, &runtime_event);
                    break;
                    
                default:
                    // Forward other events to runtime
                    break;
            }
        }
        
        // Update runtime
        kryon_runtime_update(runtime, delta_time);
        
        // Render
        if (renderer_interface->begin_frame) {
            renderer_interface->begin_frame(renderer_interface);
        }
        
        kryon_runtime_render(runtime);
        
        if (renderer_interface->end_frame) {
            renderer_interface->end_frame(renderer_interface);
        }
        
        kryon_platform_swap_buffers();
        
        // Cap frame rate
        usleep(16667); // ~60 FPS
    }
    
    if (verbose) {
        printf("🛑 Shutting down development server...\n");
    }
    
    // Cleanup
    kryon_runtime_stop(runtime);
    renderer_interface->destroy(renderer_interface);
    kryon_runtime_destroy(runtime);
    kryon_platform_shutdown();
    
    // Remove temporary file
    unlink(krb_file);
    
    if (verbose) {
        printf("✓ Development server stopped\n");
    }
    
    return 0;
}