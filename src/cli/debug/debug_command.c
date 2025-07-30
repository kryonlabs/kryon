/**
 * @file debug_command.c
 * @brief Kryon Debug Command Implementation
 */

#include "internal/runtime.h"
#include "internal/memory.h"
#include "internal/platform.h"
#include "internal/krb_format.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

static volatile bool g_running = false;
static KryonRuntime *g_debug_runtime = NULL;

static void signal_handler(int sig) {
    (void)sig;
    g_running = false;
    printf("\nDebugger interrupted\n");
}

static void print_debug_help(void) {
    printf("Kryon Debugger Commands:\n");
    printf("  help, h                 Show this help\n");
    printf("  run, r                  Start/resume execution\n");
    printf("  step, s                 Step one frame\n");
    printf("  pause, p                Pause execution\n");
    printf("  quit, q                 Quit debugger\n");
    printf("  elements, el            List all elements\n");
    printf("  element <id>, e <id>    Inspect element by ID\n");
    printf("  state, st               Show global state\n");
    printf("  events, ev              Show recent events\n");
    printf("  memory, mem             Show memory statistics\n");
    printf("  reload, rl              Reload KRB file\n");
    printf("  break <element>, b <id> Set breakpoint on element\n");
    printf("  unbreak <element>, ub   Remove breakpoint\n");
    printf("  breakpoints, bp         List breakpoints\n");
    printf("  watch <property>, w     Watch property changes\n");
    printf("  unwatch <property>      Stop watching property\n");
    printf("  trace, tr               Toggle execution tracing\n");
    printf("  profile, pr             Show performance profile\n");
}

static void debug_list_elements(KryonRuntime *runtime) {
    printf("Element Tree:\n");
    
    KryonElement *root = kryon_runtime_get_root_element(runtime);
    if (!root) {
        printf("  (no elements loaded)\n");
        return;
    }
    
    // Simple tree traversal
    static void print_element(KryonElement *elem, int depth) {
        if (!elem) return;
        
        for (int i = 0; i < depth; i++) printf("  ");
        printf("├─ [%u] %s", elem->id, elem->type_name ? elem->type_name : "Unknown");
        
        if (elem->element_id) {
            printf(" id=\"%s\"", elem->element_id);
        }
        
        if (elem->class_count > 0) {
            printf(" class=\"");
            for (size_t i = 0; i < elem->class_count; i++) {
                if (i > 0) printf(" ");
                printf("%s", elem->class_names[i]);
            }
            printf("\"");
        }
        
        printf(" (%s)\n", 
               elem->visible ? "visible" : "hidden");
        
        // Print children
        for (size_t i = 0; i < elem->child_count; i++) {
            print_element(elem->children[i], depth + 1);
        }
    }
    
    print_element(root, 0);
}

static void debug_inspect_element(KryonRuntime *runtime, uint32_t element_id) {
    KryonElement *elem = kryon_runtime_find_element(runtime, element_id);
    if (!elem) {
        printf("Element %u not found\n", element_id);
        return;
    }
    
    printf("Element %u:\n", element_id);
    printf("  Type: %s\n", elem->type_name ? elem->type_name : "Unknown");
    printf("  ID: %s\n", elem->element_id ? elem->element_id : "(none)");
    printf("  State: %d\n", elem->state);
    printf("  Visible: %s\n", elem->visible ? "true" : "false");
    printf("  Enabled: %s\n", elem->enabled ? "true" : "false");
    printf("  Position: (%.1f, %.1f)\n", elem->layout.x, elem->layout.y);
    printf("  Size: %.1f x %.1f\n", elem->layout.width, elem->layout.height);
    printf("  Children: %zu\n", elem->child_count);
    printf("  Properties: %zu\n", elem->property_count);
    
    if (elem->property_count > 0) {
        printf("  Property values:\n");
        for (size_t i = 0; i < elem->property_count; i++) {
            KryonProperty *prop = elem->properties[i];
            if (!prop) continue;
            
            printf("    %s: ", prop->name ? prop->name : "unknown");
            
            switch (prop->type) {
                case KRYON_RUNTIME_PROP_STRING:
                    printf("\"%s\"\n", prop->value.string_value ? prop->value.string_value : "null");
                    break;
                case KRYON_RUNTIME_PROP_INTEGER:
                    printf("%ld\n", prop->value.int_value);
                    break;
                case KRYON_RUNTIME_PROP_FLOAT:
                    printf("%.3f\n", prop->value.float_value);
                    break;
                case KRYON_RUNTIME_PROP_BOOLEAN:
                    printf("%s\n", prop->value.bool_value ? "true" : "false");
                    break;
                case KRYON_RUNTIME_PROP_COLOR:
                    printf("#%08X\n", prop->value.color_value);
                    break;
                default:
                    printf("(unknown type %d)\n", prop->type);
                    break;
            }
        }
    }
}

