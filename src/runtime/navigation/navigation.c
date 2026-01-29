/**

 * @file navigation.c
 * @brief Navigation system implementation for Kryon Link elements.
 *
 * This file implements the navigation manager for handling internal
 * file navigation (.kry/.krb) and external URL handling with history support.
 *
 * 0BSD License
 */
#include "lib9.h"


#include "navigation.h"
#include "memory.h"
#include "../compilation/runtime_compiler.h"
#include "runtime.h"
#include "error.h"
#include <libgen.h>
#include <sys/stat.h>
#include <time.h>

// Default configuration values
#define KRYON_NAV_DEFAULT_MAX_HISTORY 50
#define KRYON_NAV_DEFAULT_MAX_CACHE 20
#define KRYON_NAV_CACHE_DIR "/tmp/kryon_cache"

// =============================================================================
//  Forward Declarations
// =============================================================================
static void inject_pending_overlay(KryonNavigationManager* nav_manager);

// =============================================================================
//  Private Helper Functions
// =============================================================================

static NavigationHistoryItem* create_history_item(const char* path, const char* title);
static void destroy_history_item(NavigationHistoryItem* item);
static void add_to_history(KryonNavigationManager* nav_manager, const char* path, const char* title);
static void cleanup_old_history(KryonNavigationManager* nav_manager);

static CompilationCacheEntry* find_cache_entry(KryonNavigationManager* nav_manager, const char* kry_path);
static void add_to_cache(KryonNavigationManager* nav_manager, const char* kry_path, const char* krb_path);
static void cleanup_old_cache(KryonNavigationManager* nav_manager);

static bool ends_with(const char* str, const char* suffix);
static char* get_cache_path(const char* kry_path);
static void ensure_cache_directory(void);
static void update_window_from_app_element(KryonRuntime* runtime);
static KryonComponentDefinition* deep_copy_component(const KryonComponentDefinition* source);
static void free_component_copy(KryonComponentDefinition* component);

// =============================================================================
//  Navigation Manager API Implementation
// =============================================================================

KryonNavigationManager* kryon_navigation_create(KryonRuntime* runtime) {
    KryonNavigationManager* nav_manager = kryon_alloc_type(KryonNavigationManager);
    if (!nav_manager) return NULL;
    
    nav_manager->current = NULL;
    nav_manager->head = NULL;
    nav_manager->tail = NULL;
    nav_manager->history_count = 0;
    nav_manager->max_history = KRYON_NAV_DEFAULT_MAX_HISTORY;
    
    nav_manager->cache_head = NULL;
    nav_manager->cache_count = 0;
    nav_manager->max_cache = KRYON_NAV_DEFAULT_MAX_CACHE;
    
    nav_manager->runtime = runtime;
    nav_manager->pending_overlay = NULL;
    
    // Initialize runtime compiler for .kry file compilation
    if (!kryon_runtime_compiler_init()) {
        print("⚠️  Failed to initialize runtime compiler\n");
        kryon_free(nav_manager);
        return NULL;
    }
    
    // Ensure cache directory exists
    ensure_cache_directory();
    
    print("🧭 Navigation manager created\n");
    return nav_manager;
}

void kryon_navigation_destroy(KryonNavigationManager* nav_manager) {
    if (!nav_manager) return;
    
    // Clear history
    NavigationHistoryItem* item = nav_manager->head;
    while (item) {
        NavigationHistoryItem* next = item->next;
        destroy_history_item(item);
        item = next;
    }
    
    // Clear cache
    CompilationCacheEntry* cache_item = nav_manager->cache_head;
    while (cache_item) {
        CompilationCacheEntry* next = cache_item->next;
        kryon_free(cache_item->kry_path);
        kryon_free(cache_item->krb_path);
        kryon_free(cache_item);
        cache_item = next;
    }
    
    // Clean up pending overlay
    if (nav_manager->pending_overlay) {
        if (nav_manager->pending_overlay->component_name) {
            kryon_free(nav_manager->pending_overlay->component_name);
        }
        if (nav_manager->pending_overlay->component_def) {
            free_component_copy(nav_manager->pending_overlay->component_def);
        }
        kryon_free(nav_manager->pending_overlay);
    }
    
    // Shutdown runtime compiler
    kryon_runtime_compiler_shutdown();
    
    kryon_free(nav_manager);
    print("🧭 Navigation manager destroyed\n");
}

