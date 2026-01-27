/*
 * desktop_input_helpers.c - Platform-Agnostic Input Utilities
 *
 * This module provides platform-agnostic input helper functions used by
 * all renderer backends. It handles:
 * - Input state management (caret position, focus tracking)
 * - Dropdown collection and hit testing (IR tree operations)
 * - Page routing for multi-page applications
 * - Tab click detection
 *
 * These functions operate on IR component trees and don't depend on
 * any specific renderer (SDL3, Raylib, etc.)
 */

#define _POSIX_C_SOURCE 200809L
#include "desktop_internal.h"
#include "../../ir/include/ir_executor.h"
#include "../../ir/include/ir_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// ============================================================================
// PAGE ROUTER FOR MULTI-PAGE APPS
// ============================================================================

/**
 * Page router state for internal navigation in multi-page apps.
 * Tracks current page and handles loading different IR trees.
 */
typedef struct {
    char current_page[256];      // Current page path (e.g., "/docs")
    char ir_directory[512];      // Directory containing .kir files (e.g., "build/ir/")
    IRComponent* root_component; // Pointer to current root to replace on navigation
    void* renderer;              // Renderer to trigger reload
} PageRouter;

static PageRouter page_router = {0};

/**
 * Initialize the page router with starting page and IR directory.
 * Called from CLI after IR tree is loaded.
 */
void desktop_init_router(const char* initial_page, const char* ir_dir, IRComponent* root, void* renderer) {
    if (initial_page) {
        strncpy(page_router.current_page, initial_page, sizeof(page_router.current_page) - 1);
        page_router.current_page[sizeof(page_router.current_page) - 1] = '\0';
    }
    if (ir_dir) {
        strncpy(page_router.ir_directory, ir_dir, sizeof(page_router.ir_directory) - 1);
        page_router.ir_directory[sizeof(page_router.ir_directory) - 1] = '\0';
    }
    page_router.root_component = root;
    page_router.renderer = renderer;
    printf("🔗 Router initialized: page=%s, ir_dir=%s\n", page_router.current_page, page_router.ir_directory);
}

/**
 * Check if URL is external (starts with http://, https://, or mailto:)
 */
__attribute__((weak)) bool is_external_url(const char* url) {
    if (!url) return false;
    return (strncmp(url, "http://", 7) == 0 ||
            strncmp(url, "https://", 8) == 0 ||
            strncmp(url, "mailto:", 7) == 0);
}

/**
 * Map internal path to .kir filename
 * Examples: / -> index.kir, /docs -> docs.kir
 */
static void path_to_kir_file(const char* path, char* kir_path, size_t max_len) {
    if (!path || strlen(path) == 0 || strcmp(path, "/") == 0) {
        snprintf(kir_path, max_len, "%sindex.kir", page_router.ir_directory);
    } else {
        // Strip leading slash and append .kir
        const char* clean_path = path[0] == '/' ? path + 1 : path;
        snprintf(kir_path, max_len, "%s%s.kir", page_router.ir_directory, clean_path);
    }
}

// Forward declaration - renderer reload function
void desktop_renderer_reload_tree(void* renderer, IRComponent* new_root);

/**
 * Navigate to a new page by loading its IR tree.
 * This is the core of internal navigation - loads a new .kir file and swaps the IR tree.
 */
__attribute__((weak)) void navigate_to_page(const char* path) {
    char kir_path[1024];
    path_to_kir_file(path, kir_path, sizeof(kir_path));

    printf("🔗 Navigating to: %s (IR file: %s)\n", path, kir_path);

    // Check if file exists
    FILE* test = fopen(kir_path, "r");
    if (!test) {
        IR_LOG_ERROR("NAVIGATION", "Page not found: %s (tried %s)", path, kir_path);
        return;
    }
    fclose(test);

    // Load new IR tree
    IRComponent* new_root = ir_read_json_file(kir_path);
    if (!new_root) {
        IR_LOG_ERROR("NAVIGATION", "Failed to load IR from: %s", kir_path);
        return;
    }

    // Expand ForEach components (runtime expansion for desktop)
    extern void ir_expand_foreach(IRComponent* root);
    ir_expand_foreach(new_root);

    // Clean up old tree
    if (page_router.root_component) {
        ir_destroy_component(page_router.root_component);
    }

    // Update router state
    page_router.root_component = new_root;
    strncpy(page_router.current_page, path, sizeof(page_router.current_page) - 1);
    page_router.current_page[sizeof(page_router.current_page) - 1] = '\0';

    // Trigger renderer reload
    desktop_renderer_reload_tree(page_router.renderer, new_root);

    printf("✅ Navigation complete: %s\n", path);
}