static void debug_show_memory_stats(void) {
    KryonMemoryStats stats;
    kryon_memory_get_stats(g_kryon_memory_manager, &stats);
    
    printf("Memory Statistics:\n");
    printf("  Total allocated: %zu bytes\n", stats.total_allocated);
    printf("  Peak allocated: %zu bytes\n", stats.peak_allocated);
    printf("  Allocation count: %u\n", stats.alloc_count);
    printf("  Free count: %u\n", stats.free_count);
    printf("  Active allocations: %u\n", stats.alloc_count - stats.free_count);
    printf("  Out of memory events: %u\n", stats.out_of_memory_count);
    printf("  Average alloc time: %.3f μs\n", 
           stats.alloc_count > 0 ? (stats.total_alloc_time_ns / stats.alloc_count / 1000.0) : 0.0);
    printf("  Average free time: %.3f μs\n",
           stats.free_count > 0 ? (stats.total_free_time_ns / stats.free_count / 1000.0) : 0.0);
    
    // Check for leaks
    uint32_t leaks = kryon_memory_check_leaks(g_kryon_memory_manager);
    if (leaks > 0) {
        printf("  ⚠️  Memory leaks detected: %u\n", leaks);
    } else {
        printf("  ✅ No memory leaks detected\n");
    }
}