KryonNavigationResult kryon_navigate_to(KryonNavigationManager* nav_manager, const char* target, bool external) {
    if (!nav_manager || !target || strlen(target) == 0) {
        return KRYON_NAV_ERROR_INVALID_PATH;
    }
    
    print("🔗 Navigating to: %s %s\n", target, external ? "(external)" : "(internal)");
    
    // Debug current navigation state
    if (nav_manager->current && nav_manager->current->path) {
        print("🧭 Current navigation path: %s\n", nav_manager->current->path);
    } else {
        print("🧭 No current navigation path set!\n");
    }
    
    if (external || kryon_navigation_is_external_url(target)) {
        return kryon_navigation_open_external(target);
    }
    
    // Handle internal navigation - resolve relative paths
    char* resolved_path = NULL;
    if (target[0] == '/') {
        // Absolute path - use as-is
        resolved_path = kryon_strdup(target);
        print("🔗 Using absolute path: %s\n", resolved_path);
    } else {
        // Relative path - resolve based on current location
        if (nav_manager->current && nav_manager->current->path) {
            // Get directory of current file
            char* current_path_copy = kryon_strdup(nav_manager->current->path);
            char* current_dir = dirname(current_path_copy);
            
            print("🔗 Current file directory: %s\n", current_dir);
            
            // Construct resolved path
            size_t path_len = strlen(current_dir) + strlen(target) + 2; // +2 for '/' and '\0'
            resolved_path = kryon_alloc(path_len);
            if (resolved_path) {
                snprint(resolved_path, path_len, "%s/%s", current_dir, target);
            }
            kryon_free(current_path_copy);
        } else {
            // No current location - use target as-is
            print("🔗 No current location, using target as-is\n");
            resolved_path = kryon_strdup(target);
        }
    }
    
    if (!resolved_path) {
        return KRYON_NAV_ERROR_MEMORY;
    }
    
    print("🔗 Resolved path: %s\n", resolved_path);
    
    KryonNavigationResult result;
    
    if (ends_with(resolved_path, ".krb")) {
        result = kryon_navigation_load_krb(nav_manager, resolved_path);
    } else if (ends_with(resolved_path, ".kry")) {
        result = kryon_navigation_compile_and_load(nav_manager, resolved_path);
    } else {
        print("❓ Unknown file type: %s\n", resolved_path);
        kryon_free(resolved_path);
        return KRYON_NAV_ERROR_INVALID_PATH;
    }
    
    // Add to history on successful navigation
    if (result == KRYON_NAV_SUCCESS) {
        // Inject overlay ONCE after successful navigation
        inject_pending_overlay(nav_manager);
        add_to_history(nav_manager, resolved_path, NULL);
        print("✅ Link navigation successful\n");
    }
    
    kryon_free(resolved_path);
    return result;
}

KryonNavigationResult kryon_navigate_back(KryonNavigationManager* nav_manager) {
    if (!nav_manager || !nav_manager->current || !nav_manager->current->prev) {
        return KRYON_NAV_ERROR_NO_HISTORY;
    }
    
    nav_manager->current = nav_manager->current->prev;
    print("⬅️  Navigating back to: %s\n", nav_manager->current->path);
    
    // Navigate to the previous item without adding to history
    if (ends_with(nav_manager->current->path, ".krb")) {
        return kryon_navigation_load_krb(nav_manager, nav_manager->current->path);
    } else if (ends_with(nav_manager->current->path, ".kry")) {
        return kryon_navigation_compile_and_load(nav_manager, nav_manager->current->path);
    }
    
    return KRYON_NAV_ERROR_INVALID_PATH;
}

KryonNavigationResult kryon_navigate_forward(KryonNavigationManager* nav_manager) {
    if (!nav_manager || !nav_manager->current || !nav_manager->current->next) {
        return KRYON_NAV_ERROR_NO_HISTORY;
    }
    
    nav_manager->current = nav_manager->current->next;
    print("➡️  Navigating forward to: %s\n", nav_manager->current->path);
    
    // Navigate to the next item without adding to history
    if (ends_with(nav_manager->current->path, ".krb")) {
        return kryon_navigation_load_krb(nav_manager, nav_manager->current->path);
    } else if (ends_with(nav_manager->current->path, ".kry")) {
        return kryon_navigation_compile_and_load(nav_manager, nav_manager->current->path);
    }
    
    return KRYON_NAV_ERROR_INVALID_PATH;
}

NavigationHistoryItem* kryon_navigation_get_current(KryonNavigationManager* nav_manager) {
    return nav_manager ? nav_manager->current : NULL;
}

void kryon_navigation_set_current_path(KryonNavigationManager* nav_manager, const char* path) {
    if (!nav_manager || !path) return;
    
    // Set initial current path without adding to history
    if (!nav_manager->current) {
        nav_manager->current = kryon_alloc_type(NavigationHistoryItem);
        if (nav_manager->current) {
            nav_manager->current->path = kryon_strdup(path);
            nav_manager->current->title = NULL; // Will be set later if needed
            nav_manager->current->next = NULL;
            nav_manager->current->prev = NULL;
            print("🧭 Set initial navigation path: %s\n", path);
        }
    } else {
        // Update existing current path
        kryon_free(nav_manager->current->path);
        nav_manager->current->path = kryon_strdup(path);
        print("🧭 Updated navigation path: %s\n", path);
    }
}

