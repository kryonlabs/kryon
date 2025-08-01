/**
 * @file widget_transform.c
 * @brief Widget transformation implementation for dynamic layout changes
 */

#include "kryon/widget_system.h"
#include "internal/memory.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// =============================================================================
// PRIVATE FUNCTIONS
// =============================================================================

/// Save current widget state for potential rollback
static void save_widget_state(KryonWidget* widget) {
    widget->previous_type = widget->type;
}

/// Clear type-specific properties before transformation
static void clear_widget_properties(KryonWidget* widget) {
    switch (widget->type) {
        case KRYON_WIDGET_TEXT:
            kryon_free(widget->props.text_props.text);
            widget->props.text_props.text = NULL;
            break;
            
        case KRYON_WIDGET_BUTTON:
            kryon_free(widget->props.button_props.label);
            kryon_free(widget->props.button_props.on_click_script);
            widget->props.button_props.label = NULL;
            widget->props.button_props.on_click_script = NULL;
            widget->props.button_props.on_click = NULL;
            break;
            
        case KRYON_WIDGET_INPUT:
            kryon_free(widget->props.input_props.value);
            kryon_free(widget->props.input_props.placeholder);
            widget->props.input_props.value = NULL;
            widget->props.input_props.placeholder = NULL;
            break;
            
        case KRYON_WIDGET_IMAGE:
            kryon_free(widget->props.image_props.src);
            widget->props.image_props.src = NULL;
            widget->props.image_props.image_data = NULL;
            break;
            
        default:
            break;
    }
    
    // Clear the union
    memset(&widget->props, 0, sizeof(widget->props));
}

/// Initialize properties for new widget type
static void init_new_type_properties(KryonWidget* widget, KryonWidgetType new_type) {
    widget->type = new_type;
    
    switch (new_type) {
        case KRYON_WIDGET_TEXT:
            widget->props.text_props.style.font_size = 16.0f;
            widget->props.text_props.style.color = (KryonColor){0, 0, 0, 255};
            widget->props.text_props.text = kryon_strdup("Text");
            break;
            
        case KRYON_WIDGET_BUTTON:
            widget->props.button_props.background_color = (KryonColor){0, 123, 255, 255};
            widget->props.button_props.text_color = (KryonColor){255, 255, 255, 255};
            widget->props.button_props.label = kryon_strdup("Button");
            break;
            
        case KRYON_WIDGET_CONTAINER:
            widget->props.container_props.background_color = (KryonColor){0, 0, 0, 0};
            break;
            
        case KRYON_WIDGET_INPUT:
            widget->props.input_props.border.width = 1.0f;
            widget->props.input_props.border.color = (KryonColor){206, 212, 218, 255};
            widget->props.input_props.value = kryon_strdup("");
            break;
            
        case KRYON_WIDGET_IMAGE:
            widget->props.image_props.tint = (KryonColor){255, 255, 255, 255};
            break;
            
        default:
            break;
    }
}

/// Ensure widget has children array if transforming to layout type
static bool ensure_children_array(KryonWidget* widget) {
    if (!widget->children) {
        widget->children = kryon_malloc(4 * sizeof(KryonWidget*));
        if (!widget->children) {
            return false;
        }
        widget->child_capacity = 4;
        widget->child_count = 0;
    }
    return true;
}

/// Remove children array if transforming from layout type
static void remove_children_array(KryonWidget* widget) {
    if (widget->children) {
        // Release all children first
        for (size_t i = 0; i < widget->child_count; i++) {
            widget->children[i]->parent = NULL;
            kryon_widget_release(widget->children[i]);
        }
        
        kryon_free(widget->children);
        widget->children = NULL;
        widget->child_count = 0;
        widget->child_capacity = 0;
    }
}

/// Check if transformation is valid
static bool is_valid_transformation(KryonWidget* widget, KryonWidgetType new_type) {
    if (!widget) return false;
    if (widget->type == new_type) return true; // No-op transformation
    if (widget->transform_state == KRYON_TRANSFORM_IN_PROGRESS) return false;
    
    // Check for unsupported transformations
    // For now, all transformations are supported
    return true;
}

// =============================================================================
// WIDGET TRANSFORMATION API
// =============================================================================

