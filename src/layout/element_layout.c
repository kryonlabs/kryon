/**
 * @file widget_layout.c
 * @brief Flutter-inspired layout calculation algorithms
 */

#include "kryon/widget_system.h"
#include "internal/memory.h"
#include <math.h>
#include <float.h>

#ifdef HAVE_RAYLIB
#include <raylib.h>
#endif

// =============================================================================
// LAYOUT UTILITIES
// =============================================================================

/// Calculate intrinsic size for a widget
static KryonSize calculate_intrinsic_size(KryonWidget* widget) {
    KryonSize size = {0, 0};
    
    switch (widget->type) {
        case KRYON_WIDGET_TEXT: {
            // Text size calculation
            const char* text = widget->props.text_props.text;
            float font_size = widget->props.text_props.style.font_size;
            
            if (text && font_size > 0) {
                // If explicit width/height are set (by update_text_sizes_with_raylib), use them
                if (widget->width > 0 && widget->height > 0) {
                    size.width = widget->width;
                    size.height = widget->height;
                } else {
                    // Fallback approximation
                    size.width = strlen(text) * font_size * 0.6f;
                    size.height = font_size;
                }
            }
            break;
        }
        
        case KRYON_WIDGET_BUTTON: {
            // Button size based on improved text measurement + padding
            const char* label = widget->props.button_props.label;
            if (label) {
                // Use consistent font size with runtime system, default to 20 for buttons
                float font_size = 20.0f; // Default button text size
                
                // Use improved character width approximation (more accurate than before)
                // This should match the auto-sizing system in runtime.c
                float text_width = strlen(label) * font_size * 0.55f; // Refined ratio
                size.width = text_width + 24.0f; // Text width + padding (12px each side) 
                size.height = font_size + 20.0f; // Text height + padding (10px top/bottom)
            } else {
                // Default button size when no text
                size.width = 150.0f;
                size.height = 40.0f;
            }
            break;
        }
        
        case KRYON_WIDGET_INPUT: {
            // Standard input field size
            size.width = 200;
            size.height = 36;
            break;
        }
        
        case KRYON_WIDGET_IMAGE: {
            // Default image size (would be actual image dimensions in real impl)
            size.width = 100;
            size.height = 100;
            break;
        }
        
        default:
            // For layout widgets and others, size is determined by children
            break;
    }
    
    return size;
}

/// Apply size constraints to a size
static KryonSize apply_size_constraints(KryonWidget* widget, KryonSize size) {
    // Apply explicit width/height
    if (widget->width >= 0) size.width = widget->width;
    if (widget->height >= 0) size.height = widget->height;
    
    // Apply min/max constraints
    if (size.width < widget->min_width) size.width = widget->min_width;
    if (size.height < widget->min_height) size.height = widget->min_height;
    if (size.width > widget->max_width) size.width = widget->max_width;
    if (size.height > widget->max_height) size.height = widget->max_height;
    
    return size;
}

/// Get available space after padding
static KryonSize get_content_size(KryonWidget* widget, KryonSize available_space) {
    KryonSize content_size;
    content_size.width = available_space.width - widget->padding.left - widget->padding.right;
    content_size.height = available_space.height - widget->padding.top - widget->padding.bottom;
    
    // Ensure non-negative
    if (content_size.width < 0) content_size.width = 0;
    if (content_size.height < 0) content_size.height = 0;
    
    return content_size;
}

/// Position widget with padding offset
static KryonVec2 get_content_position(KryonWidget* widget, KryonVec2 position) {
    return (KryonVec2){
        position.x + widget->padding.left,
        position.y + widget->padding.top
    };
}

// =============================================================================
// LAYOUT ALGORITHMS
// =============================================================================

