// IR Markdown Module
// Markdown component implementation extracted from ir_builder.c

#define _GNU_SOURCE
#include "ir_markdown.h"
#include "../include/ir_builder.h"
#include "ir_style_builder.h"
#include "../include/ir_component_factory.h"
#include "../utils/ir_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Markdown Component Creation
// ============================================================================

IRComponent* ir_heading(uint8_t level, const char* text) {
    if (level < 1) level = 1;
    if (level > 6) level = 6;

    IRComponent* comp = ir_create_component(IR_COMPONENT_HEADING);
    if (!comp) return NULL;

    IRHeadingData* data = (IRHeadingData*)calloc(1, sizeof(IRHeadingData));
    data->level = level;
    data->text = text ? strdup(text) : NULL;
    data->id = NULL;
    comp->custom_data = (char*)data;

    // Set default styling based on level
    IRStyle* style = ir_get_style(comp);
    float font_sizes[] = {32.0f, 28.0f, 24.0f, 20.0f, 18.0f, 16.0f};
    ir_set_font(style, font_sizes[level - 1], NULL, 255, 255, 255, 255, true, false);
    ir_set_margin(comp, 24.0f, 0.0f, 12.0f, 0.0f);

    return comp;
}

IRComponent* ir_paragraph(void) {
    IRComponent* comp = ir_create_component(IR_COMPONENT_PARAGRAPH);
    if (!comp) return NULL;

    // Set default paragraph styling
    // Text wrapping is handled automatically by the backend based on component type
    IRStyle* style = ir_get_style(comp);
    ir_set_font(style, 16.0f, NULL, 230, 237, 243, 255, false, false);  // Light gray text for readability
    ir_set_margin(comp, 0.0f, 0.0f, 16.0f, 0.0f);

    return comp;
}

IRComponent* ir_blockquote(void) {
    IRComponent* comp = ir_create_component(IR_COMPONENT_BLOCKQUOTE);
    if (!comp) return NULL;

    IRBlockquoteData* data = (IRBlockquoteData*)calloc(1, sizeof(IRBlockquoteData));
    data->depth = 1;
    comp->custom_data = (char*)data;

    // Set default blockquote styling
    IRStyle* style = ir_get_style(comp);
    ir_set_padding(comp, 12.0f, 16.0f, 12.0f, 16.0f);
    ir_set_margin(comp, 0.0f, 0.0f, 16.0f, 0.0f);
    ir_set_background_color(style, 40, 44, 52, 255);  // Dark background
    ir_set_border(style, 4.0f, 80, 120, 200, 255, 0);  // Left border (blue-ish)

    return comp;
}

IRComponent* ir_code_block(const char* language, const char* code) {
    IRComponent* comp = ir_create_component(IR_COMPONENT_CODE_BLOCK);
    if (!comp) return NULL;

    IRCodeBlockData* data = (IRCodeBlockData*)calloc(1, sizeof(IRCodeBlockData));
    data->language = language ? strdup(language) : NULL;
    data->code = code ? strdup(code) : NULL;
    data->length = code ? strlen(code) : 0;
    data->show_line_numbers = false;
    data->start_line = 1;
    comp->custom_data = (char*)data;

    // Set default code block styling
    IRStyle* style = ir_get_style(comp);
    ir_set_font(style, 14.0f, "monospace", 220, 220, 220, 255, false, false);
    ir_set_background_color(style, 22, 27, 34, 255);  // Dark background
    ir_set_padding(comp, 16.0f, 16.0f, 16.0f, 16.0f);
    ir_set_margin(comp, 0.0f, 0.0f, 16.0f, 0.0f);

    return comp;
}

IRComponent* ir_horizontal_rule(void) {
    IRComponent* comp = ir_create_component(IR_COMPONENT_HORIZONTAL_RULE);
    if (!comp) return NULL;

    // Set default HR styling
    IRStyle* style = ir_get_style(comp);
    ir_set_height(comp, IR_DIMENSION_PX, 2.0f);
    ir_set_width(comp, IR_DIMENSION_PERCENT, 100.0f);
    ir_set_background_color(style, 80, 80, 80, 255);
    ir_set_margin(comp, 24.0f, 0.0f, 24.0f, 0.0f);

    return comp;
}

