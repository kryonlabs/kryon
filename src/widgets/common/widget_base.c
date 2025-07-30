/**
 * @file widget_base.c
 * @brief Kryon Widget Base Implementation
 */

#include "internal/widgets.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// WIDGET BASE FUNCTIONS
// =============================================================================

void kryon_widget_init(KryonWidget* widget, KryonWidgetType type) {
    if (!widget) return;
    
    memset(widget, 0, sizeof(KryonWidget));
    widget->type = type;
    widget->state = KRYON_WIDGET_STATE_NORMAL;
    widget->visible = true;
    widget->enabled = true;
    
    // Set default styling
    widget->style.background_color = (KryonColor){0.95f, 0.95f, 0.95f, 1.0f};
    widget->style.text_color = (KryonColor){0.0f, 0.0f, 0.0f, 1.0f};
    widget->style.border_color = (KryonColor){0.7f, 0.7f, 0.7f, 1.0f};
    widget->style.border_width = 1.0f;
    widget->style.border_radius = 4.0f;
    widget->style.font_size = 14.0f;
    widget->style.padding = (KryonPadding){8.0f, 8.0f, 8.0f, 8.0f};
    widget->style.margin = (KryonMargin){0.0f, 0.0f, 0.0f, 0.0f};
    widget->style.opacity = 1.0f;
}

void kryon_widget_cleanup(KryonWidget* widget) {
    if (!widget) return;
    
    kryon_free(widget->id);
    kryon_free(widget->class_name);
    kryon_free(widget->style.font_family);
    
    widget->id = NULL;
    widget->class_name = NULL;
    widget->style.font_family = NULL;
}

void kryon_widget_set_bounds(KryonWidget* widget, KryonRect bounds) {
    if (!widget) return;
    
    widget->bounds = bounds;
}

void kryon_widget_set_style(KryonWidget* widget, const KryonWidgetStyle* style) {
    if (!widget || !style) return;
    
    // Free existing font family
    kryon_free(widget->style.font_family);
    
    // Copy style
    widget->style = *style;
    
    // Copy font family string if provided
    if (style->font_family) {
        widget->style.font_family = kryon_alloc(strlen(style->font_family) + 1);
        if (widget->style.font_family) {
            strcpy(widget->style.font_family, style->font_family);
        }
    }
}

bool kryon_point_in_rect(KryonVec2 point, KryonRect rect) {
    return point.x >= rect.x &&
           point.x <= rect.x + rect.width &&
           point.y >= rect.y &&
           point.y <= rect.y + rect.height;
}