void kryon_layout_column(KryonWidget* widget, KryonSize available_space) {
    if (!widget || widget->type != KRYON_WIDGET_COLUMN) return;
    
    KryonSize content_size = get_content_size(widget, available_space);
    KryonVec2 content_pos = get_content_position(widget, (KryonVec2){0, 0});
    
    // Calculate total flex factor and fixed heights
    int total_flex = 0;
    float total_fixed_height = 0;
    float total_spacing = (widget->child_count > 1) ? (widget->child_count - 1) * widget->spacing : 0;
    
    for (size_t i = 0; i < widget->child_count; i++) {
        KryonWidget* child = widget->children[i];
        if (child->flex > 0) {
            total_flex += child->flex;
        } else {
            KryonSize child_intrinsic = calculate_intrinsic_size(child);
            child_intrinsic = apply_size_constraints(child, child_intrinsic);
            total_fixed_height += child_intrinsic.height;
        }
    }
    
    // Calculate remaining space for flex children
    float remaining_height = content_size.height - total_fixed_height - total_spacing;
    if (remaining_height < 0) remaining_height = 0;
    
    // Layout children
    float current_y = content_pos.y;
    
    // Handle main axis alignment for non-flex layouts
    if (total_flex == 0 && widget->main_axis != KRYON_MAIN_START) {
        float total_child_height = total_fixed_height + total_spacing;
        float extra_space = content_size.height - total_child_height;
        
        if (extra_space > 0) {
            switch (widget->main_axis) {
                case KRYON_MAIN_CENTER:
                    current_y += extra_space / 2;
                    break;
                case KRYON_MAIN_END:
                    current_y += extra_space;
                    break;
                case KRYON_MAIN_SPACE_BETWEEN:
                    if (widget->child_count > 1) {
                        widget->spacing += extra_space / (widget->child_count - 1);
                    }
                    break;
                case KRYON_MAIN_SPACE_AROUND:
                    if (widget->child_count > 0) {
                        float space_per_child = extra_space / widget->child_count;
                        current_y += space_per_child / 2;
                        widget->spacing += space_per_child;
                    }
                    break;
                case KRYON_MAIN_SPACE_EVENLY:
                    if (widget->child_count > 0) {
                        float space_per_gap = extra_space / (widget->child_count + 1);
                        current_y += space_per_gap;
                        widget->spacing += space_per_gap;
                    }
                    break;
                default:
                    break;
            }
        }
    }
    
    for (size_t i = 0; i < widget->child_count; i++) {
        KryonWidget* child = widget->children[i];
        
        // Calculate child size
        KryonSize child_size;
        if (child->flex > 0) {
            child_size.height = (remaining_height * child->flex) / total_flex;
            child_size.width = content_size.width;
        } else {
            child_size = calculate_intrinsic_size(child);
            child_size = apply_size_constraints(child, child_size);
        }
        
        // Apply cross axis alignment
        float child_x = content_pos.x;
        switch (widget->cross_axis) {
            case KRYON_CROSS_START:
                // Already at start
                break;
            case KRYON_CROSS_CENTER:
                child_x += (content_size.width - child_size.width) / 2;
                break;
            case KRYON_CROSS_END:
                child_x += content_size.width - child_size.width;
                break;
            case KRYON_CROSS_STRETCH:
                child_size.width = content_size.width;
                break;
        }
        
        // Set child position and size
        child->computed_rect = (KryonRect){
            {child_x, current_y},
            child_size
        };
        
        // Recursively layout child
        if (kryon_widget_is_layout_type(child->type)) {
            kryon_widget_calculate_layout(child, child_size);
        }
        
        current_y += child_size.height + widget->spacing;
    }
    
    // Set our own computed rect
    widget->computed_rect.size = apply_size_constraints(widget, available_space);
}