void kryon_navigation_set_overlay(KryonNavigationManager* nav_manager, const char* component_name, KryonRuntime* source_runtime) {
    if (!nav_manager || !component_name || !source_runtime) {
        return;
    }
    
    // Find the component in the source runtime
    KryonComponentDefinition* source_component = NULL;
    if (source_runtime->components) {
        for (size_t i = 0; i < source_runtime->component_count; i++) {
            KryonComponentDefinition* comp = source_runtime->components[i];
            if (comp && comp->name && strcmp(comp->name, component_name) == 0) {
                source_component = comp;
                print("✅ Found source component for overlay: %s\n", comp->name);
                break;
            }
        }
    }
    
    if (!source_component) {
        print("⚠️  Component '%s' not found in source runtime for overlay\n", component_name);
        return;
    }
    
    // Clean up any existing overlay
    if (nav_manager->pending_overlay) {
        if (nav_manager->pending_overlay->component_name) {
            kryon_free(nav_manager->pending_overlay->component_name);
        }
        if (nav_manager->pending_overlay->component_def) {
            free_component_copy(nav_manager->pending_overlay->component_def);
        }
        kryon_free(nav_manager->pending_overlay);
    }
    
    // Create new overlay information with deep copy
    nav_manager->pending_overlay = kryon_alloc_type(KryonNavigationOverlay);
    if (nav_manager->pending_overlay) {
        nav_manager->pending_overlay->component_name = kryon_strdup(component_name);
        nav_manager->pending_overlay->component_def = deep_copy_component(source_component);
        
        if (nav_manager->pending_overlay->component_def) {
            print("🔀 Overlay set for next navigation with deep copy: %s\n", component_name);
        } else {
            print("⚠️  Failed to create deep copy of component for overlay\n");
            kryon_free(nav_manager->pending_overlay->component_name);
            kryon_free(nav_manager->pending_overlay);
            nav_manager->pending_overlay = NULL;
        }
    }
}

bool kryon_navigation_can_go_back(KryonNavigationManager* nav_manager) {
    return nav_manager && nav_manager->current && nav_manager->current->prev;
}

bool kryon_navigation_can_go_forward(KryonNavigationManager* nav_manager) {
    return nav_manager && nav_manager->current && nav_manager->current->next;
}

// =============================================================================
//  File Navigation API Implementation
// =============================================================================

KryonNavigationResult kryon_navigation_load_krb(KryonNavigationManager* nav_manager, const char* krb_path) {
    if (!nav_manager || !krb_path) {
        return KRYON_NAV_ERROR_INVALID_PATH;
    }
    
    if (!kryon_navigation_file_exists(krb_path)) {
        print("❌ KRB file not found: %s\n", krb_path);
        return KRYON_NAV_ERROR_FILE_NOT_FOUND;
    }
    
    print("📄 Loading KRB file: %s\n", krb_path);
    
    // Clear all existing content before loading new file
    kryon_runtime_clear_all_content(nav_manager->runtime);
    
    // Load KRB file into runtime
    if (!kryon_runtime_load_file(nav_manager->runtime, krb_path)) {
        print("❌ Failed to load KRB file: %s\n", krb_path);
        return KRYON_NAV_ERROR_FILE_NOT_FOUND;
    }
    
    print("✅ KRB file loaded successfully: %s\n", krb_path);
    
    return KRYON_NAV_SUCCESS;
}

// Helper function to create a property for runtime element
static KryonProperty* create_string_property(const char* name, const char* value) {
    KryonProperty* prop = kryon_malloc(sizeof(KryonProperty));
    if (!prop) return NULL;
    
    memset(prop, 0, sizeof(KryonProperty));
    prop->id = 0; // Will be set if needed
    prop->name = kryon_strdup(name);
    prop->type = KRYON_RUNTIME_PROP_STRING;
    prop->value.string_value = kryon_strdup(value);
    
    if (!prop->name || !prop->value.string_value) {
        kryon_free(prop->name);
        kryon_free(prop->value.string_value);
        kryon_free(prop);
        return NULL;
    }
    
    return prop;
}

static KryonProperty* create_color_property(const char* name, const char* hex_color) {
    KryonProperty* prop = kryon_malloc(sizeof(KryonProperty));
    if (!prop) return NULL;
    
    memset(prop, 0, sizeof(KryonProperty));
    prop->id = 0;
    prop->name = kryon_strdup(name);
    prop->type = KRYON_RUNTIME_PROP_COLOR;
    
    // Parse hex color like "#404080" to uint32
    uint32_t color = 0x404080FF; // Default color with full alpha
    if (hex_color && hex_color[0] == '#') {
        sscanf(hex_color + 1, "%6x", &color);
        color = (color << 8) | 0xFF; // Add full alpha
    }
    prop->value.color_value = color;
    
    if (!prop->name) {
        kryon_free(prop);
        return NULL;
    }
    
    return prop;
}

