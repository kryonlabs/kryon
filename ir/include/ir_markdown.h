#ifndef IR_MARKDOWN_H
#define IR_MARKDOWN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ir_core.h"

// ============================================================================
// Markdown Component Creation
// ============================================================================

IRComponent* ir_heading(uint8_t level, const char* text);
IRComponent* ir_paragraph(void);
IRComponent* ir_blockquote(void);
IRComponent* ir_code_block(const char* language, const char* code);
IRComponent* ir_horizontal_rule(void);
IRComponent* ir_list(IRListType type);
IRComponent* ir_list_item(void);
IRComponent* ir_link(const char* url, const char* text);

// ============================================================================
// Markdown Component Setters
// ============================================================================

// Heading setters
void ir_set_heading_level(IRComponent* comp, uint8_t level);
void ir_set_heading_id(IRComponent* comp, const char* id);

// Code block setters
void ir_set_code_language(IRComponent* comp, const char* language);
void ir_set_code_content(IRComponent* comp, const char* code, size_t length);
void ir_set_code_show_line_numbers(IRComponent* comp, bool show);

// List setters
void ir_set_list_type(IRComponent* comp, IRListType type);
void ir_set_list_start(IRComponent* comp, uint32_t start);
void ir_set_list_tight(IRComponent* comp, bool tight);

// List item setters
void ir_set_list_item_number(IRComponent* comp, uint32_t number);
void ir_set_list_item_marker(IRComponent* comp, const char* marker);
void ir_set_list_item_task(IRComponent* comp, bool is_task, bool checked);

// Link setters
void ir_set_link_url(IRComponent* comp, const char* url);
void ir_set_link_title(IRComponent* comp, const char* title);
void ir_set_link_target(IRComponent* comp, const char* target);
void ir_set_link_rel(IRComponent* comp, const char* rel);

// Blockquote setters
void ir_set_blockquote_depth(IRComponent* comp, uint8_t depth);

#endif // IR_MARKDOWN_H