bool kryon_widget_transform_to(KryonWidget* widget, KryonWidgetType new_type) {
    if (!is_valid_transformation(widget, new_type)) {
        return false;
    }
    
    // No-op if same type
    if (widget->type == new_type) {
        return true;
    }
    
    // Mark transformation in progress
    widget->transform_state = KRYON_TRANSFORM_IN_PROGRESS;
    save_widget_state(widget);
    
    bool was_layout_type = kryon_widget_is_layout_type(widget->type);
    bool is_layout_type = kryon_widget_is_layout_type(new_type);
    
    // Clear old properties
    clear_widget_properties(widget);
    
    // Handle children array based on layout type changes
    if (!was_layout_type && is_layout_type) {
        // Transforming TO layout widget - ensure children array exists
        if (!ensure_children_array(widget)) {
            widget->transform_state = KRYON_TRANSFORM_NONE;
            return false;
        }
    } else if (was_layout_type && !is_layout_type) {
        // Transforming FROM layout widget - remove children
        remove_children_array(widget);
    }
    
    // Initialize new type properties
    init_new_type_properties(widget, new_type);
    
    // Mark layout as dirty
    kryon_widget_mark_dirty(widget);
    
    // Complete transformation
    widget->transform_state = KRYON_TRANSFORM_COMPLETE;
    
    return true;
}

void kryon_widget_preserve_layout_props(KryonWidget* widget) {
    if (!widget) return;
    
    // Layout properties (main_axis, cross_axis, spacing, padding, margin, flex)
    // are already preserved during transformation since they're outside the union
    // This function exists for future extensibility
}

void kryon_widget_cancel_transform(KryonWidget* widget) {
    if (!widget || widget->transform_state != KRYON_TRANSFORM_IN_PROGRESS) {
        return;
    }
    
    // Restore previous type (this is a simplified version)
    // In a full implementation, you'd restore all previous state
    widget->type = widget->previous_type;
    widget->transform_state = KRYON_TRANSFORM_NONE;
    
    // Re-initialize properties for original type
    clear_widget_properties(widget);
    init_new_type_properties(widget, widget->type);
}

// =============================================================================
// WIDGET PROPERTY API (for Lua integration)
// =============================================================================