static KryonProperty* create_float_property(const char* name, float value) {
    KryonProperty* prop = kryon_malloc(sizeof(KryonProperty));
    if (!prop) return NULL;
    
    memset(prop, 0, sizeof(KryonProperty));
    prop->id = 0;
    prop->name = kryon_strdup(name);
    prop->type = KRYON_RUNTIME_PROP_FLOAT;
    prop->value.float_value = value;
    
    if (!prop->name) {
        kryon_free(prop);
        return NULL;
    }
    
    return prop;
}

// New function to inject overlay after successful file load
static void inject_pending_overlay(KryonNavigationManager* nav_manager) {
    if (!nav_manager->pending_overlay || !nav_manager->pending_overlay->component_def) {
        return;
    }
    
    print("🔀 Injecting overlay component: %s\n", nav_manager->pending_overlay->component_name);
    
    KryonComponentDefinition* overlay_component = nav_manager->pending_overlay->component_def;
    
    if (!overlay_component || !nav_manager->runtime || !nav_manager->runtime->root) {
        print("⚠️  Invalid overlay component or runtime state\n");
        return;
    }
    
    print("🔍 Overlay component found: name='%s'\n", 
           overlay_component->name ? overlay_component->name : "null");
    
    print("🔀 Creating Button element directly for BackButton overlay\n");
    print("🔍 DEBUG: Before Button creation, root has %zu children\n", nav_manager->runtime->root->child_count);
    // Create element WITHOUT parent to avoid automatic parent addition
    KryonElement* element = kryon_element_create(nav_manager->runtime, 0x0401, NULL);
    if (!element) {
        print("⚠️  Failed to create Button element for overlay\n");
        return;
    }
    print("🔍 DEBUG: Button element created with ID=%u\n", element->id);
    
    // Set the type name to Button
    element->type_name = kryon_strdup("Button");
    
    // Add Button properties directly (BackButton definition)
    element->width = 150.0f;
    element->height = 40.0f;
    element->visible = true;
    
    // Allocate properties array for the Button
    element->property_capacity = 8; // text, posX, posY, backgroundColor, color, borderRadius, zIndex, onClick
    element->properties = kryon_malloc(element->property_capacity * sizeof(KryonProperty*));
    element->property_count = 0;
    
    if (!element->properties) {
        print("⚠️  Failed to allocate properties array for Button\n");
        kryon_element_destroy(nav_manager->runtime, element);
        return;
    }
    
    // Create and add Button properties (matching BackButton component)
    element->properties[element->property_count++] = create_string_property("text", "<-- Back to Examples");
    element->properties[element->property_count++] = create_float_property("posX", 20.0f);
    element->properties[element->property_count++] = create_float_property("posY", 20.0f);
    element->properties[element->property_count++] = create_color_property("backgroundColor", "#404080");
    element->properties[element->property_count++] = create_color_property("color", "#FFFFFF");
    element->properties[element->property_count++] = create_float_property("borderRadius", 8.0f);
    element->properties[element->property_count++] = create_float_property("zIndex", 1000.0f);
    element->properties[element->property_count++] = create_string_property("onClick", "navigateBack");
    
    // Check if any property creation failed
    for (size_t i = 0; i < element->property_count; i++) {
        if (!element->properties[i]) {
            print("⚠️  Failed to create property %zu for Button\n", i);
            kryon_element_destroy(nav_manager->runtime, element);
            return;
        }
    }
    
    print("🔀 Button properties created: %zu properties (text, colors, styling)\n", element->property_count);
    
    print("🔀 Button element configured: %dx%d with posX=20, posY=20\n", 
           (int)element->width, (int)element->height);
    
    // Add to root element as child
    if (nav_manager->runtime->root->child_count >= nav_manager->runtime->root->child_capacity) {
        size_t new_capacity = nav_manager->runtime->root->child_capacity == 0 ? 4 : nav_manager->runtime->root->child_capacity * 2;
        KryonElement** new_children = kryon_realloc(nav_manager->runtime->root->children, 
                                                  new_capacity * sizeof(KryonElement*));
        if (new_children) {
            nav_manager->runtime->root->children = new_children;
            nav_manager->runtime->root->child_capacity = new_capacity;
        }
    }
    
    print("🔍 DEBUG: About to inject Button into root. Current child_count=%zu, capacity=%zu\n", 
           nav_manager->runtime->root->child_count, nav_manager->runtime->root->child_capacity);
    
    if (nav_manager->runtime->root->child_count < nav_manager->runtime->root->child_capacity) {
        size_t injection_index = nav_manager->runtime->root->child_count;
        nav_manager->runtime->root->children[injection_index] = element;
        nav_manager->runtime->root->child_count++;
        element->parent = nav_manager->runtime->root;
        nav_manager->runtime->root->needs_render = true;
        
        print("✅ Overlay Button element injected at index %zu (ID=%u)\n", injection_index, element->id);
        
        // Debug: Show DETAILED element tree structure after injection
        print("🔍 DEBUG: Root element now has %zu children:\n", nav_manager->runtime->root->child_count);
        for (size_t i = 0; i < nav_manager->runtime->root->child_count; i++) {
            KryonElement* child = nav_manager->runtime->root->children[i];
            print("  [%zu] ID=%u %s (type=0x%04X) ptr=%p\n", i, 
                   child->id, 
                   child->type_name ? child->type_name : "unknown",
                   child->type,
                   (void*)child);
        }
        
        // CRITICAL: Validate element tree integrity after injection
        print("🔍 VALIDATION: Checking element tree integrity after overlay injection\n");
        
        // Validate root element
        if (!nav_manager->runtime->root) {
            print("❌ VALIDATION: Root element is NULL after injection!\n");
        } else if (!nav_manager->runtime->root->type_name) {
            print("❌ VALIDATION: Root element type_name is NULL after injection!\n");
        } else {
            print("✅ VALIDATION: Root element OK: %s\n", nav_manager->runtime->root->type_name);
        }
        
        // Validate all children
        for (size_t i = 0; i < nav_manager->runtime->root->child_count; i++) {
            KryonElement* child = nav_manager->runtime->root->children[i];
            if (!child) {
                print("❌ VALIDATION: Child %zu is NULL!\n", i);
                continue;
            }
            if (!child->type_name) {
                print("❌ VALIDATION: Child %zu type_name is NULL!\n", i);
                continue;
            }
            if (child->property_count > 0 && !child->properties) {
                print("❌ VALIDATION: Child %zu (%s) has properties count %zu but NULL array!\n", 
                       i, child->type_name, child->property_count);
                continue;
            }
            if (child->child_count > 0 && !child->children) {
                print("❌ VALIDATION: Child %zu (%s) has child_count %zu but NULL array!\n", 
                       i, child->type_name, child->child_count);
                continue;
            }
            print("✅ VALIDATION: Child %zu (%s) is valid\n", i, child->type_name);
        }
    } else {
        print("⚠️  Cannot inject overlay: children array full\n");
        kryon_element_destroy(nav_manager->runtime, element);
    }
    
    // Clear the pending overlay IMMEDIATELY after injection
    if (nav_manager->pending_overlay->component_name) {
        kryon_free(nav_manager->pending_overlay->component_name);
    }
    if (nav_manager->pending_overlay->component_def) {
        free_component_copy(nav_manager->pending_overlay->component_def);
    }
    kryon_free(nav_manager->pending_overlay);
    nav_manager->pending_overlay = NULL;
    
    // Calculate layout positions after overlay injection
    print("🧮 Navigation: Calculating layout after overlay injection\n");
    kryon_runtime_calculate_layout(nav_manager->runtime);
    
    // Update window properties from new App element
    update_window_from_app_element(nav_manager->runtime);
}

