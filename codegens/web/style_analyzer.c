#define _GNU_SOURCE
#include "style_analyzer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Style Default Detection
// ============================================================================

bool ir_style_has_custom_values(IRStyle* style) {
    if (!style) return false;

    // Check colors (transparent = default)
    if (style->background.type != IR_COLOR_TRANSPARENT) return true;
    if (style->font.color.type != IR_COLOR_TRANSPARENT) return true;

    // Check dimensions (auto = default)
    if (style->width.type != IR_DIMENSION_AUTO) return true;
    if (style->height.type != IR_DIMENSION_AUTO) return true;

    // Check spacing (0 = default)
    if (style->margin.top != 0 || style->margin.right != 0 ||
        style->margin.bottom != 0 || style->margin.left != 0) return true;
    if (style->padding.top != 0 || style->padding.right != 0 ||
        style->padding.bottom != 0 || style->padding.left != 0) return true;

    // Check border (0 width = default)
    if (style->border.width > 0) return true;

    // Check typography
    if (style->font.size > 0) return true;
    if (style->font.bold) return true;
    if (style->font.italic) return true;
    if (style->font.weight > 0) return true;
    if (style->font.family != NULL) return true;
    if (style->font.line_height > 0 && style->font.line_height != 1.5f) return true;
    if (style->font.letter_spacing != 0) return true;
    if (style->font.word_spacing != 0) return true;
    if (style->font.align != IR_TEXT_ALIGN_LEFT) return true;
    if (style->font.decoration != IR_TEXT_DECORATION_NONE) return true;

    // Check transform (identity = default)
    if (style->transform.translate_x != 0 || style->transform.translate_y != 0) return true;
    if (style->transform.scale_x != 1.0f || style->transform.scale_y != 1.0f) return true;
    if (style->transform.rotate != 0) return true;

    // Check opacity and visibility (1.0 and true = defaults)
    if (style->opacity < 1.0f) return true;
    if (!style->visible) return true;

    // Check position mode (relative = default)
    if (style->position_mode != IR_POSITION_RELATIVE) return true;

    // Check z-index (0 = default)
    if (style->z_index > 0) return true;

    // Check overflow (visible = default)
    if (style->overflow_x != IR_OVERFLOW_VISIBLE) return true;
    if (style->overflow_y != IR_OVERFLOW_VISIBLE) return true;

    // Check effects
    if (style->box_shadow.enabled) return true;
    if (style->text_effect.shadow.enabled) return true;
    if (style->text_effect.overflow != IR_TEXT_OVERFLOW_VISIBLE) return true;
    if (style->text_effect.fade_type != IR_TEXT_FADE_NONE) return true;
    if (style->filter_count > 0) return true;

    // Check animations and transitions
    if (style->animation_count > 0) return true;
    if (style->transition_count > 0) return true;

    // Check responsive design
    if (style->breakpoint_count > 0) return true;
    if (style->pseudo_style_count > 0) return true;
    if (style->container_type != IR_CONTAINER_TYPE_NORMAL) return true;

    // All defaults
    return false;
}

// ============================================================================
// Event Handler Detection
// ============================================================================

bool has_event_handlers(IRComponent* component, IRLogicBlock* logic_block) {
    if (!component) return false;

    // Check legacy IREvent linked list
    if (component->events != NULL) return true;

    // Check logic block handlers (transpile mode)
    if (logic_block) {
        // Common event types to check
        const char* event_types[] = {
            "click", "change", "input", "focus", "blur",
            "mouseenter", "mouseleave", "keydown", "keyup", "submit"
        };

        for (size_t i = 0; i < sizeof(event_types) / sizeof(event_types[0]); i++) {
            const char* handler = ir_logic_block_get_handler(logic_block,
                                                              component->id,
                                                              event_types[i]);
            if (handler) return true;
        }
    }

    return false;
}

// ============================================================================
// Natural Class Name Generation
// ============================================================================

char* generate_natural_class_name(IRComponent* component) {
    if (!component) return NULL;

    // Priority 1: Use component->css_class if set (CSS class from original HTML)
    if (component->css_class && component->css_class[0] != '\0') {
        return strdup(component->css_class);
    }

    // Priority 2: Use component->tag if set (user custom class or semantic tag fallback)
    if (component->tag && component->tag[0] != '\0') {
        return strdup(component->tag);
    }

    // Priority 3: Natural semantic names based on component type
    const char* class_name = NULL;

    switch (component->type) {
        case IR_COMPONENT_BUTTON:
            class_name = "button";
            break;
        case IR_COMPONENT_ROW:
            class_name = "row";
            break;
        case IR_COMPONENT_COLUMN:
            class_name = "column";
            break;
        case IR_COMPONENT_CENTER:
            class_name = "center";
            break;
        case IR_COMPONENT_CONTAINER:
            class_name = "container";
            break;
        case IR_COMPONENT_TEXT:
            class_name = "text";
            break;
        case IR_COMPONENT_INPUT:
            class_name = "input";
            break;
        case IR_COMPONENT_CHECKBOX:
            class_name = "checkbox";
            break;
        case IR_COMPONENT_IMAGE:
            class_name = "image";
            break;
        case IR_COMPONENT_LINK:
            class_name = "link";
            break;
        case IR_COMPONENT_HEADING:
            class_name = "heading";
            break;
        case IR_COMPONENT_PARAGRAPH:
            class_name = "paragraph";
            break;
        case IR_COMPONENT_LIST:
            class_name = "list";
            break;
        case IR_COMPONENT_LIST_ITEM:
            class_name = "list-item";
            break;
        case IR_COMPONENT_TABLE:
            class_name = "table";
            break;
        case IR_COMPONENT_CODE_BLOCK:
            class_name = "code";
            break;
        default:
            // Generic fallback
            class_name = "element";
            break;
    }

    return strdup(class_name);
}

// ============================================================================
// Complete Style Analysis
// ============================================================================

StyleAnalysis* analyze_component_style(IRComponent* component, IRLogicBlock* logic_block) {
    if (!component) return NULL;

    StyleAnalysis* analysis = calloc(1, sizeof(StyleAnalysis));
    if (!analysis) return NULL;

    // Check if component has custom styling
    analysis->needs_css = ir_style_has_custom_values(component->style);

    // Check if component has event handlers
    analysis->needs_id = has_event_handlers(component, logic_block);

    // Generate natural class name (will be used if needs_css is true)
    analysis->suggested_class = generate_natural_class_name(component);

    return analysis;
}

void style_analysis_free(StyleAnalysis* analysis) {
    if (!analysis) return;

    if (analysis->suggested_class) {
        free(analysis->suggested_class);
    }

    free(analysis);
}