bool kryon_widget_set_property(KryonWidget* widget, const char* property, const char* value) {
    if (!widget || !property || !value) return false;
    
    // Layout properties (work for all widget types)
    if (strcmp(property, "main_axis") == 0) {
        if (strcmp(value, "start") == 0) widget->main_axis = KRYON_MAIN_START;
        else if (strcmp(value, "center") == 0) widget->main_axis = KRYON_MAIN_CENTER;
        else if (strcmp(value, "end") == 0) widget->main_axis = KRYON_MAIN_END;
        else if (strcmp(value, "space_between") == 0) widget->main_axis = KRYON_MAIN_SPACE_BETWEEN;
        else if (strcmp(value, "space_around") == 0) widget->main_axis = KRYON_MAIN_SPACE_AROUND;
        else if (strcmp(value, "space_evenly") == 0) widget->main_axis = KRYON_MAIN_SPACE_EVENLY;
        else return false;
        kryon_widget_mark_dirty(widget);
        return true;
    }
    
    if (strcmp(property, "cross_axis") == 0) {
        if (strcmp(value, "start") == 0) widget->cross_axis = KRYON_CROSS_START;
        else if (strcmp(value, "center") == 0) widget->cross_axis = KRYON_CROSS_CENTER;
        else if (strcmp(value, "end") == 0) widget->cross_axis = KRYON_CROSS_END;
        else if (strcmp(value, "stretch") == 0) widget->cross_axis = KRYON_CROSS_STRETCH;
        else return false;
        kryon_widget_mark_dirty(widget);
        return true;
    }
    
    if (strcmp(property, "spacing") == 0) {
        widget->spacing = atof(value);
        kryon_widget_mark_dirty(widget);
        return true;
    }
    
    if (strcmp(property, "flex") == 0) {
        widget->flex = atoi(value);
        kryon_widget_mark_dirty(widget);
        return true;
    }
    
    if (strcmp(property, "width") == 0) {
        widget->width_expr = kryon_expression_parse(value);
        // If it's a literal, set the value immediately
        if (!kryon_expression_is_reactive(&widget->width_expr)) {
            widget->width = widget->width_expr.data.literal_value;
        }
        kryon_widget_mark_dirty(widget);
        return true;
    }
    
    if (strcmp(property, "height") == 0) {
        widget->height_expr = kryon_expression_parse(value);
        // If it's a literal, set the value immediately
        if (!kryon_expression_is_reactive(&widget->height_expr)) {
            widget->height = widget->height_expr.data.literal_value;
        }
        kryon_widget_mark_dirty(widget);
        return true;
    }
    
    if (strcmp(property, "posX") == 0) {
        widget->posX_expr = kryon_expression_parse(value);
        kryon_widget_mark_dirty(widget);
        return true;
    }
    
    if (strcmp(property, "posY") == 0) {
        widget->posY_expr = kryon_expression_parse(value);
        kryon_widget_mark_dirty(widget);
        return true;
    }
    
    if (strcmp(property, "visible") == 0) {
        widget->visible = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        kryon_widget_mark_dirty(widget);
        return true;
    }
    
    // Content alignment properties
    if (strcmp(property, "contentAlignment") == 0) {
        if (strcmp(value, "top-left") == 0) widget->content_alignment = KRYON_CONTENT_TOP_LEFT;
        else if (strcmp(value, "top-center") == 0) widget->content_alignment = KRYON_CONTENT_TOP_CENTER;
        else if (strcmp(value, "top-right") == 0) widget->content_alignment = KRYON_CONTENT_TOP_RIGHT;
        else if (strcmp(value, "center-left") == 0) widget->content_alignment = KRYON_CONTENT_CENTER_LEFT;
        else if (strcmp(value, "center") == 0) widget->content_alignment = KRYON_CONTENT_CENTER;
        else if (strcmp(value, "center-right") == 0) widget->content_alignment = KRYON_CONTENT_CENTER_RIGHT;
        else if (strcmp(value, "bottom-left") == 0) widget->content_alignment = KRYON_CONTENT_BOTTOM_LEFT;
        else if (strcmp(value, "bottom-center") == 0) widget->content_alignment = KRYON_CONTENT_BOTTOM_CENTER;
        else if (strcmp(value, "bottom-right") == 0) widget->content_alignment = KRYON_CONTENT_BOTTOM_RIGHT;
        else return false;
        kryon_widget_mark_dirty(widget);
        return true;
    }
    
    if (strcmp(property, "contentAlignmentX") == 0) {
        if (strcmp(value, "left") == 0) widget->content_alignment_x = KRYON_ALIGN_LEFT;
        else if (strcmp(value, "center") == 0) widget->content_alignment_x = KRYON_ALIGN_CENTER_H;
        else if (strcmp(value, "right") == 0) widget->content_alignment_x = KRYON_ALIGN_RIGHT;
        else return false;
        kryon_widget_mark_dirty(widget);
        return true;
    }
    
    if (strcmp(property, "contentAlignmentY") == 0) {
        if (strcmp(value, "top") == 0) widget->content_alignment_y = KRYON_ALIGN_TOP;
        else if (strcmp(value, "center") == 0) widget->content_alignment_y = KRYON_ALIGN_CENTER_V;
        else if (strcmp(value, "bottom") == 0) widget->content_alignment_y = KRYON_ALIGN_BOTTOM;
        else return false;
        kryon_widget_mark_dirty(widget);
        return true;
    }
    
    if (strcmp(property, "contentDistribution") == 0) {
        if (strcmp(value, "none") == 0) widget->content_distribution = KRYON_DISTRIBUTE_NONE;
        else if (strcmp(value, "space-between") == 0) widget->content_distribution = KRYON_DISTRIBUTE_SPACE_BETWEEN;
        else if (strcmp(value, "space-around") == 0) widget->content_distribution = KRYON_DISTRIBUTE_SPACE_AROUND;
        else if (strcmp(value, "space-evenly") == 0) widget->content_distribution = KRYON_DISTRIBUTE_SPACE_EVENLY;
        else return false;
        kryon_widget_mark_dirty(widget);
        return true;
    }
    
    // Type-specific properties
    switch (widget->type) {
        case KRYON_WIDGET_TEXT:
            if (strcmp(property, "text") == 0) {
                kryon_free(widget->props.text_props.text);
                widget->props.text_props.text = kryon_strdup(value);
                kryon_widget_mark_dirty(widget);
                return true;
            }
            if (strcmp(property, "font_size") == 0) {
                widget->props.text_props.style.font_size = atof(value);
                kryon_widget_mark_dirty(widget);
                return true;
            }
            break;
            
        case KRYON_WIDGET_BUTTON:
            if (strcmp(property, "text") == 0 || strcmp(property, "label") == 0) {
                kryon_free(widget->props.button_props.label);
                widget->props.button_props.label = kryon_strdup(value);
                kryon_widget_mark_dirty(widget);
                return true;
            }
            if (strcmp(property, "on_click") == 0) {
                kryon_free(widget->props.button_props.on_click_script);
                widget->props.button_props.on_click_script = kryon_strdup(value);
                return true;
            }
            break;
            
        case KRYON_WIDGET_INPUT:
            if (strcmp(property, "value") == 0) {
                kryon_free(widget->props.input_props.value);
                widget->props.input_props.value = kryon_strdup(value);
                return true;
            }
            if (strcmp(property, "placeholder") == 0) {
                kryon_free(widget->props.input_props.placeholder);
                widget->props.input_props.placeholder = kryon_strdup(value);
                return true;
            }
            break;
            
        case KRYON_WIDGET_IMAGE:
            if (strcmp(property, "src") == 0) {
                kryon_free(widget->props.image_props.src);
                widget->props.image_props.src = kryon_strdup(value);
                kryon_widget_mark_dirty(widget);
                return true;
            }
            break;
            
        default:
            break;
    }
    
    return false;
}

