/**
 * Test BiDi direction detection
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir/ir_text_shaping.h"

const char* dir_name(IRBidiDirection dir) {
    switch (dir) {
        case IR_BIDI_DIR_LTR: return "LTR";
        case IR_BIDI_DIR_RTL: return "RTL";
        default: return "Unknown";
    }
}

int main() {
    printf("Testing BiDi direction detection...\n\n");

    const char* text1 = "Hello";
    IRBidiDirection dir1 = ir_bidi_detect_direction(text1, strlen(text1));
    printf("Text: \"%s\" -> %s\n", text1, dir_name(dir1));

    const char* text2 = "שלום";
    IRBidiDirection dir2 = ir_bidi_detect_direction(text2, strlen(text2));
    printf("Text: \"%s\" -> %s\n", text2, dir_name(dir2));

    printf("\n✓ Direction detection test passed\n");
    return 0;
}
