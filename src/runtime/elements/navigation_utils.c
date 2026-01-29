/**

 * @file navigation_utils.c
 * @brief Shared navigation utilities for elements that support navigation
 * 
 * This file provides reusable navigation functions that can be used by
 * any element that needs to support internal/external navigation.
 * 
 * 0BSD License
 */
#include "lib9.h"


#include "navigation_utils.h"
#include "elements.h"
#include "runtime.h"
#include "../navigation/navigation.h"
#include "../../shared/kryon_mappings.h"

// =============================================================================
// PUBLIC NAVIGATION UTILITY FUNCTIONS
// =============================================================================

void navigation_ensure_manager(struct KryonRuntime* runtime, const char* element_name) {
    if (!runtime->navigation_manager) {
        runtime->navigation_manager = kryon_navigation_create(runtime);
        if (!runtime->navigation_manager) {
            fprintf(stderr, "⚠️  Failed to create navigation manager for %s element\n", element_name);
        } else {
            fprintf(stderr, "🧭 Navigation manager created (%s element with navigation detected)\n", element_name);
            
            // Set current path from runtime's loaded file if available
            if (runtime->current_file_path) {
                kryon_navigation_set_current_path(runtime->navigation_manager, runtime->current_file_path);
                fprintf(stderr, "🧭 Set navigation path from runtime: %s\n", runtime->current_file_path);
            }
        }
    }
}

void navigation_check_and_init(struct KryonRuntime* runtime, 
                              struct KryonElement* element, 
                              const char* element_name) {
    const char* to = get_element_property_string(element, "to");
    if (to && strlen(to) > 0) {
        navigation_ensure_manager(runtime, element_name);
    }
}

bool navigation_handle_click(struct KryonRuntime* runtime, 
                            struct KryonElement* element,
                            const char* element_name) {
    const char* to = get_element_property_string(element, "to");
    const char* overlay = get_element_property_string(element, "overlay");
    bool external = get_element_property_bool(element, "external", false);
    
    fprintf(stderr, "🐛 CLICK DEBUG: Initial 'to' property = '%s'\n", to ? to : "NULL");
    
    if (!to || strlen(to) == 0) {
        fprintf(stderr, "🐛 CLICK DEBUG: 'to' property is empty or NULL, not navigating\n");
        return false; // No navigation properties
    }
    
    // Auto-detect external URLs or use explicit external flag
    bool is_external = external || navigation_is_external_url(to);
    
    fprintf(stderr, "🔗 %s navigation: %s %s\n", element_name, to, is_external ? "(external)" : "(internal)");
    if (overlay && strlen(overlay) > 0) {
        fprintf(stderr, "🔀 With overlay component: %s\n", overlay);
    }
    
    if (is_external) {
        // Handle external URLs
        #ifdef __linux__
            char command[1024];
            snprint(command, sizeof(command), "xdg-open '%s' 2>/dev/null &", to);
            system(command);
        #elif __APPLE__
            char command[1024];
            snprint(command, sizeof(command), "open '%s' &", to);
            system(command);
        #elif _WIN32
            // Will need to include windows.h for ShellExecuteA
            // ShellExecuteA(NULL, "open", to, NULL, NULL, SW_SHOWNORMAL);
            fprintf(stderr, "🌐 External %s link: %s (Windows support coming soon)\n", element_name, to);
        #else
            fprintf(stderr, "🌐 External %s link: %s (Platform not supported)\n", element_name, to);
        #endif
    } else {
        // Handle internal navigation using the navigation manager
        if (runtime->navigation_manager) {
            // Set overlay component if specified
            if (overlay && strlen(overlay) > 0) {
                fprintf(stderr, "🔀 Setting overlay '%s' for navigation\n", overlay);
                kryon_navigation_set_overlay(runtime->navigation_manager, overlay, runtime);
            }
            
            fprintf(stderr, "📄 %s navigating to internal file: %s\n", element_name, to);
            
            // Check 'to' property before navigation
            const char* to_before_nav = get_element_property_string(element, "to");
            fprintf(stderr, "🐛 CLICK DEBUG: 'to' property before navigation = '%s'\n", to_before_nav ? to_before_nav : "NULL");
            
            KryonNavigationResult result = kryon_navigate_to(runtime->navigation_manager, to, false);
            
            // CRITICAL FIX: DO NOT access element after navigation!
            // Navigation destroys the old element tree, making 'element' invalid
            fprintf(stderr, "🔧 NAVIGATION DEBUG: Navigation completed, element is now invalid\n");
            
            if (result == KRYON_NAV_SUCCESS) {
                fprintf(stderr, "✅ %s navigation successful\n", element_name);
            } else {
                fprintf(stderr, "❌ %s navigation failed with result: %d\n", element_name, result);
            }
        } else {
            fprintf(stderr, "⚠️  Navigation manager not available for %s internal navigation\n", element_name);
        }
    }
    
    return true; // Navigation was handled
}

bool navigation_is_external_url(const char* path) {
    if (!path) return false;
    
    return (strncmp(path, "http://", 7) == 0 ||
            strncmp(path, "https://", 8) == 0 ||
            strncmp(path, "mailto:", 7) == 0 ||
            strncmp(path, "file://", 7) == 0 ||
            strncmp(path, "ftp://", 6) == 0);
}