KryonNavigationResult kryon_navigation_compile_and_load(KryonNavigationManager* nav_manager, const char* kry_path) {
    if (!nav_manager || !kry_path) {
        return KRYON_NAV_ERROR_INVALID_PATH;
    }
    
    if (!kryon_navigation_file_exists(kry_path)) {
        print("❌ KRY file not found: %s\n", kry_path);
        return KRYON_NAV_ERROR_FILE_NOT_FOUND;
    }
    
    print("⚡ Compiling KRY file: %s\n", kry_path);
    
    char* compiled_krb_path = NULL;
    KryonNavigationResult result = kryon_compile_kry_to_temp(nav_manager, kry_path, &compiled_krb_path);
    
    if (result != KRYON_NAV_SUCCESS) {
        return result;
    }
    
    // Load the compiled KRB file
    result = kryon_navigation_load_krb(nav_manager, compiled_krb_path);
    kryon_free(compiled_krb_path);
    
    return result;
}

KryonNavigationResult kryon_navigation_open_external(const char* url) {
    if (!url || strlen(url) == 0) {
        return KRYON_NAV_ERROR_INVALID_PATH;
    }
    
    print("🌐 Opening external URL: %s\n", url);
    
    #ifdef __linux__
        char command[2048];
        snprint(command, sizeof(command), "xdg-open '%s' 2>/dev/null &", url);
        int result = system(command);
        if (result == 0) {
            print("✅ External URL opened successfully\n");
            return KRYON_NAV_SUCCESS;
        } else {
            print("❌ Failed to open external URL\n");
            return KRYON_NAV_ERROR_EXTERNAL_FAILED;
        }
    #elif __APPLE__
        char command[2048];
        snprint(command, sizeof(command), "open '%s' &", url);
        int result = system(command);
        if (result == 0) {
            print("✅ External URL opened successfully\n");
            return KRYON_NAV_SUCCESS;
        } else {
            print("❌ Failed to open external URL\n");
            return KRYON_NAV_ERROR_EXTERNAL_FAILED;
        }
    #elif _WIN32
        // TODO: Implement Windows support with ShellExecuteA
        print("🌐 External URL (Windows support coming soon): %s\n", url);
        return KRYON_NAV_SUCCESS;
    #else
        print("🌐 External URL (Platform not supported): %s\n", url);
        return KRYON_NAV_ERROR_EXTERNAL_FAILED;
    #endif
}