IRComponent* ir_list(IRListType type) {
    IRComponent* comp = ir_create_component(IR_COMPONENT_LIST);
    if (!comp) return NULL;

    IRListData* data = (IRListData*)calloc(1, sizeof(IRListData));
    data->type = type;
    data->start = 1;
    data->tight = true;
    comp->custom_data = (char*)data;

    // Set default list styling
    IRStyle* style __attribute__((unused)) = ir_get_style(comp);
    ir_set_margin(comp, 0.0f, 0.0f, 16.0f, 0.0f);
    ir_set_padding(comp, 0.0f, 0.0f, 0.0f, 24.0f);  // Left padding for indentation

    return comp;
}

IRComponent* ir_list_item(void) {
    IRComponent* comp = ir_create_component(IR_COMPONENT_LIST_ITEM);
    if (!comp) return NULL;

    IRListItemData* data = (IRListItemData*)calloc(1, sizeof(IRListItemData));
    data->number = 0;
    data->marker = NULL;
    data->is_task_item = false;
    data->is_checked = false;
    comp->custom_data = (char*)data;

    // Set default list item styling
    IRStyle* style __attribute__((unused)) = ir_get_style(comp);
    ir_set_margin(comp, 0.0f, 0.0f, 8.0f, 0.0f);

    return comp;
}

IRComponent* ir_link(const char* url, const char* text) {
    IRComponent* comp = ir_create_component(IR_COMPONENT_LINK);
    if (!comp) return NULL;

    IRLinkData* data = (IRLinkData*)calloc(1, sizeof(IRLinkData));
    data->url = url ? strdup(url) : NULL;
    data->title = NULL;
    comp->custom_data = (char*)data;

    // Set default link styling (underlined blue text)
    IRStyle* style = ir_get_style(comp);
    ir_set_font(style, 16.0f, NULL, 100, 150, 255, 255, false, false);
    // Note: Text decoration flags are set via style->text_decorations (not text_decoration)

    // If text is provided, create a Text child component
    if (text) {
        IRComponent* text_comp = ir_text(text);
        if (text_comp) {
            ir_add_child(comp, text_comp);
        }
    }

    return comp;
}

// ============================================================================
// Markdown Component Setters
// ============================================================================

// Heading setters
void ir_set_heading_level(IRComponent* comp, uint8_t level) {
    if (!comp || comp->type != IR_COMPONENT_HEADING) return;
    IRHeadingData* data = (IRHeadingData*)comp->custom_data;
    if (!data) return;

    if (level < 1) level = 1;
    if (level > 6) level = 6;
    data->level = level;

    // Update font size based on level
    float font_sizes[] = {32.0f, 28.0f, 24.0f, 20.0f, 18.0f, 16.0f};
    IRStyle* style = ir_get_style(comp);
    style->font.size = font_sizes[level - 1];
}

void ir_set_heading_id(IRComponent* comp, const char* id) {
    if (!comp || comp->type != IR_COMPONENT_HEADING) return;
    IRHeadingData* data = (IRHeadingData*)comp->custom_data;
    if (!data) return;

    if (data->id) free(data->id);
    data->id = id ? strdup(id) : NULL;
}

// Code block setters
void ir_set_code_language(IRComponent* comp, const char* language) {
    if (!comp || comp->type != IR_COMPONENT_CODE_BLOCK) return;
    IRCodeBlockData* data = (IRCodeBlockData*)comp->custom_data;
    if (!data) return;

    if (data->language) free(data->language);
    data->language = language ? strdup(language) : NULL;
}