char* kryon_widget_get_property(KryonWidget* widget, const char* property) {
    if (!widget || !property) return NULL;
    
    char* result = kryon_malloc(256); // Max property value length
    if (!result) return NULL;
    
    // Layout properties
    if (strcmp(property, "main_axis") == 0) {
        switch (widget->main_axis) {
            case KRYON_MAIN_START: strcpy(result, "start"); break;
            case KRYON_MAIN_CENTER: strcpy(result, "center"); break;
            case KRYON_MAIN_END: strcpy(result, "end"); break;
            case KRYON_MAIN_SPACE_BETWEEN: strcpy(result, "space_between"); break;
            case KRYON_MAIN_SPACE_AROUND: strcpy(result, "space_around"); break;
            case KRYON_MAIN_SPACE_EVENLY: strcpy(result, "space_evenly"); break;
        }
        return result;
    }
    
    if (strcmp(property, "cross_axis") == 0) {
        switch (widget->cross_axis) {
            case KRYON_CROSS_START: strcpy(result, "start"); break;
            case KRYON_CROSS_CENTER: strcpy(result, "center"); break;
            case KRYON_CROSS_END: strcpy(result, "end"); break;
            case KRYON_CROSS_STRETCH: strcpy(result, "stretch"); break;
        }
        return result;
    }
    
    if (strcmp(property, "type") == 0) {
        strcpy(result, kryon_widget_type_to_string(widget->type));
        return result;
    }
    
    if (strcmp(property, "spacing") == 0) {
        snprintf(result, 256, "%.2f", widget->spacing);
        return result;
    }
    
    if (strcmp(property, "flex") == 0) {
        snprintf(result, 256, "%d", widget->flex);
        return result;
    }
    
    // Type-specific properties
    switch (widget->type) {
        case KRYON_WIDGET_TEXT:
            if (strcmp(property, "text") == 0) {
                kryon_free(result);
                return widget->props.text_props.text ? kryon_strdup(widget->props.text_props.text) : kryon_strdup("");
            }
            break;
            
        case KRYON_WIDGET_BUTTON:
            if (strcmp(property, "text") == 0 || strcmp(property, "label") == 0) {
                kryon_free(result);
                return widget->props.button_props.label ? kryon_strdup(widget->props.button_props.label) : kryon_strdup("");
            }
            break;
            
        case KRYON_WIDGET_INPUT:
            if (strcmp(property, "value") == 0) {
                kryon_free(result);
                return widget->props.input_props.value ? kryon_strdup(widget->props.input_props.value) : kryon_strdup("");
            }
            break;
            
        default:
            break;
    }
    
    kryon_free(result);
    return NULL;
}