void kryon_layout_row(KryonWidget* widget, KryonSize available_space) {
    if (!widget || widget->type != KRYON_WIDGET_ROW) return;
    
    KryonSize content_size = get_content_size(widget, available_space);
    KryonVec2 content_pos = get_content_position(widget, (KryonVec2){0, 0});
    
    // Calculate total flex factor and fixed widths
    int total_flex = 0;
    float total_fixed_width = 0;
    float total_spacing = (widget->child_count > 1) ? (widget->child_count - 1) * widget->spacing : 0;
    
    for (size_t i = 0; i < widget->child_count; i++) {
        KryonWidget* child = widget->children[i];
        if (child->flex > 0) {
            total_flex += child->flex;
        } else {
            KryonSize child_intrinsic = calculate_intrinsic_size(child);
            child_intrinsic = apply_size_constraints(child, child_intrinsic);
            total_fixed_width += child_intrinsic.width;
        }
    }
    
    // Calculate remaining space for flex children
    float remaining_width = content_size.width - total_fixed_width - total_spacing;
    if (remaining_width < 0) remaining_width = 0;
    
    // Layout children
    float current_x = content_pos.x;
    
    // Handle main axis alignment for non-flex layouts
    if (total_flex == 0 && widget->main_axis != KRYON_MAIN_START) {
        float total_child_width = total_fixed_width + total_spacing;
        float extra_space = content_size.width - total_child_width;
        
        if (extra_space > 0) {
            switch (widget->main_axis) {
                case KRYON_MAIN_CENTER:
                    current_x += extra_space / 2;
                    break;
                case KRYON_MAIN_END:
                    current_x += extra_space;
                    break;
                case KRYON_MAIN_SPACE_BETWEEN:
                    if (widget->child_count > 1) {
                        widget->spacing += extra_space / (widget->child_count - 1);
                    }
                    break;
                case KRYON_MAIN_SPACE_AROUND:
                    if (widget->child_count > 0) {
                        float space_per_child = extra_space / widget->child_count;
                        current_x += space_per_child / 2;
                        widget->spacing += space_per_child;
                    }
                    break;
                case KRYON_MAIN_SPACE_EVENLY:
                    if (widget->child_count > 0) {
                        float space_per_gap = extra_space / (widget->child_count + 1);
                        current_x += space_per_gap;
                        widget->spacing += space_per_gap;
                    }
                    break;
                default:
                    break;
            }
        }
    }
    
    for (size_t i = 0; i < widget->child_count; i++) {
        KryonWidget* child = widget->children[i];
        
        // Calculate child size
        KryonSize child_size;
        if (child->flex > 0) {
            child_size.width = (remaining_width * child->flex) / total_flex;
            child_size.height = content_size.height;
        } else {
            child_size = calculate_intrinsic_size(child);
            child_size = apply_size_constraints(child, child_size);
        }
        
        // Apply cross axis alignment
        float child_y = content_pos.y;
        switch (widget->cross_axis) {
            case KRYON_CROSS_START:
                // Already at start
                break;
            case KRYON_CROSS_CENTER:
                child_y += (content_size.height - child_size.height) / 2;
                break;
            case KRYON_CROSS_END:
                child_y += content_size.height - child_size.height;
                break;
            case KRYON_CROSS_STRETCH:
                child_size.height = content_size.height;
                break;
        }
        
        // Set child position and size
        child->computed_rect = (KryonRect){
            {current_x, child_y},
            child_size
        };
        
        // Recursively layout child
        if (kryon_widget_is_layout_type(child->type)) {
            kryon_widget_calculate_layout(child, child_size);
        }
        
        current_x += child_size.width + widget->spacing;
    }
    
    // Set our own computed rect
    widget->computed_rect.size = apply_size_constraints(widget, available_space);
}

void kryon_layout_stack(KryonWidget* widget, KryonSize available_space) {
    if (!widget || widget->type != KRYON_WIDGET_STACK) return;
    
    KryonSize content_size = get_content_size(widget, available_space);
    KryonVec2 content_pos = get_content_position(widget, (KryonVec2){0, 0});
    
    // In stack layout, all children are positioned at the same location
    // and sized to fit the available space (or their intrinsic size)
    for (size_t i = 0; i < widget->child_count; i++) {
        KryonWidget* child = widget->children[i];
        
        KryonSize child_size = calculate_intrinsic_size(child);
        child_size = apply_size_constraints(child, child_size);
        
        // If child doesn't have explicit size, use available space
        if (child->width < 0 && child_size.width == 0) {
            child_size.width = content_size.width;
        }
        if (child->height < 0 && child_size.height == 0) {
            child_size.height = content_size.height;
        }
        
        // Position child based on alignment
        float child_x = content_pos.x;
        float child_y = content_pos.y;
        
        // Apply main axis alignment (horizontal for stack)
        switch (widget->main_axis) {
            case KRYON_MAIN_START:
                // Already at start
                break;
            case KRYON_MAIN_CENTER:
                child_x += (content_size.width - child_size.width) / 2;
                break;
            case KRYON_MAIN_END:
                child_x += content_size.width - child_size.width;
                break;
            default:
                break;
        }
        
        // Apply cross axis alignment (vertical for stack)
        switch (widget->cross_axis) {
            case KRYON_CROSS_START:
                // Already at start
                break;
            case KRYON_CROSS_CENTER:
                child_y += (content_size.height - child_size.height) / 2;
                break;
            case KRYON_CROSS_END:
                child_y += content_size.height - child_size.height;
                break;
            case KRYON_CROSS_STRETCH:
                child_size.width = content_size.width;
                child_size.height = content_size.height;
                break;
        }
        
        // Set child position and size
        child->computed_rect = (KryonRect){
            {child_x, child_y},
            child_size
        };
        
        // Recursively layout child
        if (kryon_widget_is_layout_type(child->type)) {
            kryon_widget_calculate_layout(child, child_size);
        }
    }
    
    // Set our own computed rect
    widget->computed_rect.size = apply_size_constraints(widget, available_space);
}

