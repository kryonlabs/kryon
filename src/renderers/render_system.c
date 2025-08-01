/**
 * @file render_system.c
 * @brief Kryon Render System Implementation
 * 
 * High-level rendering system that combines layout engine with renderers
 * to provide a complete UI rendering solution.
 */

#include "kryon/renderer.h"
#include "internal/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// =============================================================================
// RENDER SYSTEM IMPLEMENTATION
// =============================================================================

KryonRenderSystem* kryon_render_system_create(KryonRendererType renderer_type, size_t max_elements) {
    KryonRenderSystem* system = malloc(sizeof(KryonRenderSystem));
    if (!system) {
        fprintf(stderr, "Kryon Render System: Failed to allocate render system\n");
        return NULL;
    }
    
    memset(system, 0, sizeof(KryonRenderSystem));
    
    // Create layout engine
    system->layout_engine = kryon_layout_engine_create(max_elements);
    if (!system->layout_engine) {
        fprintf(stderr, "Kryon Render System: Failed to create layout engine\n");
        free(system);
        return NULL;
    }
    
    // Create renderer
    system->renderer = kryon_renderer_create(renderer_type);
    if (!system->renderer) {
        fprintf(stderr, "Kryon Render System: Failed to create renderer\n");
        kryon_layout_engine_destroy(system->layout_engine);
        free(system);
        return NULL;
    }
    
    // Initialize state
    system->auto_render = true;
    system->viewport_size = (KryonSize){800.0f, 600.0f};
    
    return system;
}

void kryon_render_system_destroy(KryonRenderSystem* system) {
    if (!system) return;
    
    if (system->renderer) {
        if (system->renderer->initialized) {
            kryon_renderer_shutdown(system->renderer);
        }
        kryon_renderer_destroy(system->renderer);
    }
    
    if (system->layout_engine) {
        kryon_layout_engine_destroy(system->layout_engine);
    }
    
    free(system);
}

bool kryon_render_system_initialize(KryonRenderSystem* system, void* renderer_config, KryonSize viewport_size) {
    if (!system || !system->renderer) {
        fprintf(stderr, "Kryon Render System: Invalid system in initialize\n");
        return false;
    }
    
    // Initialize renderer
    if (!kryon_renderer_initialize(system->renderer, renderer_config)) {
        fprintf(stderr, "Kryon Render System: Failed to initialize renderer\n");
        return false;
    }
    
    // Set viewport size
    system->viewport_size = viewport_size;
    
    printf("Kryon Render System: Initialized with viewport %.0fx%.0f\n", 
           viewport_size.width, viewport_size.height);
    
    return true;
}

void kryon_render_system_set_root(KryonRenderSystem* system, KryonElement* root) {
    if (!system) return;
    
    system->root_element = root;
}

bool kryon_render_system_render_frame(KryonRenderSystem* system) {
    if (!system || !system->renderer || !system->layout_engine) {
        fprintf(stderr, "Kryon Render System: Invalid system in render_frame\n");
        return false;
    }
    
    if (!system->renderer->initialized) {
        fprintf(stderr, "Kryon Render System: Renderer not initialized\n");
        return false;
    }
    
    // Begin frame
    if (!system->renderer->vtable.begin_frame(system->renderer)) {
        fprintf(stderr, "Kryon Render System: Failed to begin frame\n");
        return false;
    }
    
    // Clear background
    KryonColor clear_color = {30, 30, 35, 255}; // Dark gray
    system->renderer->vtable.clear(system->renderer, clear_color);
    
    // Calculate layout if we have a root element
    if (system->root_element) {
        kryon_layout_calculate(system->layout_engine, system->root_element, system->viewport_size);
        
        // Get render commands
        KryonRenderCommandArray commands = kryon_layout_get_render_commands(system->layout_engine);
        
        // Execute render commands
        if (commands.count > 0) {
            if (!system->renderer->vtable.execute_commands(system->renderer, &commands)) {
                fprintf(stderr, "Kryon Render System: Failed to execute render commands\n");
                return false;
            }
        }
    }
    
    // End frame
    if (!system->renderer->vtable.end_frame(system->renderer)) {
        fprintf(stderr, "Kryon Render System: Failed to end frame\n");
        return false;
    }
    
    return true;
}

bool kryon_render_system_handle_event(KryonRenderSystem* system, const KryonEvent* event) {
    if (!system || !event) return false;
    
    // Handle window resize events
    if (event->type == KRYON_EVENT_WINDOW_RESIZE) {
        system->viewport_size = event->data.window.new_size;
        printf("Kryon Render System: Viewport resized to %.0fx%.0f\n", 
               system->viewport_size.width, system->viewport_size.height);
        return true;
    }
    
    // TODO: Implement other event handling (mouse clicks, keyboard input, etc.)
    
    return false;
}

// =============================================================================
// RENDERER FACTORY
// =============================================================================

KryonRenderer* kryon_renderer_create(KryonRendererType type) {
    switch (type) {
        case KRYON_RENDERER_SDL2:
            return kryon_renderer_create_sdl2();
            
        case KRYON_RENDERER_RAYLIB:
            return kryon_renderer_create_raylib();
            
        case KRYON_RENDERER_HTML:
            return kryon_renderer_create_html();
            
        case KRYON_RENDERER_TERMINAL:
            return kryon_renderer_create_terminal();
            
        case KRYON_RENDERER_DEBUG:
            return kryon_renderer_create_debug();
            
        default:
            fprintf(stderr, "Kryon Renderer: Unknown renderer type: %d\n", type);
            return NULL;
    }
}

void kryon_renderer_destroy(KryonRenderer* renderer) {
    if (!renderer) return;
    
    // Clean up implementation data
    free(renderer->implementation_data);
    free(renderer);
}

bool kryon_renderer_initialize(KryonRenderer* renderer, void* config) {
    if (!renderer || !renderer->vtable.initialize) {
        fprintf(stderr, "Kryon Renderer: Invalid renderer in initialize\n");
        return false;
    }
    
    return renderer->vtable.initialize(renderer, config);
}

void kryon_renderer_shutdown(KryonRenderer* renderer) {
    if (!renderer || !renderer->vtable.shutdown) return;
    
    renderer->vtable.shutdown(renderer);
}

// =============================================================================
// STUB IMPLEMENTATIONS FOR OTHER RENDERERS
// =============================================================================

// Raylib renderer is now implemented in raylib_renderer.c
// Forward declaration is in renderer.h

KryonRenderer* kryon_renderer_create_html(void) {
    fprintf(stderr, "Kryon HTML: Renderer not yet implemented\n");
    return NULL;
}

KryonRenderer* kryon_renderer_create_terminal(void) {
    fprintf(stderr, "Kryon Terminal: Renderer not yet implemented\n");
    return NULL;
}

KryonRenderer* kryon_renderer_create_debug(void) {
    fprintf(stderr, "Kryon Debug: Renderer not yet implemented\n");
    return NULL;
}