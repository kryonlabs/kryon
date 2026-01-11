#include "parsers/html/html_parser.h"
#include "parsers/html/html_to_ir.h"
#include "ir_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test basic HTML parsing
void test_basic_parsing() {
    const char* html = "<h1 data-ir-type=\"HEADING\" data-level=\"1\">Hello World</h1>";

    HtmlNode* ast = ir_html_parse(html, strlen(html));
    assert(ast != NULL);
    assert(ast->type == HTML_NODE_ELEMENT);
    assert(strcmp(ast->tag_name, "h1") == 0);
    assert(ast->data_level == 1);
    assert(strcmp(ast->data_ir_type, "HEADING") == 0);

    printf("✅ Basic parsing test passed\n");

    html_node_free(ast);
}

// Test nested HTML parsing
void test_nested_parsing() {
    const char* html =
        "<div class=\"kryon-container\">"
        "  <h1>Title</h1>"
        "  <p>This is a paragraph.</p>"
        "  <ul>"
        "    <li>Item 1</li>"
        "    <li>Item 2</li>"
        "  </ul>"
        "</div>";

    HtmlNode* ast = ir_html_parse(html, strlen(html));
    assert(ast != NULL);
    assert(strcmp(ast->tag_name, "div") == 0);
    assert(ast->child_count == 3);  // h1, p, ul

    // Check first child (h1)
    assert(strcmp(ast->children[0]->tag_name, "h1") == 0);
    assert(ast->children[0]->child_count == 1);
    assert(ast->children[0]->children[0]->type == HTML_NODE_TEXT);

    // Check third child (ul)
    assert(strcmp(ast->children[2]->tag_name, "ul") == 0);
    assert(ast->children[2]->child_count == 2);  // 2 list items

    printf("✅ Nested parsing test passed\n");

    html_node_free(ast);
}

// Test attribute parsing
void test_attribute_parsing() {
    const char* html = "<div id=\"main\" class=\"container\" style=\"width: 800px; height: 600px;\"></div>";

    HtmlNode* ast = ir_html_parse(html, strlen(html));
    assert(ast != NULL);
    assert(strcmp(ast->id, "main") == 0);
    assert(strcmp(ast->class_name, "container") == 0);
    assert(strcmp(ast->style, "width: 800px; height: 600px;") == 0);

    printf("✅ Attribute parsing test passed\n");

    html_node_free(ast);
}

// Test HTML to IR conversion
void test_html_to_ir() {
    const char* html =
        "<div>"
        "  <h1>Welcome</h1>"
        "  <p>This is a test.</p>"
        "</div>";

    HtmlNode* ast = ir_html_parse(html, strlen(html));
    assert(ast != NULL);

    IRComponent* root = ir_html_to_ir_convert(ast);
    assert(root != NULL);
    assert(root->type == IR_COMPONENT_CONTAINER);
    assert(root->child_count == 2);

    // Check first child (heading)
    assert(root->children[0]->type == IR_COMPONENT_HEADING);

    // Check second child (paragraph)
    assert(root->children[1]->type == IR_COMPONENT_PARAGRAPH);

    printf("✅ HTML to IR conversion test passed\n");

    html_node_free(ast);
    // Note: IR components need proper cleanup via ir_component_free()
}

// Test CSS parsing
void test_css_parsing() {
    const char* html = "<div style=\"width: 800px; height: 600px; background-color: #f0f0f0;\"></div>";

    HtmlNode* ast = ir_html_parse(html, strlen(html));
    assert(ast != NULL);

    IRComponent* component = ir_html_to_ir_convert(ast);
    assert(component != NULL);

    // Check dimensions
    assert(component->style->width.value == 800.0f);
    assert(component->style->width.type == IR_DIMENSION_PX);
    assert(component->style->height.value == 600.0f);
    assert(component->style->height.type == IR_DIMENSION_PX);

    // Check background color (approximately #f0f0f0 = rgb(240, 240, 240))
    assert(component->style->background.data.r == 240);
    assert(component->style->background.data.g == 240);
    assert(component->style->background.data.b == 240);

    printf("✅ CSS parsing test passed\n");

    html_node_free(ast);
}

// Test self-closing tags
void test_self_closing() {
    const char* html = "<img src=\"image.png\" alt=\"Test Image\" />";

    HtmlNode* ast = ir_html_parse(html, strlen(html));
    assert(ast != NULL);
    assert(strcmp(ast->tag_name, "img") == 0);
    assert(strcmp(ast->src, "image.png") == 0);
    assert(strcmp(ast->alt, "Test Image") == 0);

    printf("✅ Self-closing tag test passed\n");

    html_node_free(ast);
}

int main() {
    printf("Running HTML parser tests...\n\n");

    test_basic_parsing();
    test_nested_parsing();
    test_attribute_parsing();
    test_html_to_ir();
    test_css_parsing();
    test_self_closing();

    printf("\n✅ All tests passed!\n");

    return 0;
}