// =============================================================================
// REACTIVE EXPRESSION IMPLEMENTATION
// =============================================================================

bool kryon_expression_is_reactive(const KryonExpression* expr) {
    if (!expr) return false;
    
    switch (expr->type) {
        case KRYON_EXPR_LITERAL:
            return false; // Literal values are not reactive
        case KRYON_EXPR_PERCENTAGE:
        case KRYON_EXPR_ROOT_WIDTH:
        case KRYON_EXPR_ROOT_HEIGHT:
        case KRYON_EXPR_PARENT_WIDTH:
        case KRYON_EXPR_PARENT_HEIGHT:
        case KRYON_EXPR_ARITHMETIC:
            return true; // These need to be evaluated reactively
    }
    return false;
}

KryonExpression kryon_expression_literal(float value) {
    KryonExpression expr = {0};
    expr.type = KRYON_EXPR_LITERAL;
    expr.data.literal_value = value;
    return expr;
}

KryonExpression kryon_expression_parse(const char* value) {
    if (!value) {
        return kryon_expression_literal(0.0f);
    }
    
    // Handle percentage values (e.g., "100%")
    if (strstr(value, "%")) {
        KryonExpression expr = {0};
        expr.type = KRYON_EXPR_PERCENTAGE;
        expr.data.percentage_value = atof(value) / 100.0f;
        return expr;
    }
    
    // Handle $root variables
    if (strstr(value, "$root.width")) {
        if (strstr(value, " - ")) {
            // Handle arithmetic: $root.width - 200
            KryonExpression expr = {0};
            expr.type = KRYON_EXPR_ARITHMETIC;
            expr.data.arithmetic.left_type = KRYON_EXPR_ROOT_WIDTH;
            expr.data.arithmetic.left_value = 0;
            expr.data.arithmetic.operator = '-';
            
            // Parse the right side
            const char* minus_pos = strstr(value, " - ");
            if (minus_pos) {
                float right_val = atof(minus_pos + 3);
                expr.data.arithmetic.right_type = KRYON_EXPR_LITERAL;
                expr.data.arithmetic.right_value = right_val;
            }
            return expr;
        } else {
            // Simple $root.width
            KryonExpression expr = {0};
            expr.type = KRYON_EXPR_ROOT_WIDTH;
            return expr;
        }
    }
    
    if (strstr(value, "$root.height")) {
        if (strstr(value, " - ")) {
            // Handle arithmetic: $root.height - 200
            KryonExpression expr = {0};
            expr.type = KRYON_EXPR_ARITHMETIC;
            expr.data.arithmetic.left_type = KRYON_EXPR_ROOT_HEIGHT;
            expr.data.arithmetic.left_value = 0;
            expr.data.arithmetic.operator = '-';
            
            // Parse the right side
            const char* minus_pos = strstr(value, " - ");
            if (minus_pos) {
                float right_val = atof(minus_pos + 3);
                expr.data.arithmetic.right_type = KRYON_EXPR_LITERAL;
                expr.data.arithmetic.right_value = right_val;
            }
            return expr;
        } else {
            // Simple $root.height
            KryonExpression expr = {0};
            expr.type = KRYON_EXPR_ROOT_HEIGHT;
            return expr;
        }
    }
    
    // Handle $parent variables
    if (strstr(value, "$parent.width")) {
        KryonExpression expr = {0};
        expr.type = KRYON_EXPR_PARENT_WIDTH;
        return expr;
    }
    
    if (strstr(value, "$parent.height")) {
        KryonExpression expr = {0};
        expr.type = KRYON_EXPR_PARENT_HEIGHT;
        return expr;
    }
    
    // Default: treat as literal numeric value
    return kryon_expression_literal(atof(value));
}

