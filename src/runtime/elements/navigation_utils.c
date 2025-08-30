/**
 * @file navigation_utils.c
 * @brief Shared navigation utilities for elements that support navigation
 * 
 * This file provides reusable navigation functions that can be used by
 * any element that needs to support internal/external navigation.
 * 
 * 0BSD License
 */

#include "navigation_utils.h"
#include "elements.h"
#include "runtime.h"
#include "../navigation/navigation.h"
#include "../../shared/kryon_mappings.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// =============================================================================
// PUBLIC NAVIGATION UTILITY FUNCTIONS
// =============================================================================

void navigation_ensure_manager(struct KryonRuntime* runtime, const char* element_name) {
    if (!runtime->navigation_manager) {
        runtime->navigation_manager = kryon_navigation_create(runtime);
        if (!runtime->navigation_manager) {
            printf("⚠️  Failed to create navigation manager for %s element\n", element_name);
        } else {
            printf("🧭 Navigation manager created (%s element with navigation detected)\n", element_name);
            
            // Set current path from runtime's loaded file if available
            if (runtime->current_file_path) {
                kryon_navigation_set_current_path(runtime->navigation_manager, runtime->current_file_path);
                printf("🧭 Set navigation path from runtime: %s\n", runtime->current_file_path);
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
    bool external = get_element_property_bool(element, "external", false);
    
    if (!to || strlen(to) == 0) {
        return false; // No navigation properties
    }
    
    // Auto-detect external URLs or use explicit external flag
    bool is_external = external || navigation_is_external_url(to);
    
    printf("🔗 %s navigation: %s %s\n", element_name, to, is_external ? "(external)" : "(internal)");
    
    if (is_external) {
        // Handle external URLs
        #ifdef __linux__
            char command[1024];
            snprintf(command, sizeof(command), "xdg-open '%s' 2>/dev/null &", to);
            system(command);
        #elif __APPLE__
            char command[1024];
            snprintf(command, sizeof(command), "open '%s' &", to);
            system(command);
        #elif _WIN32
            // Will need to include windows.h for ShellExecuteA
            // ShellExecuteA(NULL, "open", to, NULL, NULL, SW_SHOWNORMAL);
            printf("🌐 External %s link: %s (Windows support coming soon)\n", element_name, to);
        #else
            printf("🌐 External %s link: %s (Platform not supported)\n", element_name, to);
        #endif
    } else {
        // Handle internal navigation using the navigation manager
        if (runtime->navigation_manager) {
            printf("📄 %s navigating to internal file: %s\n", element_name, to);
            KryonNavigationResult result = kryon_navigate_to(runtime->navigation_manager, to, false);
            
            if (result == KRYON_NAV_SUCCESS) {
                printf("✅ %s navigation successful\n", element_name);
            } else {
                printf("❌ %s navigation failed with result: %d\n", element_name, result);
            }
        } else {
            printf("⚠️  Navigation manager not available for %s internal navigation\n", element_name);
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