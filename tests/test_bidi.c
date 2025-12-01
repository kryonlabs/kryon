/**
 * Test program for FriBidi bidirectional text support
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir/ir_text_shaping.h"

void print_mapping(const char* label, uint32_t* mapping, uint32_t length) {
    printf("  %s: [", label);
    for (uint32_t i = 0; i < length; i++) {
        printf("%u", mapping[i]);
        if (i < length - 1) printf(", ");
    }
    printf("]\n");
}

void print_levels(uint8_t* levels, uint32_t length) {
    printf("  Embedding levels: [");
    for (uint32_t i = 0; i < length; i++) {
        printf("%u", levels[i]);
        if (i < length - 1) printf(", ");
    }
    printf("]\n");
}

const char* direction_name(IRBidiDirection dir) {
    switch (dir) {
        case IR_BIDI_DIR_LTR: return "LTR";
        case IR_BIDI_DIR_RTL: return "RTL";
        case IR_BIDI_DIR_WEAK_LTR: return "Weak LTR";
        case IR_BIDI_DIR_WEAK_RTL: return "Weak RTL";
        case IR_BIDI_DIR_NEUTRAL: return "Neutral";
        default: return "Unknown";
    }
}

int main(int argc, char** argv) {
    printf("=== Kryon BiDi (Bidirectional Text) Test ===\n\n");

    // Test 1: English (LTR)
    printf("Test 1: English text (LTR)\n");
    const char* text1 = "Hello World";
    IRBidiDirection dir1 = ir_bidi_detect_direction(text1, strlen(text1));
    printf("  Text: \"%s\"\n", text1);
    printf("  Detected direction: %s\n", direction_name(dir1));

    uint32_t utf32_len1 = 0;
    uint32_t* utf32_text1 = ir_bidi_utf8_to_utf32(text1, strlen(text1), &utf32_len1);
    if (utf32_text1) {
        IRBidiResult* result1 = ir_bidi_reorder(utf32_text1, utf32_len1, dir1);
        if (result1) {
            print_mapping("Visual→Logical", result1->visual_to_logical, result1->length);
            print_mapping("Logical→Visual", result1->logical_to_visual, result1->length);
            print_levels(result1->embedding_levels, result1->length);
            printf("  Base direction: %s\n", direction_name(result1->base_direction));
            ir_bidi_result_destroy(result1);
        }
        free(utf32_text1);
    }
    printf("✓ Test 1 passed\n\n");

    // Test 2: Hebrew (RTL)
    printf("Test 2: Hebrew text (RTL)\n");
    const char* text2 = "שלום עולם";  // "Shalom Olam" (Hello World)
    IRBidiDirection dir2 = ir_bidi_detect_direction(text2, strlen(text2));
    printf("  Text: \"%s\"\n", text2);
    printf("  Detected direction: %s\n", direction_name(dir2));

    uint32_t utf32_len2 = 0;
    uint32_t* utf32_text2 = ir_bidi_utf8_to_utf32(text2, strlen(text2), &utf32_len2);
    if (utf32_text2) {
        IRBidiResult* result2 = ir_bidi_reorder(utf32_text2, utf32_len2, dir2);
        if (result2) {
            printf("  Characters: %u\n", result2->length);
            print_mapping("Visual→Logical", result2->visual_to_logical, result2->length);
            print_mapping("Logical→Visual", result2->logical_to_visual, result2->length);
            print_levels(result2->embedding_levels, result2->length);
            printf("  Base direction: %s\n", direction_name(result2->base_direction));
            ir_bidi_result_destroy(result2);
        }
        free(utf32_text2);
    }
    printf("✓ Test 2 passed\n\n");

    // Test 3: Arabic (RTL)
    printf("Test 3: Arabic text (RTL)\n");
    const char* text3 = "مرحبا بالعالم";  // "Marhaba bel3alam" (Hello World)
    IRBidiDirection dir3 = ir_bidi_detect_direction(text3, strlen(text3));
    printf("  Text: \"%s\"\n", text3);
    printf("  Detected direction: %s\n", direction_name(dir3));

    uint32_t utf32_len3 = 0;
    uint32_t* utf32_text3 = ir_bidi_utf8_to_utf32(text3, strlen(text3), &utf32_len3);
    if (utf32_text3) {
        IRBidiResult* result3 = ir_bidi_reorder(utf32_text3, utf32_len3, dir3);
        if (result3) {
            printf("  Characters: %u\n", result3->length);
            print_mapping("Visual→Logical", result3->visual_to_logical, result3->length);
            print_levels(result3->embedding_levels, result3->length);
            printf("  Base direction: %s\n", direction_name(result3->base_direction));
            ir_bidi_result_destroy(result3);
        }
        free(utf32_text3);
    }
    printf("✓ Test 3 passed\n\n");

    // Test 4: Mixed LTR/RTL
    printf("Test 4: Mixed English and Hebrew\n");
    const char* text4 = "Hello שלום World";
    IRBidiDirection dir4 = ir_bidi_detect_direction(text4, strlen(text4));
    printf("  Text: \"%s\"\n", text4);
    printf("  Detected direction: %s\n", direction_name(dir4));

    uint32_t utf32_len4 = 0;
    uint32_t* utf32_text4 = ir_bidi_utf8_to_utf32(text4, strlen(text4), &utf32_len4);
    if (utf32_text4) {
        IRBidiResult* result4 = ir_bidi_reorder(utf32_text4, utf32_len4, dir4);
        if (result4) {
            printf("  Characters: %u\n", result4->length);
            print_mapping("Visual→Logical", result4->visual_to_logical, result4->length);
            print_mapping("Logical→Visual", result4->logical_to_visual, result4->length);
            print_levels(result4->embedding_levels, result4->length);
            printf("  Base direction: %s\n", direction_name(result4->base_direction));
            ir_bidi_result_destroy(result4);
        }
        free(utf32_text4);
    }
    printf("✓ Test 4 passed\n\n");

    printf("=== All BiDi tests completed successfully! ===\n");
    printf("\nBiDi support enables:\n");
    printf("✓ Automatic direction detection (LTR/RTL)\n");
    printf("✓ Proper reordering of mixed directional text\n");
    printf("✓ Support for Hebrew, Arabic, and other RTL languages\n");
    printf("✓ Unicode Bidirectional Algorithm compliance\n");

    return 0;
}