/**
 * Open external URLs in system browser.
 * Uses platform-specific commands (xdg-open on Linux, open on macOS, start on Windows).
 */
__attribute__((weak)) void open_external_url(const char* url) {
    if (!url) return;

    printf("🔗 Opening external URL: %s\n", url);

    // Validate URL for safety (prevent command injection)
    for (const char* p = url; *p; p++) {
        if (*p == '"' || *p == '\'' || *p == ';' || *p == '`' || *p == '$') {
            IR_LOG_ERROR("NAVIGATION", "Unsafe characters in URL: %s", url);
            return;
        }
    }

    char command[2048];
    #if defined(_WIN32)
        snprintf(command, sizeof(command), "start \"\" \"%s\"", url);
    #elif defined(__APPLE__)
        snprintf(command, sizeof(command), "open \"%s\"", url);
    #else
        snprintf(command, sizeof(command), "xdg-open \"%s\" > /dev/null 2>&1 &", url);
    #endif

    int result = system(command);
    if (result != 0) {
        IR_LOG_ERROR("NAVIGATION", "Failed to open URL: %s (exit code: %d)", url, result);
    }
}

// ============================================================================
// TAB CLICK DETECTION (Frontend-Agnostic)
// ============================================================================

/**
 * Check if clicked button is a tab in a TabGroup and handle tab switching.
 * Returns true if handled as tab, false otherwise.
 */
__attribute__((weak)) bool try_handle_as_tab_click(DesktopIRRenderer* renderer, IRComponent* clicked) {
    if (getenv("KRYON_DEBUG_TAB_LAYOUT")) {
        fprintf(stderr, "[TAB_CLICK_CHECK] Component ID=%u parent=%p\n",
               clicked ? clicked->id : 0, clicked ? clicked->parent : NULL);
    }

    if (!clicked || !clicked->parent || !clicked->parent->custom_data) {
        if (getenv("KRYON_DEBUG_TAB_LAYOUT")) {
            fprintf(stderr, "[TAB_CLICK_CHECK] Early return: clicked=%p parent=%p custom_data=%p\n",
                   clicked, clicked ? clicked->parent : NULL,
                   (clicked && clicked->parent) ? clicked->parent->custom_data : NULL);
        }
        return false;
    }

    TabGroupState* tg_state = (TabGroupState*)clicked->parent->custom_data;

    // Verify this looks like a valid TabGroupState (tab_bar matches parent)
    if (!tg_state || tg_state->tab_bar != clicked->parent) {
        if (getenv("KRYON_DEBUG_TAB_LAYOUT")) {
            fprintf(stderr, "[TAB_CLICK_CHECK] Not a tab: tg_state=%p tab_bar=%p parent=%p\n",
                   tg_state, tg_state ? tg_state->tab_bar : NULL, clicked->parent);
        }
        return false;
    }

    // Find which tab was clicked
    for (uint32_t i = 0; i < tg_state->tab_count; i++) {
        if (tg_state->tabs[i] == clicked) {
            if (getenv("KRYON_DEBUG_TAB_LAYOUT")) {
                fprintf(stderr, "[TAB_CLICK_DETECTED] Tab %u clicked, calling ir_tabgroup_handle_tab_click\n", i);
            }
            // C handles tab selection (panel switching, visuals)
            ir_tabgroup_handle_tab_click(tg_state, i);

            // Trigger user's onClick callback if they provided one

            return true;
        }
    }

    if (getenv("KRYON_DEBUG_TAB_LAYOUT")) {
        fprintf(stderr, "[TAB_CLICK_CHECK] Not found in tabs array (tab_count=%u)\n", tg_state->tab_count);
    }
    return false;
}

