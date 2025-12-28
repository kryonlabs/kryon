#include "parsers/html/css_parser.h"
#include "ir_core.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Test CSS parsing with a real inline style from generated HTML
int main() {
    printf("Testing CSS Parser Roundtrip...\n\n");

    // Example inline style from generated HTML
    const char* style = "width: 1000.00px; height: 600.00px; background-color: rgba(10, 14, 39, 1.00); color: rgba(0, 0, 0, 0.00); text-align: left; ";

    // Create an empty IRStyle
    IRStyle test_style = {0};

    // Initialize with defaults
    test_style.width.type = IR_DIMENSION_AUTO;
    test_style.height.type = IR_DIMENSION_AUTO;
    test_style.background.type = IR_COLOR_TRANSPARENT;
    test_style.font.color.type = IR_COLOR_TRANSPARENT;

    printf("1. Parsing inline CSS:\n   %s\n\n", style);

    // Parse CSS into properties
    uint32_t prop_count = 0;
    CSSProperty* props = ir_css_parse_inline(style, &prop_count);

    if (!props) {
        printf("❌ Failed to parse CSS\n");
        return 1;
    }

    printf("2. Found %u CSS properties:\n", prop_count);
    for (uint32_t i = 0; i < prop_count; i++) {
        printf("   - %s: %s\n", props[i].property, props[i].value);
    }
    printf("\n");

    // Apply properties to IRStyle
    ir_css_apply_to_style(&test_style, props, prop_count);

    printf("3. Applied to IRStyle:\n");
    printf("   Width: %.2fpx (type=%d)\n", test_style.width.value, test_style.width.type);
    printf("   Height: %.2fpx (type=%d)\n", test_style.height.value, test_style.height.type);
    printf("   Background: rgba(%u, %u, %u, %u)\n",
           test_style.background.data.r,
           test_style.background.data.g,
           test_style.background.data.b,
           test_style.background.data.a);
    printf("   Text Color: rgba(%u, %u, %u, %u)\n",
           test_style.font.color.data.r,
           test_style.font.color.data.g,
           test_style.font.color.data.b,
           test_style.font.color.data.a);
    printf("   Text Align: %d\n", test_style.font.align);
    printf("\n");

    // Verify values
    int errors = 0;

    if (test_style.width.value != 1000.0f) {
        printf("❌ Width mismatch: expected 1000.00, got %.2f\n", test_style.width.value);
        errors++;
    }

    if (test_style.height.value != 600.0f) {
        printf("❌ Height mismatch: expected 600.00, got %.2f\n", test_style.height.value);
        errors++;
    }

    if (test_style.background.data.r != 10 ||
        test_style.background.data.g != 14 ||
        test_style.background.data.b != 39) {
        printf("❌ Background color mismatch\n");
        errors++;
    }

    if (test_style.font.color.data.r != 0 ||
        test_style.font.color.data.g != 0 ||
        test_style.font.color.data.b != 0 ||
        test_style.font.color.data.a != 0) {
        printf("❌ Text color mismatch\n");
        errors++;
    }

    // Clean up
    ir_css_free_properties(props, prop_count);

    if (errors == 0) {
        printf("✅ All CSS roundtrip tests passed!\n");
        return 0;
    } else {
        printf("❌ %d test(s) failed\n", errors);
        return 1;
    }
}