float kryon_expression_evaluate(const KryonExpression* expr, const KryonWidget* widget, const KryonWidget* root) {
    if (!expr) return 0.0f;
    
    switch (expr->type) {
        case KRYON_EXPR_LITERAL:
            return expr->data.literal_value;
            
        case KRYON_EXPR_PERCENTAGE: {
            // For percentage, we need the parent container size
            if (widget && widget->parent) {
                // Assuming width percentage is based on parent width
                return expr->data.percentage_value * widget->parent->computed_rect.size.width;
            }
            return expr->data.percentage_value * 100.0f; // Fallback
        }
        
        case KRYON_EXPR_ROOT_WIDTH:
            return root ? root->computed_rect.size.width : 800.0f;
            
        case KRYON_EXPR_ROOT_HEIGHT:
            return root ? root->computed_rect.size.height : 600.0f;
            
        case KRYON_EXPR_PARENT_WIDTH:
            return (widget && widget->parent) ? widget->parent->computed_rect.size.width : 0.0f;
            
        case KRYON_EXPR_PARENT_HEIGHT:
            return (widget && widget->parent) ? widget->parent->computed_rect.size.height : 0.0f;
            
        case KRYON_EXPR_ARITHMETIC: {
            float left_val = 0.0f;
            float right_val = expr->data.arithmetic.right_value;
            
            // Evaluate left side
            switch (expr->data.arithmetic.left_type) {
                case KRYON_EXPR_ROOT_WIDTH:
                    left_val = root ? root->computed_rect.size.width : 800.0f;
                    break;
                case KRYON_EXPR_ROOT_HEIGHT:
                    left_val = root ? root->computed_rect.size.height : 600.0f;
                    break;
                case KRYON_EXPR_PARENT_WIDTH:
                    left_val = (widget && widget->parent) ? widget->parent->computed_rect.size.width : 0.0f;
                    break;
                case KRYON_EXPR_PARENT_HEIGHT:
                    left_val = (widget && widget->parent) ? widget->parent->computed_rect.size.height : 0.0f;
                    break;
                default:
                    left_val = expr->data.arithmetic.left_value;
                    break;
            }
            
            // Perform arithmetic
            switch (expr->data.arithmetic.operator) {
                case '+': return left_val + right_val;
                case '-': return left_val - right_val;
                case '*': return left_val * right_val;
                case '/': return (right_val != 0.0f) ? left_val / right_val : left_val;
                default: return left_val;
            }
        }
    }
    
    return 0.0f;
}

void kryon_widget_update_reactive_properties(KryonWidget* widget, const KryonWidget* root) {
    if (!widget) return;
    
    // Only evaluate reactive expressions, literal values are already set
    if (kryon_expression_is_reactive(&widget->width_expr)) {
        widget->width = kryon_expression_evaluate(&widget->width_expr, widget, root);
    }
    
    if (kryon_expression_is_reactive(&widget->height_expr)) {
        widget->height = kryon_expression_evaluate(&widget->height_expr, widget, root);
    }
    
    // Update position from reactive expressions only
    if (kryon_expression_is_reactive(&widget->posX_expr)) {
        float posX = kryon_expression_evaluate(&widget->posX_expr, widget, root);
        widget->computed_rect.position.x = posX;
    }
    
    if (kryon_expression_is_reactive(&widget->posY_expr)) {
        float posY = kryon_expression_evaluate(&widget->posY_expr, widget, root);
        widget->computed_rect.position.y = posY;
    }
    
    // Mark widget as needing layout recalculation if any reactive property was updated
    if (kryon_expression_is_reactive(&widget->width_expr) || 
        kryon_expression_is_reactive(&widget->height_expr) ||
        kryon_expression_is_reactive(&widget->posX_expr) ||
        kryon_expression_is_reactive(&widget->posY_expr)) {
        kryon_widget_mark_dirty(widget);
    }
}

void kryon_widget_update_reactive_properties_recursive(KryonWidget* widget, const KryonWidget* root) {
    if (!widget) return;
    
    // Update this widget's reactive properties
    kryon_widget_update_reactive_properties(widget, root);
    
    // Update children recursively
    for (size_t i = 0; i < widget->child_count; i++) {
        kryon_widget_update_reactive_properties_recursive(widget->children[i], root);
    }
}