/// Apply content alignment to position a child within container
static KryonVec2 apply_content_alignment(KryonWidget* container, KryonSize container_size, KryonSize child_size, KryonVec2 base_pos) {
    float child_x = base_pos.x;
    float child_y = base_pos.y;
    
    // Use combined alignment if set, otherwise use separate X/Y alignment
    if (container->content_alignment != KRYON_CONTENT_TOP_LEFT) {
        switch (container->content_alignment) {
            case KRYON_CONTENT_TOP_LEFT:
                // Already at top-left
                break;
            case KRYON_CONTENT_TOP_CENTER:
                child_x += (container_size.width - child_size.width) / 2;
                break;
            case KRYON_CONTENT_TOP_RIGHT:
                child_x += container_size.width - child_size.width;
                break;
            case KRYON_CONTENT_CENTER_LEFT:
                child_y += (container_size.height - child_size.height) / 2;
                break;
            case KRYON_CONTENT_CENTER:
                child_x += (container_size.width - child_size.width) / 2;
                child_y += (container_size.height - child_size.height) / 2;
                break;
            case KRYON_CONTENT_CENTER_RIGHT:
                child_x += container_size.width - child_size.width;
                child_y += (container_size.height - child_size.height) / 2;
                break;
            case KRYON_CONTENT_BOTTOM_LEFT:
                child_y += container_size.height - child_size.height;
                break;
            case KRYON_CONTENT_BOTTOM_CENTER:
                child_x += (container_size.width - child_size.width) / 2;
                child_y += container_size.height - child_size.height;
                break;
            case KRYON_CONTENT_BOTTOM_RIGHT:
                child_x += container_size.width - child_size.width;
                child_y += container_size.height - child_size.height;
                break;
        }
    } else {
        // Use separate X/Y alignment
        switch (container->content_alignment_x) {
            case KRYON_ALIGN_LEFT:
                // Already at left
                break;
            case KRYON_ALIGN_CENTER_H:
                child_x += (container_size.width - child_size.width) / 2;
                break;
            case KRYON_ALIGN_RIGHT:
                child_x += container_size.width - child_size.width;
                break;
        }
        
        switch (container->content_alignment_y) {
            case KRYON_ALIGN_TOP:
                // Already at top
                break;
            case KRYON_ALIGN_CENTER_V:
                child_y += (container_size.height - child_size.height) / 2;
                break;
            case KRYON_ALIGN_BOTTOM:
                child_y += container_size.height - child_size.height;
                break;
        }
    }
    
    return (KryonVec2){child_x, child_y};
}

/// Apply content distribution for multiple children
static void apply_content_distribution(KryonWidget* container, KryonSize container_size, KryonVec2 base_pos, bool is_horizontal) {
    if (container->child_count <= 1 || container->content_distribution == KRYON_DISTRIBUTE_NONE) {
        return;
    }
    
    // Calculate total child size
    float total_child_size = 0;
    for (size_t i = 0; i < container->child_count; i++) {
        KryonWidget* child = container->children[i];
        if (is_horizontal) {
            total_child_size += child->computed_rect.size.width;
        } else {
            total_child_size += child->computed_rect.size.height;
        }
    }
    
    float available_space = is_horizontal ? container_size.width : container_size.height;
    float extra_space = available_space - total_child_size;
    
    if (extra_space <= 0) return;
    
    float spacing = 0;
    float offset = 0;
    
    switch (container->content_distribution) {
        case KRYON_DISTRIBUTE_NONE:
            return;
            
        case KRYON_DISTRIBUTE_SPACE_BETWEEN:
            spacing = extra_space / (container->child_count - 1);
            break;
            
        case KRYON_DISTRIBUTE_SPACE_AROUND:
            spacing = extra_space / container->child_count;
            offset = spacing / 2;
            break;
            
        case KRYON_DISTRIBUTE_SPACE_EVENLY:
            spacing = extra_space / (container->child_count + 1);
            offset = spacing;
            break;
    }
    
    // Apply distribution
    float current_pos = is_horizontal ? base_pos.x + offset : base_pos.y + offset;
    
    for (size_t i = 0; i < container->child_count; i++) {
        KryonWidget* child = container->children[i];
        
        if (is_horizontal) {
            child->computed_rect.position.x = current_pos;
            current_pos += child->computed_rect.size.width + spacing;
        } else {
            child->computed_rect.position.y = current_pos;
            current_pos += child->computed_rect.size.height + spacing;
        }
    }
}

