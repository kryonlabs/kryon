/**

 * @file debug_command.c
 * @brief Kryon Debug Command Implementation
 */
#include "lib9.h"


#include "memory.h"
#include "runtime.h"
#include <getopt.h>

// Forward declarations
static void print_element_tree(const KryonElement *element, int indent);
static void print_indent(int indent);
static const char* get_property_value_string(const KryonProperty *prop);

int debug_command(int argc, char *argv[]) {
    if (argc < 2) {
        fprint(2, "Error: No KRB file specified\n");
        print("Usage: kryon debug <file.krb> [options]\n");
        print("Options:\n");
        print("  --tree    Show element tree structure (default)\n");
        print("  --help    Show this help\n");
        return 1;
    }
    
    const char *krb_file_path = argv[1];
    bool show_tree = true;  // Default action
    
    // Parse options (skip command and file argument)
    int opt_argc = argc - 2;
    char **opt_argv = argv + 2;
    
    static struct option long_options[] = {
        {"tree", no_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int c;
    optind = 0; // Reset getopt
    while ((c = getopt_long(opt_argc, opt_argv, "th", long_options, NULL)) != -1) {
        switch (c) {
            case 't':
                show_tree = true;
                break;
            case 'h':
                print("Usage: kryon debug <file.krb> [options]\n");
                print("Options:\n");
                print("  --tree    Show element tree structure (default)\n");
                print("  --help    Show this help\n");
                return 0;
            default:
                break;
        }
    }
    
    print("🔍 Kryon Debug: Analyzing KRB file: %s\n\n", krb_file_path);
    
    // Initialize memory manager if not already done
    if (!g_kryon_memory_manager) {
        KryonMemoryConfig config = {
            .initial_heap_size = 8 * 1024 * 1024,     // 8MB
            .max_heap_size = 256 * 1024 * 1024,       // 256MB max
            .enable_leak_detection = true,
            .enable_bounds_checking = false,
            .enable_statistics = false,
            .use_system_malloc = true,
            .large_alloc_threshold = 64 * 1024        // 64KB
        };
        
        g_kryon_memory_manager = kryon_memory_init(&config);
        if (!g_kryon_memory_manager) {
            fprint(2, "❌ Error: Failed to initialize memory manager\n");
            return 1;
        }
    }
    
    // Create runtime system
    KryonRuntime* runtime = kryon_runtime_create(NULL);
    if (!runtime) {
        fprint(2, "❌ Failed to create runtime system\n");
        return 1;
    }
    
    // Load KRB file into runtime
    if (!kryon_runtime_load_file(runtime, krb_file_path)) {
        fprint(2, "❌ Failed to load KRB file: %s\n", krb_file_path);
        kryon_runtime_destroy(runtime);
        return 1;
    }
    
    print("✅ KRB file loaded successfully\n\n");
    
    if (show_tree) {
        print("📋 Element Tree Structure:\n");
        print("═══════════════════════════\n");
        
        if (runtime->root) {
            print_element_tree(runtime->root, 0);
        } else {
            print("❌ No root element found\n");
        }
        
        print("\n📊 Summary:\n");
        print("───────────\n");
        print("Total elements: %zu\n", runtime->element_count);
    }
    
    // Cleanup
    kryon_runtime_destroy(runtime);
    
    return 0;
}

static void print_element_tree(const KryonElement *element, int indent) {
    if (!element) {
        return;
    }
    
    print_indent(indent);
    print("%s", element->type_name ? element->type_name : "Unknown");
    
    if (element->child_count > 0) {
        print(" (%zu children)", element->child_count);
    }
    
    // Show key properties
    for (size_t i = 0; i < element->property_count; i++) {
        const KryonProperty *prop = element->properties[i];
        if (!prop) continue;
        
        // Show important properties inline
        if (prop->name && (
            strcmp(prop->name, "text") == 0 ||
            strcmp(prop->name, "windowTitle") == 0 ||
            strcmp(prop->name, "mainAxisAlignment") == 0 ||
            strcmp(prop->name, "backgroundColor") == 0)) {
            
            const char *value = get_property_value_string(prop);
            if (value) {
                print(" [%s: %s]", prop->name, value);
            }
        }
    }
    
    print("\n");
    
    // Print children recursively
    for (size_t i = 0; i < element->child_count; i++) {
        if (i == element->child_count - 1) {
            // Last child - use └── 
            print_indent(indent);
            print("└── ");
            print_element_tree(element->children[i], indent + 1);
        } else {
            // Not last child - use ├──
            print_indent(indent);
            print("├── ");
            print_element_tree(element->children[i], indent + 1);
        }
    }
}

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        print("    ");
    }
}

static const char* get_property_value_string(const KryonProperty *prop) {
    static char buffer[256]; // Static buffer for formatting non-string values
    
    if (!prop) {
        return NULL;
    }
    
    switch (prop->type) {
        case KRYON_RUNTIME_PROP_STRING:
            return prop->value.string_value ? prop->value.string_value : "(null)";
        
        case KRYON_RUNTIME_PROP_INTEGER:
            snprint(buffer, sizeof(buffer), "%lld", (long long)prop->value.int_value);
            return buffer;
        
        case KRYON_RUNTIME_PROP_FLOAT:
            snprint(buffer, sizeof(buffer), "%.2f", prop->value.float_value);
            return buffer;
        
        case KRYON_RUNTIME_PROP_BOOLEAN:
            return prop->value.bool_value ? "true" : "false";
        
        case KRYON_RUNTIME_PROP_COLOR:
            snprint(buffer, sizeof(buffer), "#%08X", prop->value.color_value);
            return buffer;
        
        case KRYON_RUNTIME_PROP_REFERENCE:
            snprint(buffer, sizeof(buffer), "@%u", prop->value.ref_id);
            return buffer;
        
        case KRYON_RUNTIME_PROP_EXPRESSION:
            return "[expression]";
        
        case KRYON_RUNTIME_PROP_FUNCTION:
            return "[function]";
        
        default:
            return "[unknown]";
    }
}