void ir_set_code_content(IRComponent* comp, const char* code, size_t length) {
    if (!comp || comp->type != IR_COMPONENT_CODE_BLOCK) return;
    IRCodeBlockData* data = (IRCodeBlockData*)comp->custom_data;
    if (!data) return;

    if (data->code) free(data->code);
    if (code && length > 0) {
        data->code = (char*)malloc(length + 1);
        memcpy(data->code, code, length);
        data->code[length] = '\0';
        data->length = length;
    } else {
        data->code = NULL;
        data->length = 0;
    }
}

void ir_set_code_show_line_numbers(IRComponent* comp, bool show) {
    if (!comp || comp->type != IR_COMPONENT_CODE_BLOCK) return;
    IRCodeBlockData* data = (IRCodeBlockData*)comp->custom_data;
    if (!data) return;
    data->show_line_numbers = show;
}

// List setters
void ir_set_list_type(IRComponent* comp, IRListType type) {
    if (!comp || comp->type != IR_COMPONENT_LIST) return;
    IRListData* data = (IRListData*)comp->custom_data;
    if (!data) return;
    data->type = type;
}

void ir_set_list_start(IRComponent* comp, uint32_t start) {
    if (!comp || comp->type != IR_COMPONENT_LIST) return;
    IRListData* data = (IRListData*)comp->custom_data;
    if (!data) return;
    data->start = start;
}

void ir_set_list_tight(IRComponent* comp, bool tight) {
    if (!comp || comp->type != IR_COMPONENT_LIST) return;
    IRListData* data = (IRListData*)comp->custom_data;
    if (!data) return;
    data->tight = tight;
}

// List item setters
void ir_set_list_item_number(IRComponent* comp, uint32_t number) {
    if (!comp || comp->type != IR_COMPONENT_LIST_ITEM) return;
    IRListItemData* data = (IRListItemData*)comp->custom_data;
    if (!data) return;
    data->number = number;
}

void ir_set_list_item_marker(IRComponent* comp, const char* marker) {
    if (!comp || comp->type != IR_COMPONENT_LIST_ITEM) return;
    IRListItemData* data = (IRListItemData*)comp->custom_data;
    if (!data) return;

    if (data->marker) free(data->marker);
    data->marker = marker ? strdup(marker) : NULL;
}

void ir_set_list_item_task(IRComponent* comp, bool is_task, bool checked) {
    if (!comp || comp->type != IR_COMPONENT_LIST_ITEM) return;
    IRListItemData* data = (IRListItemData*)comp->custom_data;
    if (!data) return;
    data->is_task_item = is_task;
    data->is_checked = checked;
}

// Link setters
void ir_set_link_url(IRComponent* comp, const char* url) {
    if (!comp || comp->type != IR_COMPONENT_LINK) return;
    IRLinkData* data = (IRLinkData*)comp->custom_data;
    if (!data) return;

    if (data->url) free(data->url);
    data->url = url ? strdup(url) : NULL;
}

void ir_set_link_title(IRComponent* comp, const char* title) {
    if (!comp || comp->type != IR_COMPONENT_LINK) return;
    IRLinkData* data = (IRLinkData*)comp->custom_data;
    if (!data) return;

    if (data->title) free(data->title);
    data->title = title ? strdup(title) : NULL;
}

void ir_set_link_target(IRComponent* comp, const char* target) {
    if (!comp || comp->type != IR_COMPONENT_LINK) return;
    IRLinkData* data = (IRLinkData*)comp->custom_data;
    if (!data) return;

    if (data->target) free(data->target);
    data->target = target ? strdup(target) : NULL;
}

void ir_set_link_rel(IRComponent* comp, const char* rel) {
    if (!comp || comp->type != IR_COMPONENT_LINK) return;
    IRLinkData* data = (IRLinkData*)comp->custom_data;
    if (!data) return;

    if (data->rel) free(data->rel);
    data->rel = rel ? strdup(rel) : NULL;
}

// Blockquote setters
void ir_set_blockquote_depth(IRComponent* comp, uint8_t depth) {
    if (!comp || comp->type != IR_COMPONENT_BLOCKQUOTE) return;
    IRBlockquoteData* data = (IRBlockquoteData*)comp->custom_data;
    if (!data) return;
    data->depth = depth;
}
