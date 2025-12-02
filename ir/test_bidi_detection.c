#include <stdio.h>
#include <string.h>
#include "ir_core.h"
#include "ir_builder.h"
#include "ir_text_shaping.h"

int main() {
    fprintf(stderr, "=== BiDi Detection Test ===\n");

    // Initialize IR context
    IRContext* ctx = ir_create_context();
    if (!ctx) {
        fprintf(stderr, "Failed to create IR context\n");
        return 1;
    }

    // Create a text component with Arabic text
    IRComponent* arabic_text = ir_create_component(IR_COMPONENT_TEXT);
    if (!arabic_text) {
        fprintf(stderr, "Failed to create text component\n");
        return 1;
    }

    // Set Arabic text content
    const char* arabic = "مرحبا بالعالم";  // "Hello World" in Arabic
    ir_set_text_content(arabic_text, arabic);
    fprintf(stderr, "Created text component with Arabic: '%s'\n", arabic);

    // Check initial direction (should be AUTO = 0)
    if (arabic_text->layout) {
        fprintf(stderr, "Initial base_direction: %d (should be 0=AUTO)\n",
                arabic_text->layout->flex.base_direction);
    }

    // Trigger layout computation (this should detect direction)
    ir_layout_compute(arabic_text, 800, 600);

    // Check final direction (should be RTL = 2)
    if (arabic_text->layout) {
        fprintf(stderr, "After layout base_direction: %d ",
                arabic_text->layout->flex.base_direction);
        switch (arabic_text->layout->flex.base_direction) {
            case 0: fprintf(stderr, "(AUTO)\n"); break;
            case 1: fprintf(stderr, "(LTR)\n"); break;
            case 2: fprintf(stderr, "(RTL)\n"); break;
            case 3: fprintf(stderr, "(INHERIT)\n"); break;
            default: fprintf(stderr, "(UNKNOWN)\n"); break;
        }
    }

    // Test with Hebrew text
    IRComponent* hebrew_text = ir_create_component(IR_COMPONENT_TEXT);
    const char* hebrew = "שלום עולם";  // "Hello World" in Hebrew
    ir_set_text_content(hebrew_text, hebrew);
    fprintf(stderr, "\nCreated text component with Hebrew: '%s'\n", hebrew);

    fprintf(stderr, "Initial base_direction: %d\n",
            hebrew_text->layout ? hebrew_text->layout->flex.base_direction : -1);

    ir_layout_compute(hebrew_text, 800, 600);

    fprintf(stderr, "After layout base_direction: %d ",
            hebrew_text->layout->flex.base_direction);
    switch (hebrew_text->layout->flex.base_direction) {
        case 0: fprintf(stderr, "(AUTO)\n"); break;
        case 1: fprintf(stderr, "(LTR)\n"); break;
        case 2: fprintf(stderr, "(RTL)\n"); break;
        case 3: fprintf(stderr, "(INHERIT)\n"); break;
        default: fprintf(stderr, "(UNKNOWN)\n"); break;
    }

    // Test with English text (should be LTR)
    IRComponent* english_text = ir_create_component(IR_COMPONENT_TEXT);
    const char* english = "Hello World";
    ir_set_text_content(english_text, english);
    fprintf(stderr, "\nCreated text component with English: '%s'\n", english);

    ir_layout_compute(english_text, 800, 600);

    fprintf(stderr, "After layout base_direction: %d ",
            english_text->layout->flex.base_direction);
    switch (english_text->layout->flex.base_direction) {
        case 0: fprintf(stderr, "(AUTO)\n"); break;
        case 1: fprintf(stderr, "(LTR)\n"); break;
        case 2: fprintf(stderr, "(RTL)\n"); break;
        case 3: fprintf(stderr, "(INHERIT)\n"); break;
        default: fprintf(stderr, "(UNKNOWN)\n"); break;
    }

    fprintf(stderr, "\n=== Test Complete ===\n");

    // Cleanup
    ir_destroy_context(ctx);

    return 0;
}
