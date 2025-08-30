/**
 * @file navigation.c
 * @brief Navigation system implementation for Kryon Link elements.
 *
 * This file implements the navigation manager for handling internal
 * file navigation (.kry/.krb) and external URL handling with history support.
 *
 * 0BSD License
 */

#include "navigation.h"
#include "memory.h"
#include "../compilation/runtime_compiler.h"
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// Default configuration values
#define KRYON_NAV_DEFAULT_MAX_HISTORY 50
#define KRYON_NAV_DEFAULT_MAX_CACHE 20
#define KRYON_NAV_CACHE_DIR "/tmp/kryon_cache"

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
    
    // Initialize runtime compiler for .kry file compilation
    if (!kryon_runtime_compiler_init()) {
        printf("⚠️  Failed to initialize runtime compiler\n");
        kryon_free(nav_manager);
        return NULL;
    }
    
    // Ensure cache directory exists
    ensure_cache_directory();
    
    printf("🧭 Navigation manager created\n");
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
    
    // Shutdown runtime compiler
    kryon_runtime_compiler_shutdown();
    
    kryon_free(nav_manager);
    printf("🧭 Navigation manager destroyed\n");
}

KryonNavigationResult kryon_navigate_to(KryonNavigationManager* nav_manager, const char* target, bool external) {
    if (!nav_manager || !target || strlen(target) == 0) {
        return KRYON_NAV_ERROR_INVALID_PATH;
    }
    
    printf("🔗 Navigating to: %s %s\n", target, external ? "(external)" : "(internal)");
    
    // Debug current navigation state
    if (nav_manager->current && nav_manager->current->path) {
        printf("🧭 Current navigation path: %s\n", nav_manager->current->path);
    } else {
        printf("🧭 No current navigation path set!\n");
    }
    
    if (external || kryon_navigation_is_external_url(target)) {
        return kryon_navigation_open_external(target);
    }
    
    // Handle internal navigation - resolve relative paths
    char* resolved_path = NULL;
    if (target[0] == '/') {
        // Absolute path - use as-is
        resolved_path = kryon_strdup(target);
        printf("🔗 Using absolute path: %s\n", resolved_path);
    } else {
        // Relative path - resolve based on current location
        if (nav_manager->current && nav_manager->current->path) {
            // Get directory of current file
            char* current_path_copy = kryon_strdup(nav_manager->current->path);
            char* current_dir = dirname(current_path_copy);
            
            printf("🔗 Current file directory: %s\n", current_dir);
            
            // Construct resolved path
            size_t path_len = strlen(current_dir) + strlen(target) + 2; // +2 for '/' and '\0'
            resolved_path = kryon_alloc(path_len);
            if (resolved_path) {
                snprintf(resolved_path, path_len, "%s/%s", current_dir, target);
            }
            kryon_free(current_path_copy);
        } else {
            // No current location - use target as-is
            printf("🔗 No current location, using target as-is\n");
            resolved_path = kryon_strdup(target);
        }
    }
    
    if (!resolved_path) {
        return KRYON_NAV_ERROR_MEMORY;
    }
    
    printf("🔗 Resolved path: %s\n", resolved_path);
    
    KryonNavigationResult result;
    
    if (ends_with(resolved_path, ".krb")) {
        result = kryon_navigation_load_krb(nav_manager, resolved_path);
    } else if (ends_with(resolved_path, ".kry")) {
        result = kryon_navigation_compile_and_load(nav_manager, resolved_path);
    } else {
        printf("❓ Unknown file type: %s\n", resolved_path);
        kryon_free(resolved_path);
        return KRYON_NAV_ERROR_INVALID_PATH;
    }
    
    // Add to history on successful navigation
    if (result == KRYON_NAV_SUCCESS) {
        add_to_history(nav_manager, resolved_path, NULL);
    }
    
    kryon_free(resolved_path);
    return result;
}

KryonNavigationResult kryon_navigate_back(KryonNavigationManager* nav_manager) {
    if (!nav_manager || !nav_manager->current || !nav_manager->current->prev) {
        return KRYON_NAV_ERROR_NO_HISTORY;
    }
    
    nav_manager->current = nav_manager->current->prev;
    printf("⬅️  Navigating back to: %s\n", nav_manager->current->path);
    
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
    printf("➡️  Navigating forward to: %s\n", nav_manager->current->path);
    
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
            printf("🧭 Set initial navigation path: %s\n", path);
        }
    } else {
        // Update existing current path
        kryon_free(nav_manager->current->path);
        nav_manager->current->path = kryon_strdup(path);
        printf("🧭 Updated navigation path: %s\n", path);
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
        printf("❌ KRB file not found: %s\n", krb_path);
        return KRYON_NAV_ERROR_FILE_NOT_FOUND;
    }
    
    printf("📄 Loading KRB file: %s\n", krb_path);
    
    // Clear all existing content before loading new file
    kryon_runtime_clear_all_content(nav_manager->runtime);
    
    // Load KRB file into runtime
    if (!kryon_runtime_load_file(nav_manager->runtime, krb_path)) {
        printf("❌ Failed to load KRB file: %s\n", krb_path);
        return KRYON_NAV_ERROR_FILE_NOT_FOUND;
    }
    
    printf("✅ KRB file loaded successfully: %s\n", krb_path);
    
    // Calculate layout positions before rendering begins
    printf("🧮 Navigation: Calculating layout after KRB load\n");
    kryon_runtime_calculate_layout(nav_manager->runtime);
    
    // Update window properties from new App element
    update_window_from_app_element(nav_manager->runtime);
    
    return KRYON_NAV_SUCCESS;
}