void kryon_layout_container(KryonWidget* widget, KryonSize available_space) {
    if (!widget || widget->type != KRYON_WIDGET_CONTAINER) return;
    
    // For containers with explicit position (posX/posY), preserve their position
    // Store the original position before any layout calculations
    KryonVec2 original_position = widget->computed_rect.position;
    
    KryonSize content_size = get_content_size(widget, available_space);
    KryonVec2 content_pos = get_content_position(widget, original_position);
    
    if (widget->child_count == 1) {
        // Single child - use content alignment
        KryonWidget* child = widget->children[0];
        
        KryonSize child_size = calculate_intrinsic_size(child);
        child_size = apply_size_constraints(child, child_size);
        
        // If child doesn't have explicit size, use available space
        if (child->width < 0 && child_size.width == 0) {
            child_size.width = content_size.width;
        }
        if (child->height < 0 && child_size.height == 0) {
            child_size.height = content_size.height;
        }
        
        // Check if child has explicit positioning (non-zero posX/posY)
        KryonVec2 child_original_pos = child->computed_rect.position;
        KryonVec2 child_pos;
        
        if (child_original_pos.x != 0 || child_original_pos.y != 0) {
            // Child has explicit positioning - preserve it
            child_pos = child_original_pos;
        } else {
            // Child has no explicit positioning - apply content alignment
            child_pos = apply_content_alignment(widget, content_size, child_size, content_pos);
        }
        
        // Set child position and size
        child->computed_rect = (KryonRect){child_pos, child_size};
        
        // Recursively layout child
        if (kryon_widget_is_layout_type(child->type)) {
            kryon_widget_calculate_layout(child, child_size);
        }
    } else if (widget->child_count > 1) {
        // Multiple children - calculate sizes first, then apply distribution
        for (size_t i = 0; i < widget->child_count; i++) {
            KryonWidget* child = widget->children[i];
            
            KryonSize child_size = calculate_intrinsic_size(child);
            child_size = apply_size_constraints(child, child_size);
            
            // Check if child has explicit positioning (non-zero posX/posY)
            KryonVec2 child_original_pos = child->computed_rect.position;
            KryonVec2 child_pos;
            
            if (child_original_pos.x != 0 || child_original_pos.y != 0) {
                // Child has explicit positioning - preserve it
                child_pos = child_original_pos;
            } else {
                // Child has no explicit positioning - position at content position
                child_pos = content_pos;
            }
            
            // Set child position and size
            child->computed_rect = (KryonRect){child_pos, child_size};
            
            // Recursively layout child
            if (kryon_widget_is_layout_type(child->type)) {
                kryon_widget_calculate_layout(child, child_size);
            }
        }
        
        // Apply content distribution (assume vertical by default)
        apply_content_distribution(widget, content_size, content_pos, false);
    }
    
    // Set our own computed rect size, but preserve the original position
    widget->computed_rect.size = apply_size_constraints(widget, available_space);
    widget->computed_rect.position = original_position;
}

// =============================================================================
// MAIN LAYOUT FUNCTION
// =============================================================================

void kryon_widget_calculate_layout(KryonWidget* root, KryonSize available_space) {
    if (!root) return;
    
    // Skip layout if not dirty and size hasn't changed
    if (!root->layout_dirty && 
        root->computed_rect.size.width == available_space.width &&
        root->computed_rect.size.height == available_space.height) {
        return;
    }
    
    // Update reactive properties for this widget and all children
    kryon_widget_update_reactive_properties_recursive(root, root);
    
    // Store the original position before layout calculations
    // This preserves explicit positioning (posX/posY) from KRY files
    KryonVec2 original_position = root->computed_rect.position;
    
    // Calculate layout based on widget type
    switch (root->type) {
        case KRYON_WIDGET_COLUMN:
            kryon_layout_column(root, available_space);
            break;
            
        case KRYON_WIDGET_ROW:
            kryon_layout_row(root, available_space);
            break;
            
        case KRYON_WIDGET_STACK:
            kryon_layout_stack(root, available_space);
            break;
            
        case KRYON_WIDGET_CONTAINER:
            kryon_layout_container(root, available_space);
            break;
            
        case KRYON_WIDGET_EXPANDED:
        case KRYON_WIDGET_FLEXIBLE:
            // These are handled by their parent's layout algorithm
            // They shouldn't be root widgets, but handle gracefully
            root->computed_rect.size = available_space;
            break;
            
        default:
            // Non-layout widgets: use intrinsic size
            root->computed_rect.size = calculate_intrinsic_size(root);
            root->computed_rect.size = apply_size_constraints(root, root->computed_rect.size);
            break;
    }
    
    // Restore the original position if it was explicitly set (non-zero)
    // This ensures posX/posY from KRY files are preserved
    if (original_position.x != 0 || original_position.y != 0) {
        root->computed_rect.position = original_position;
    }
    
    // Clear dirty flag
    root->layout_dirty = false;
}