// ============================================================================
// INPUT STATE MANAGEMENT
// ============================================================================
// NOTE: input_states and input_state_count are defined in desktop_globals.c

/**
 * Get or create input runtime state for a component
 *
 * Maintains a table of input states indexed by component ID.
 * Input state tracks caret position, scroll offset, focus, and blink state.
 *
 * Args:
 *   id - Component ID to get state for
 *
 * Returns:
 *   Pointer to InputRuntimeState, or NULL if table is full
 */
InputRuntimeState* get_input_state(uint32_t id) {
    for (size_t i = 0; i < input_state_count; i++) {
        if (input_states[i].id == id) return &input_states[i];
    }
    if (input_state_count < sizeof(input_states) / sizeof(input_states[0])) {
        input_states[input_state_count] = (InputRuntimeState){
            .id = id,
            .cursor_index = 0,
            .scroll_x = 0.0f,
            .focused = false,
            .last_blink_ms = 0,
            .caret_visible = true
        };
        input_state_count++;
        return &input_states[input_state_count - 1];
    }
    return NULL;
}

// ============================================================================
// DROPDOWN MENU HIT TESTING
// ============================================================================

/**
 * Collect all open dropdowns from component tree
 *
 * Recursively traverses the component tree and collects all open dropdowns.
 * Used for hit testing and state management.
 *
 * Args:
 *   component - Component to search (root on first call)
 *   dropdown_list - Array to fill with open dropdown components
 *   count - Pointer to current count (incremented as dropdowns are added)
 *   max_count - Maximum entries in dropdown_list
 */
void collect_open_dropdowns(IRComponent* component, IRComponent** dropdown_list, int* count, int max_count) {
    if (!component || !dropdown_list || !count || *count >= max_count) return;

    // Check if this component is an open dropdown
    if (component->type == IR_COMPONENT_DROPDOWN) {
        IRDropdownState* state = ir_get_dropdown_state(component);
        if (state && state->is_open && component->rendered_bounds.valid) {
            dropdown_list[*count] = component;
            (*count)++;
        }
    }

    // Recursively check children
    for (uint32_t i = 0; i < component->child_count && *count < max_count; i++) {
        collect_open_dropdowns(component->children[i], dropdown_list, count, max_count);
    }
}

/**
 * Hit test to find dropdown menu at point
 *
 * Checks if a point (typically mouse position) is within an open dropdown menu.
 * Respects z-index by checking dropdowns in reverse order.
 *
 * Args:
 *   root - Root component to search from
 *   x - X coordinate to test
 *   y - Y coordinate to test
 *
 * Returns:
 *   Dropdown component if point is in menu, NULL otherwise
 */
IRComponent* find_dropdown_menu_at_point(IRComponent* root, float x, float y) {
    if (!root) return NULL;

    // Collect all open dropdowns
    #define MAX_OPEN_DROPDOWNS_CHECK 10
    IRComponent* open_dropdowns[MAX_OPEN_DROPDOWNS_CHECK];
    int dropdown_count = 0;
    collect_open_dropdowns(root, open_dropdowns, &dropdown_count, MAX_OPEN_DROPDOWNS_CHECK);

    // Check each open dropdown menu (in reverse order for proper z-index)
    for (int i = dropdown_count - 1; i >= 0; i--) {
        IRComponent* dropdown = open_dropdowns[i];
        IRDropdownState* state = ir_get_dropdown_state(dropdown);
        if (!state || !state->is_open) continue;

        IRRenderedBounds bounds = dropdown->rendered_bounds;
        if (!bounds.valid) continue;

        // Calculate menu area
        float menu_y = bounds.y + bounds.height;
        float menu_height = fminf(state->option_count * 35.0f, 200.0f);

        // Check if point is in menu area
        if (x >= bounds.x && x < bounds.x + bounds.width &&
            y >= menu_y && y < menu_y + menu_height) {
            return dropdown;
        }
    }

    return NULL;
}