KryonNavigationResult kryon_navigation_compile_and_load(KryonNavigationManager* nav_manager, const char* kry_path) {
    if (!nav_manager || !kry_path) {
        return KRYON_NAV_ERROR_INVALID_PATH;
    }
    
    if (!kryon_navigation_file_exists(kry_path)) {
        printf("❌ KRY file not found: %s\n", kry_path);
        return KRYON_NAV_ERROR_FILE_NOT_FOUND;
    }
    
    printf("⚡ Compiling KRY file: %s\n", kry_path);
    
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
    
    printf("🌐 Opening external URL: %s\n", url);
    
    #ifdef __linux__
        char command[2048];
        snprintf(command, sizeof(command), "xdg-open '%s' 2>/dev/null &", url);
        int result = system(command);
        if (result == 0) {
            printf("✅ External URL opened successfully\n");
            return KRYON_NAV_SUCCESS;
        } else {
            printf("❌ Failed to open external URL\n");
            return KRYON_NAV_ERROR_EXTERNAL_FAILED;
        }
    #elif __APPLE__
        char command[2048];
        snprintf(command, sizeof(command), "open '%s' &", url);
        int result = system(command);
        if (result == 0) {
            printf("✅ External URL opened successfully\n");
            return KRYON_NAV_SUCCESS;
        } else {
            printf("❌ Failed to open external URL\n");
            return KRYON_NAV_ERROR_EXTERNAL_FAILED;
        }
    #elif _WIN32
        // TODO: Implement Windows support with ShellExecuteA
        printf("🌐 External URL (Windows support coming soon): %s\n", url);
        return KRYON_NAV_SUCCESS;
    #else
        printf("🌐 External URL (Platform not supported): %s\n", url);
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
        printf("⚡ Using cached compilation: %s\n", cached->krb_path);
        *output_krb_path = kryon_strdup(cached->krb_path);
        return KRYON_NAV_SUCCESS;
    }
    
    // Generate cache file path
    char* cache_path = get_cache_path(kry_path);
    if (!cache_path) {
        return KRYON_NAV_ERROR_MEMORY;
    }
    
    printf("⚡ Compiling %s -> %s\n", kry_path, cache_path);
    
    // Use the runtime compiler to compile the .kry file
    KryonCompileResult compile_result = kryon_compile_file(kry_path, cache_path, NULL);
    if (compile_result != KRYON_COMPILE_SUCCESS) {
        printf("❌ Compilation failed: %s\n", kryon_compile_result_string(compile_result));
        kryon_free(cache_path);
        return KRYON_NAV_ERROR_COMPILATION_FAILED;
    }
    
    // Add to cache
    add_to_cache(nav_manager, kry_path, cache_path);
    
    *output_krb_path = cache_path;
    printf("✅ Compilation successful: %s\n", cache_path);
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
    
    printf("🧹 Compilation cache cleared\n");
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
    
    printf("🪟 Checking window properties from new App element...\n");
    
    // Get window properties from App element
    float window_width = get_element_property_float(runtime->root, "windowWidth", 800.0f);
    float window_height = get_element_property_float(runtime->root, "windowHeight", 600.0f);
    const char* window_title = get_element_property_string(runtime->root, "windowTitle");
    
    printf("🪟 New window properties: %gx%g, title: '%s'\n", 
           window_width, window_height, window_title ? window_title : "null");
    
    // Get renderer from runtime (assuming it's available)
    KryonRenderer* renderer = runtime->renderer;
    if (!renderer) {
        printf("⚠️  No renderer available for window updates\n");
        return;
    }
    
    // Update window size if different using renderer abstraction
    if (window_width != 800.0f || window_height != 600.0f) {
        printf("🪟 Updating window size to %gx%g\n", window_width, window_height);
        KryonRenderResult result = kryon_renderer_update_window_size(
            renderer, (int)window_width, (int)window_height);
        if (result != KRYON_RENDER_SUCCESS) {
            printf("⚠️  Failed to update window size\n");
        }
    }
    
    // Update window title if provided using renderer abstraction
    if (window_title && strlen(window_title) > 0) {
        printf("🪟 Updating window title to: '%s'\n", window_title);
        KryonRenderResult result = kryon_renderer_update_window_title(renderer, window_title);
        if (result != KRYON_RENDER_SUCCESS) {
            printf("⚠️  Failed to update window title\n");
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
    
    snprintf(cache_path, strlen(KRYON_NAV_CACHE_DIR) + strlen(filename) + 10, 
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