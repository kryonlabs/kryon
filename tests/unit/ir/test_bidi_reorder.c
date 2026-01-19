/**
 * Test BiDi reordering
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir/ir_text_shaping.h"

int main() {
    printf("Testing BiDi reordering...\n\n");

    const char* text = "Hello";
    uint32_t utf32_len = 0;
    uint32_t* utf32_text = ir_bidi_utf8_to_utf32(text, strlen(text), &utf32_len);

    if (!utf32_text) {
        printf("UTF-8 to UTF-32 conversion failed\n");
        return 1;
    }

    printf("Converted to %u UTF-32 characters\n", utf32_len);

    printf("Calling ir_bidi_reorder...\n");
    IRBidiResult* result = ir_bidi_reorder(utf32_text, utf32_len, IR_BIDI_DIR_LTR);

    if (result) {
        printf("✓ Reordering succeeded\n");
        printf("Length: %u\n", result->length);

        printf("Visual→Logical: [");
        for (uint32_t i = 0; i < result->length; i++) {
            printf("%u", result->visual_to_logical[i]);
            if (i < result->length - 1) printf(", ");
        }
        printf("]\n");

        ir_bidi_result_destroy(result);
    } else {
        printf("✗ Reordering failed\n");
        free(utf32_text);
        return 1;
    }

    free(utf32_text);
    printf("\n✓ Test passed\n");
    return 0;
}