int debug_command(int argc, char *argv[]) {
    const char *input_file = NULL;
    const char *renderer = "software"; // Use software renderer for debugging
    bool verbose = true;
    
    // Parse arguments
    static struct option long_options[] = {
        {"renderer", required_argument, 0, 'r'},
        {"quiet", no_argument, 0, 'q'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "r:qh", long_options, NULL)) != -1) {
        switch (c) {
            case 'r':
                renderer = optarg;
                break;
            case 'q':
                verbose = false;
                break;
            case 'h':
                printf("Usage: kryon debug <file.krb> [options]\n");
                printf("Options:\n");
                printf("  -r, --renderer <name>  Renderer backend (default: software)\n");
                printf("  -q, --quiet            Less verbose output\n");
                printf("  -h, --help             Show this help\n");
                printf("\nInteractive debugger for Kryon applications.\n");
                return 0;
            case '?':
                return 1;
            default:
                abort();
        }
    }
    
    if (optind >= argc) {
        fprintf(stderr, "Error: No input file specified\n");
        return 1;
    }
    
    input_file = argv[optind];
    
    printf("🐛 Kryon Interactive Debugger\n");
    printf("File: %s\n", input_file);
    printf("Renderer: %s\n", renderer);
    printf("Type 'help' for available commands.\n\n");
    
    // Install signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize platform in headless mode for debugging
    KryonPlatformConfig platform_config = {
        .window_title = "Kryon Debugger",
        .window_width = 800,
        .window_height = 600,
        .fullscreen = false,
        .vsync = false,
        .resizable = true
    };
    
    if (!kryon_platform_init(&platform_config)) {
        fprintf(stderr, "Error: Failed to initialize platform\n");
        return 1;
    }
    
    // Create runtime in debug mode
    KryonRuntimeConfig runtime_config = kryon_runtime_debug_config();
    runtime_config.enable_profiling = true;
    runtime_config.enable_validation = true;
    
    g_debug_runtime = kryon_runtime_create(&runtime_config);
    if (!g_debug_runtime) {
        fprintf(stderr, "Error: Failed to create debug runtime\n");
        kryon_platform_shutdown();
        return 1;
    }
    
    // Load KRB file
    if (!kryon_runtime_load_file(g_debug_runtime, input_file)) {
        size_t error_count;
        const char **errors = kryon_runtime_get_errors(g_debug_runtime, &error_count);
        for (size_t i = 0; i < error_count; i++) {
            fprintf(stderr, "Error: %s\n", errors[i]);
        }
        kryon_runtime_destroy(g_debug_runtime);
        kryon_platform_shutdown();
        return 1;
    }
    
    printf("✅ KRB file loaded successfully\n");
    printf("📊 Ready for debugging. Enter commands:\n\n");
    
    // Interactive debug loop
    g_running = true;
    bool runtime_running = false;
    char *line;
    
    while (g_running && (line = readline("(kryon-debug) ")) != NULL) {
        if (strlen(line) > 0) {
            add_history(line);
        }
        
        // Parse command
        char *cmd = strtok(line, " \t\n");
        if (!cmd) {
            free(line);
            continue;
        }
        
        if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {
            print_debug_help();
            
        } else if (strcmp(cmd, "run") == 0 || strcmp(cmd, "r") == 0) {
            if (!runtime_running) {
                if (kryon_runtime_start(g_debug_runtime)) {
                    runtime_running = true;
                    printf("✅ Runtime started\n");
                } else {
                    printf("❌ Failed to start runtime\n");
                }
            } else {
                printf("Runtime already running\n");
            }
            
        } else if (strcmp(cmd, "step") == 0 || strcmp(cmd, "s") == 0) {
            if (runtime_running) {
                kryon_runtime_update(g_debug_runtime, 1.0/60.0);
                printf("⏭️  Stepped one frame\n");
            } else {
                printf("Runtime not running. Use 'run' first.\n");
            }
            
        } else if (strcmp(cmd, "pause") == 0 || strcmp(cmd, "p") == 0) {
            if (runtime_running) {
                kryon_runtime_stop(g_debug_runtime);
                runtime_running = false;
                printf("⏸️  Runtime paused\n");
            } else {
                printf("Runtime not running\n");
            }
            
        } else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "q") == 0) {
            break;
            
        } else if (strcmp(cmd, "elements") == 0 || strcmp(cmd, "el") == 0) {
            debug_list_elements(g_debug_runtime);
            
        } else if (strcmp(cmd, "element") == 0 || strcmp(cmd, "e") == 0) {
            char *id_str = strtok(NULL, " \t\n");
            if (id_str) {
                uint32_t id = (uint32_t)atoi(id_str);
                debug_inspect_element(g_debug_runtime, id);
            } else {
                printf("Usage: element <id>\n");
            }
            
        } else if (strcmp(cmd, "memory") == 0 || strcmp(cmd, "mem") == 0) {
            debug_show_memory_stats();
            
        } else if (strcmp(cmd, "reload") == 0 || strcmp(cmd, "rl") == 0) {
            if (runtime_running) {
                kryon_runtime_stop(g_debug_runtime);
                runtime_running = false;
            }
            
            if (kryon_runtime_load_file(g_debug_runtime, input_file)) {
                printf("✅ KRB file reloaded\n");
            } else {
                printf("❌ Failed to reload KRB file\n");
            }
            
        } else if (strcmp(cmd, "state") == 0 || strcmp(cmd, "st") == 0) {
            printf("Global state inspection not yet implemented\n");
            
        } else if (strcmp(cmd, "events") == 0 || strcmp(cmd, "ev") == 0) {
            printf("Event history not yet implemented\n");
            
        } else if (strcmp(cmd, "break") == 0 || strcmp(cmd, "b") == 0) {
            printf("Breakpoints not yet implemented\n");
            
        } else if (strcmp(cmd, "profile") == 0 || strcmp(cmd, "pr") == 0) {
            printf("Performance profiling not yet implemented\n");
            
        } else {
            printf("Unknown command: %s\n", cmd);
            printf("Type 'help' for available commands.\n");
        }
        
        free(line);
    }
    
    printf("\n🛑 Shutting down debugger...\n");
    
    // Cleanup
    if (runtime_running) {
        kryon_runtime_stop(g_debug_runtime);
    }
    kryon_runtime_destroy(g_debug_runtime);
    kryon_platform_shutdown();
    
    printf("✅ Debugger shutdown complete\n");
    return 0;
}