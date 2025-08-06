/**
 * @file widget_state.c
 * @brief Kryon Widget State Management
 */

#include "internal/widgets.h"
#include "internal/memory.h"
#include <string.h>

// =============================================================================
// WIDGET STATE MANAGEMENT
// =============================================================================

typedef struct {
    KryonWidget** widgets;
    size_t widget_count;
    size_t widget_capacity;
    KryonWidget* focused_widget;
} KryonWidgetManager;

KryonWidgetManager* kryon_widget_manager_create(void) {
    KryonWidgetManager* manager = kryon_alloc(sizeof(KryonWidgetManager));
    if (!manager) return NULL;
    
    memset(manager, 0, sizeof(KryonWidgetManager));
    manager->widget_capacity = 16;
    manager->widgets = kryon_alloc(sizeof(KryonWidget*) * manager->widget_capacity);
    
    if (!manager->widgets) {
        kryon_free(manager);
        return NULL;
    }
    
    return manager;
}

void kryon_widget_manager_destroy(KryonWidgetManager* manager) {
    if (!manager) return;
    
    kryon_free(manager->widgets);
    kryon_free(manager);
}

bool kryon_widget_register(KryonWidgetManager* manager, KryonWidget* widget) {
    if (!manager || !widget) return false;
    
    // Expand capacity if needed
    if (manager->widget_count >= manager->widget_capacity) {
        size_t new_capacity = manager->widget_capacity * 2;
        KryonWidget** new_widgets = kryon_alloc(sizeof(KryonWidget*) * new_capacity);
        if (!new_widgets) return false;
        
        memcpy(new_widgets, manager->widgets, sizeof(KryonWidget*) * manager->widget_count);
        kryon_free(manager->widgets);
        
        manager->widgets = new_widgets;
        manager->widget_capacity = new_capacity;
    }
    
    manager->widgets[manager->widget_count] = widget;
    manager->widget_count++;
    
    return true;
}

void kryon_widget_unregister(KryonWidgetManager* manager, const char* widget_id) {
    if (!manager || !widget_id) return;
    
    for (size_t i = 0; i < manager->widget_count; i++) {
        KryonWidget* widget = manager->widgets[i];
        if (widget && widget->id && strcmp(widget->id, widget_id) == 0) {
            // Remove from array by shifting elements
            for (size_t j = i; j < manager->widget_count - 1; j++) {
                manager->widgets[j] = manager->widgets[j + 1];
            }
            manager->widget_count--;
            
            // Clear focus if this was the focused widget
            if (manager->focused_widget == widget) {
                manager->focused_widget = NULL;
            }
            
            break;
        }
    }
}

KryonWidget* kryon_widget_find(KryonWidgetManager* manager, const char* widget_id) {
    if (!manager || !widget_id) return NULL;
    
    for (size_t i = 0; i < manager->widget_count; i++) {
        KryonWidget* widget = manager->widgets[i];
        if (widget && widget->id && strcmp(widget->id, widget_id) == 0) {
            return widget;
        }
    }
    
    return NULL;
}

void kryon_widget_set_focus(KryonWidgetManager* manager, const char* widget_id) {
    if (!manager) return;
    
    // Clear current focus
    if (manager->focused_widget) {
        manager->focused_widget->state = KRYON_WIDGET_STATE_NORMAL;
    }
    
    // Set new focus
    KryonWidget* widget = kryon_widget_find(manager, widget_id);
    if (widget && widget->enabled) {
        manager->focused_widget = widget;
        widget->state = KRYON_WIDGET_STATE_FOCUSED;
    } else {
        manager->focused_widget = NULL;
    }
}

void kryon_widget_clear_focus(KryonWidgetManager* manager) {
    if (!manager || !manager->focused_widget) return;
    
    manager->focused_widget->state = KRYON_WIDGET_STATE_NORMAL;
    manager->focused_widget = NULL;
}

KryonWidget* kryon_widget_get_focused(KryonWidgetManager* manager) {
    return manager ? manager->focused_widget : NULL;
}