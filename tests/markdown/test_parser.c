/*
 * Unit tests for Kryon markdown parser
 * Tests AST parsing and conversion to IR components
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../ir/ir_core.h"
#include "../../ir/ir_builder.h"
#include "../../ir/ir_markdown_parser.h"

// Test counter
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    printf("  Testing %s... ", name); \
    fflush(stdout);

#define PASS() \
    printf("✅ PASS\n"); \
    tests_passed++;

#define FAIL(msg) \
    printf("❌ FAIL: %s\n", msg); \
    tests_failed++;

// Helper: count children
static int count_children(IRComponent* comp) {
    return comp->child_count;
}

// Helper: find child by type
static IRComponent* find_child_by_type(IRComponent* parent, IRComponentType type) {
    for (uint32_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i]->type == type) {
            return parent->children[i];
        }
    }
    return NULL;
}

// Test 1: Parse simple heading
void test_simple_heading(void) {
    TEST("simple heading");

    const char* md = "# Hello World\n";
    IRComponent* root = ir_markdown_parse(md, 0);

    if (!root) {
        FAIL("Failed to parse");
        return;
    }

    // Root should be a Column
    if (root->type != IR_COMPONENT_COLUMN) {
        FAIL("Root should be Column");
        ir_destroy_component(root);
        return;
    }

    // Should have one child (heading)
    if (root->child_count == 0) {
        FAIL("Root has no children");
        ir_destroy_component(root);
        return;
    }

    IRComponent* heading = root->children[0];
    if (heading->type != IR_COMPONENT_HEADING) {
        FAIL("Expected Heading component");
        ir_destroy_component(root);
        return;
    }

    IRHeadingData* data = (IRHeadingData*)heading->custom_data;
    if (!data || data->level != 1) {
        FAIL("Heading level should be 1");
        ir_destroy_component(root);
        return;
    }

    if (!data->text || strcmp(data->text, "Hello World") != 0) {
        FAIL("Heading text mismatch");
        ir_destroy_component(root);
        return;
    }

    ir_destroy_component(root);
    PASS();
}

// Test 2: Parse paragraph
void test_paragraph(void) {
    TEST("paragraph");

    const char* md = "This is a simple paragraph.\n";
    IRComponent* root = ir_markdown_parse(md, 0);

    if (!root) {
        FAIL("Failed to parse");
        return;
    }

    IRComponent* para = find_child_by_type(root, IR_COMPONENT_PARAGRAPH);
    if (!para) {
        FAIL("Expected Paragraph component");
        ir_destroy_component(root);
        return;
    }

    ir_destroy_component(root);
    PASS();
}

// Test 3: Parse list
void test_list(void) {
    TEST("unordered list");

    const char* md = "- Item 1\n- Item 2\n- Item 3\n";
    IRComponent* root = ir_markdown_parse(md, 0);

    if (!root) {
        FAIL("Failed to parse");
        return;
    }

    IRComponent* list = find_child_by_type(root, IR_COMPONENT_LIST);
    if (!list) {
        FAIL("Expected List component");
        ir_destroy_component(root);
        return;
    }

    IRListData* data = (IRListData*)list->custom_data;
    if (!data || data->type != IR_LIST_UNORDERED) {
        FAIL("List should be unordered");
        ir_destroy_component(root);
        return;
    }

    int item_count = count_children(list);
    if (item_count != 3) {
        FAIL("Expected 3 list items");
        ir_destroy_component(root);
        return;
    }

    ir_destroy_component(root);
    PASS();
}

// Test 4: Parse code block
void test_code_block(void) {
    TEST("code block");

    const char* md = "```python\nprint('hello')\n```\n";
    IRComponent* root = ir_markdown_parse(md, 0);

    if (!root) {
        FAIL("Failed to parse");
        return;
    }

    IRComponent* code = find_child_by_type(root, IR_COMPONENT_CODE_BLOCK);
    if (!code) {
        FAIL("Expected CodeBlock component");
        ir_destroy_component(root);
        return;
    }

    IRCodeBlockData* data = (IRCodeBlockData*)code->custom_data;
    if (!data || !data->language || strcmp(data->language, "python") != 0) {
        FAIL("Language should be python");
        ir_destroy_component(root);
        return;
    }

    if (!data->code || strstr(data->code, "print") == NULL) {
        FAIL("Code content missing");
        ir_destroy_component(root);
        return;
    }

    ir_destroy_component(root);
    PASS();
}

// Test 5: Parse Mermaid flowchart
void test_mermaid_flowchart(void) {
    TEST("mermaid flowchart");

    const char* md = "```mermaid\nflowchart LR\n    A[Start] --> B[End]\n```\n";
    IRComponent* root = ir_markdown_parse(md, 0);

    if (!root) {
        FAIL("Failed to parse");
        return;
    }

    // Should have converted to Flowchart component
    IRComponent* flowchart = find_child_by_type(root, IR_COMPONENT_FLOWCHART);
    if (!flowchart) {
        FAIL("Expected Flowchart component (Mermaid not converted)");
        ir_destroy_component(root);
        return;
    }

    ir_destroy_component(root);
    PASS();
}

// Test 6: Parse table
void test_table(void) {
    TEST("table");

    const char* md = "| Name | Age |\n|------|-----|\n| Alice | 30 |\n| Bob | 25 |\n";
    IRComponent* root = ir_markdown_parse(md, 0);

    if (!root) {
        FAIL("Failed to parse");
        return;
    }

    IRComponent* table = find_child_by_type(root, IR_COMPONENT_TABLE);
    if (!table) {
        FAIL("Expected Table component");
        ir_destroy_component(root);
        return;
    }

    int row_count = count_children(table);
    if (row_count < 2) {  // Header + at least one data row
        FAIL("Expected at least 2 rows");
        ir_destroy_component(root);
        return;
    }

    ir_destroy_component(root);
    PASS();
}

// Test 7: Parse blockquote
void test_blockquote(void) {
    TEST("blockquote");

    const char* md = "> This is a quote\n> spanning two lines\n";
    IRComponent* root = ir_markdown_parse(md, 0);

    if (!root) {
        FAIL("Failed to parse");
        return;
    }

    IRComponent* quote = find_child_by_type(root, IR_COMPONENT_BLOCKQUOTE);
    if (!quote) {
        FAIL("Expected Blockquote component");
        ir_destroy_component(root);
        return;
    }

    ir_destroy_component(root);
    PASS();
}

// Test 8: Parse horizontal rule
void test_horizontal_rule(void) {
    TEST("horizontal rule");

    const char* md = "Before\n\n---\n\nAfter\n";
    IRComponent* root = ir_markdown_parse(md, 0);

    if (!root) {
        FAIL("Failed to parse");
        return;
    }

    IRComponent* hr = find_child_by_type(root, IR_COMPONENT_HORIZONTAL_RULE);
    if (!hr) {
        FAIL("Expected HorizontalRule component");
        ir_destroy_component(root);
        return;
    }

    ir_destroy_component(root);
    PASS();
}

// Test 9: Parse empty input
void test_empty_input(void) {
    TEST("empty input");

    const char* md = "";
    IRComponent* root = ir_markdown_parse(md, 0);

    if (!root) {
        FAIL("Should return empty container");
        return;
    }

    // Should be empty Column
    if (root->type != IR_COMPONENT_COLUMN) {
        FAIL("Root should be Column");
        ir_destroy_component(root);
        return;
    }

    if (root->child_count != 0) {
        FAIL("Should have no children");
        ir_destroy_component(root);
        return;
    }

    ir_destroy_component(root);
    PASS();
}

// Test 10: Parse complex document
void test_complex_document(void) {
    TEST("complex document");

    const char* md =
        "# Title\n\n"
        "Paragraph with **bold** and *italic*.\n\n"
        "## Subtitle\n\n"
        "- List item 1\n"
        "- List item 2\n\n"
        "```python\n"
        "print('code')\n"
        "```\n\n"
        "| Col1 | Col2 |\n"
        "|------|------|\n"
        "| A    | B    |\n";

    IRComponent* root = ir_markdown_parse(md, 0);

    if (!root) {
        FAIL("Failed to parse");
        return;
    }

    // Should have multiple children (heading, paragraph, list, code, table)
    int child_count = count_children(root);
    if (child_count < 4) {
        char msg[100];
        snprintf(msg, sizeof(msg), "Expected at least 4 children, got %d", child_count);
        FAIL(msg);
        ir_destroy_component(root);
        return;
    }

    // Verify we have at least heading, list, code block, and table
    bool has_heading = find_child_by_type(root, IR_COMPONENT_HEADING) != NULL;
    bool has_list = find_child_by_type(root, IR_COMPONENT_LIST) != NULL;
    bool has_code = find_child_by_type(root, IR_COMPONENT_CODE_BLOCK) != NULL;
    bool has_table = find_child_by_type(root, IR_COMPONENT_TABLE) != NULL;

    if (!has_heading || !has_list || !has_code || !has_table) {
        FAIL("Missing expected components");
        ir_destroy_component(root);
        return;
    }

    ir_destroy_component(root);
    PASS();
}

int main(void) {
    printf("\n=== Kryon Markdown Parser Tests ===\n\n");

    // Run all tests
    test_simple_heading();
    test_paragraph();
    test_list();
    test_code_block();
    test_mermaid_flowchart();
    test_table();
    test_blockquote();
    test_horizontal_rule();
    test_empty_input();
    test_complex_document();

    // Print summary
    printf("\n=== Test Summary ===\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Total:  %d\n\n", tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