// =============================================================================
//  Compilation Cache API Implementation
// =============================================================================

KryonNavigationResult kryon_compile_kry_to_temp(KryonNavigationManager* nav_manager, const char* kry_path, char** output_krb_path) {
    if (!nav_manager || !kry_path || !output_krb_path) {
        return KRYON_NAV_ERROR_INVALID_PATH;
    }
    
    // Check cache first
    CompilationCacheEntry* cached = find_cache_entry(nav_manager, kry_path);
    time_t kry_mtime = kryon_navigation_file_mtime(kry_path);
    
    if (cached && cached->krb_mtime >= kry_mtime && kryon_navigation_file_exists(cached->krb_path)) {
        print("⚡ Using cached compilation: %s\n", cached->krb_path);
        *output_krb_path = kryon_strdup(cached->krb_path);
        return KRYON_NAV_SUCCESS;
    }
    
    // Generate cache file path
    char* cache_path = get_cache_path(kry_path);
    if (!cache_path) {
        return KRYON_NAV_ERROR_MEMORY;
    }
    
    print("⚡ Compiling %s -> %s\n", kry_path, cache_path);
    
    // Use the runtime compiler to compile the .kry file
    KryonResult compile_result = kryon_compile_file(kry_path, cache_path, NULL);
    if (compile_result != KRYON_SUCCESS) {
        // The error message now comes from the unified error handler
        print("❌ Compilation failed: %s\n", kryon_error_get_message(compile_result));
        kryon_free(cache_path);
        return KRYON_NAV_ERROR_COMPILATION_FAILED;
    }
    
    // Add to cache
    add_to_cache(nav_manager, kry_path, cache_path);
    
    *output_krb_path = cache_path;
    print("✅ Compilation successful: %s\n", cache_path);
    return KRYON_NAV_SUCCESS;
}

void kryon_navigation_clear_cache(KryonNavigationManager* nav_manager) {
    if (!nav_manager) return;
    
    CompilationCacheEntry* cache_item = nav_manager->cache_head;
    while (cache_item) {
        CompilationCacheEntry* next = cache_item->next;
        
        // Remove cached file
        unlink(cache_item->krb_path);
        
        kryon_free(cache_item->kry_path);
        kryon_free(cache_item->krb_path);
        kryon_free(cache_item);
        cache_item = next;
    }
    
    nav_manager->cache_head = NULL;
    nav_manager->cache_count = 0;
    
    print("🧹 Compilation cache cleared\n");
}

// =============================================================================
//  Window Update Implementation
// =============================================================================

static void update_window_from_app_element(KryonRuntime* runtime) {
    if (!runtime || !runtime->root) {
        return;
    }
    
    // Check if root element is an App element
    if (!runtime->root->type_name || strcmp(runtime->root->type_name, "App") != 0) {
        return;
    }
    
    print("🪟 Checking window properties from new App element...\n");
    
    // Get window properties from App element
    float window_width = get_element_property_float(runtime->root, "windowWidth", 800.0f);
    float window_height = get_element_property_float(runtime->root, "windowHeight", 600.0f);
    const char* window_title = get_element_property_string(runtime->root, "windowTitle");
    
    print("🪟 New window properties: %gx%g, title: '%s'\n", 
           window_width, window_height, window_title ? window_title : "null");
    
    // Get renderer from runtime (assuming it's available)
    KryonRenderer* renderer = runtime->renderer;
    if (!renderer) {
        print("⚠️  No renderer available for window updates\n");
        return;
    }
    
    // Update window size if different using renderer abstraction
    if (window_width != 800.0f || window_height != 600.0f) {
        print("🪟 Updating window size to %gx%g\n", window_width, window_height);
        KryonRenderResult result = kryon_renderer_update_window_size(
            renderer, (int)window_width, (int)window_height);
        if (result != KRYON_RENDER_SUCCESS) {
            print("⚠️  Failed to update window size\n");
        }
    }
    
    // Update window title if provided using renderer abstraction
    if (window_title && strlen(window_title) > 0) {
        print("🪟 Updating window title to: '%s'\n", window_title);
        KryonRenderResult result = kryon_renderer_update_window_title(renderer, window_title);
        if (result != KRYON_RENDER_SUCCESS) {
            print("⚠️  Failed to update window title\n");
        }
    }
}

// =============================================================================
//  Utility Functions Implementation
// =============================================================================

bool kryon_navigation_is_external_url(const char* path) {
    if (!path) return false;
    
    return (strncmp(path, "http://", 7) == 0 ||
            strncmp(path, "https://", 8) == 0 ||
            strncmp(path, "mailto:", 7) == 0 ||
            strncmp(path, "file://", 7) == 0 ||
            strncmp(path, "ftp://", 6) == 0);
}

bool kryon_navigation_file_exists(const char* path) {
    if (!path) return false;
    
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

time_t kryon_navigation_file_mtime(const char* path) {
    if (!path) return 0;
    
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

const char* kryon_navigation_result_string(KryonNavigationResult result) {
    switch (result) {
        case KRYON_NAV_SUCCESS: return "Success";
        case KRYON_NAV_ERROR_FILE_NOT_FOUND: return "File not found";
        case KRYON_NAV_ERROR_COMPILATION_FAILED: return "Compilation failed";
        case KRYON_NAV_ERROR_INVALID_PATH: return "Invalid path";
        case KRYON_NAV_ERROR_EXTERNAL_FAILED: return "External URL failed";
        case KRYON_NAV_ERROR_MEMORY: return "Memory allocation failed";
        case KRYON_NAV_ERROR_NO_HISTORY: return "No navigation history";
        default: return "Unknown error";
    }
}

// =============================================================================
//  Private Helper Functions Implementation
// =============================================================================

static NavigationHistoryItem* create_history_item(const char* path, const char* title) {
    NavigationHistoryItem* item = kryon_alloc_type(NavigationHistoryItem);
    if (!item) return NULL;
    
    item->path = kryon_strdup(path);
    item->title = title ? kryon_strdup(title) : NULL;
    item->timestamp = time(NULL);
    item->next = NULL;
    item->prev = NULL;
    
    return item;
}

static void destroy_history_item(NavigationHistoryItem* item) {
    if (!item) return;
    
    kryon_free(item->path);
    kryon_free(item->title);
    kryon_free(item);
}

static void add_to_history(KryonNavigationManager* nav_manager, const char* path, const char* title) {
    if (!nav_manager || !path) return;
    
    NavigationHistoryItem* item = create_history_item(path, title);
    if (!item) return;
    
    // If we're not at the end of history, remove forward history
    if (nav_manager->current && nav_manager->current->next) {
        NavigationHistoryItem* to_remove = nav_manager->current->next;
        while (to_remove) {
            NavigationHistoryItem* next = to_remove->next;
            destroy_history_item(to_remove);
            nav_manager->history_count--;
            to_remove = next;
        }
        nav_manager->current->next = NULL;
        nav_manager->tail = nav_manager->current;
    }
    
    // Add new item
    if (nav_manager->tail) {
        nav_manager->tail->next = item;
        item->prev = nav_manager->tail;
        nav_manager->tail = item;
    } else {
        nav_manager->head = nav_manager->tail = item;
    }
    
    nav_manager->current = item;
    nav_manager->history_count++;
    
    cleanup_old_history(nav_manager);
}

static void cleanup_old_history(KryonNavigationManager* nav_manager) {
    while (nav_manager->history_count > nav_manager->max_history && nav_manager->head) {
        NavigationHistoryItem* to_remove = nav_manager->head;
        nav_manager->head = to_remove->next;
        if (nav_manager->head) {
            nav_manager->head->prev = NULL;
        } else {
            nav_manager->tail = NULL;
        }
        destroy_history_item(to_remove);
        nav_manager->history_count--;
    }
}

static CompilationCacheEntry* find_cache_entry(KryonNavigationManager* nav_manager, const char* kry_path) {
    if (!nav_manager || !kry_path) return NULL;
    
    CompilationCacheEntry* entry = nav_manager->cache_head;
    while (entry) {
        if (strcmp(entry->kry_path, kry_path) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

static void add_to_cache(KryonNavigationManager* nav_manager, const char* kry_path, const char* krb_path) {
    if (!nav_manager || !kry_path || !krb_path) return;
    
    CompilationCacheEntry* entry = kryon_alloc_type(CompilationCacheEntry);
    if (!entry) return;
    
    entry->kry_path = kryon_strdup(kry_path);
    entry->krb_path = kryon_strdup(krb_path);
    entry->kry_mtime = kryon_navigation_file_mtime(kry_path);
    entry->krb_mtime = kryon_navigation_file_mtime(krb_path);
    entry->next = nav_manager->cache_head;
    
    nav_manager->cache_head = entry;
    nav_manager->cache_count++;
    
    cleanup_old_cache(nav_manager);
}
    
static bool ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    
    if (suffix_len > str_len) return false;
    
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static char* get_cache_path(const char* kry_path) {
    if (!kry_path) return NULL;
    
    // Extract filename without path and extension
    const char* filename = strrchr(kry_path, '/');
    if (!filename) filename = kry_path;
    else filename++; // Skip the '/'
    
    // Create cache path
    char* cache_path = kryon_alloc(strlen(KRYON_NAV_CACHE_DIR) + strlen(filename) + 10);
    if (!cache_path) return NULL;
    
    char* base_name = kryon_strdup(filename);
    char* dot = strrchr(base_name, '.');
    if (dot) *dot = '\0'; // Remove extension
    
    snprint(cache_path, strlen(KRYON_NAV_CACHE_DIR) + strlen(filename) + 10, 
             "%s/%s.krb", KRYON_NAV_CACHE_DIR, base_name);
    
    kryon_free(base_name);
    return cache_path;
}

static void ensure_cache_directory(void) {
    struct stat st = {0};
    if (stat(KRYON_NAV_CACHE_DIR, &st) == -1) {
        mkdir(KRYON_NAV_CACHE_DIR, 0755);
    }
}

static void cleanup_old_cache(KryonNavigationManager* nav_manager) {
    // Simple cleanup - remove oldest entries
    while (nav_manager->cache_count > nav_manager->max_cache && nav_manager->cache_head) {
        CompilationCacheEntry* entry = nav_manager->cache_head;
        CompilationCacheEntry* prev = NULL;
        
        // Find the last entry
        while (entry && entry->next) {
            prev = entry;
            entry = entry->next;
        }
        
        if (entry) {
            if (prev) {
                prev->next = NULL;
            } else {
                nav_manager->cache_head = NULL;
            }
            
            // Remove cached file if it exists
            if (entry->krb_path) {
                unlink(entry->krb_path);
            }
            
            // Free memory safely with null checks
            if (entry->kry_path) kryon_free(entry->kry_path);
            if (entry->krb_path) kryon_free(entry->krb_path);
            kryon_free(entry);
            nav_manager->cache_count--;
        }
    }
}

// =============================================================================
//  Component Deep Copy Implementation
// =============================================================================

static KryonComponentDefinition* deep_copy_component(const KryonComponentDefinition* source) {
    if (!source) {
        return NULL;
    }
    
    KryonComponentDefinition* copy = kryon_alloc_type(KryonComponentDefinition);
    if (!copy) {
        return NULL;
    }
    
    // Copy name
    copy->name = source->name ? kryon_strdup(source->name) : NULL;
    
    // Copy parameter info
    copy->parameter_count = source->parameter_count;
    if (source->parameter_count > 0 && source->parameters) {
        copy->parameters = kryon_alloc(source->parameter_count * sizeof(char*));
        if (copy->parameters) {
            for (size_t i = 0; i < source->parameter_count; i++) {
                copy->parameters[i] = source->parameters[i] ? kryon_strdup(source->parameters[i]) : NULL;
            }
        }
        
        if (source->param_defaults) {
            copy->param_defaults = kryon_alloc(source->parameter_count * sizeof(char*));
            if (copy->param_defaults) {
                for (size_t i = 0; i < source->parameter_count; i++) {
                    copy->param_defaults[i] = source->param_defaults[i] ? kryon_strdup(source->param_defaults[i]) : NULL;
                }
            }
        }
    } else {
        copy->parameters = NULL;
        copy->param_defaults = NULL;
    }
    
    // Copy state variables
    copy->state_count = source->state_count;
    if (source->state_count > 0 && source->state_vars) {
        copy->state_vars = kryon_alloc(source->state_count * sizeof(KryonComponentStateVar));
        if (copy->state_vars) {
            memcpy(copy->state_vars, source->state_vars, source->state_count * sizeof(KryonComponentStateVar));
        }
    } else {
        copy->state_vars = NULL;
    }
    
    // Copy functions
    copy->function_count = source->function_count;
    if (source->function_count > 0 && source->functions) {
        copy->functions = kryon_alloc(source->function_count * sizeof(KryonComponentFunction));
        if (copy->functions) {
            memcpy(copy->functions, source->functions, source->function_count * sizeof(KryonComponentFunction));
        }
    } else {
        copy->functions = NULL;
    }
    
    // Copy UI template (this is the important part for overlay injection)
    copy->ui_template = source->ui_template; // Shallow copy for now - elements are shared
    
    print("🔀 Deep copied component: %s\n", copy->name ? copy->name : "unnamed");
    return copy;
}

static void free_component_copy(KryonComponentDefinition* component) {
    if (!component) {
        return;
    }
    
    // Free name
    if (component->name) {
        kryon_free(component->name);
    }
    
    // Free parameters
    if (component->parameters) {
        for (size_t i = 0; i < component->parameter_count; i++) {
            if (component->parameters[i]) {
                kryon_free(component->parameters[i]);
            }
        }
        kryon_free(component->parameters);
    }
    
    if (component->param_defaults) {
        for (size_t i = 0; i < component->parameter_count; i++) {
            if (component->param_defaults[i]) {
                kryon_free(component->param_defaults[i]);
            }
        }
        kryon_free(component->param_defaults);
    }
    
    // Free state variables
    if (component->state_vars) {
        kryon_free(component->state_vars);
    }
    
    // Free functions
    if (component->functions) {
        kryon_free(component->functions);
    }
    
    // Note: We don't free ui_template as it's a shallow copy
    
    kryon_free(component);
}
