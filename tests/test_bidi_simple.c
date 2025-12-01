/**
 * Simple BiDi test to debug segfault
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir/ir_text_shaping.h"

int main() {
    printf("Testing BiDi UTF-8 to UTF-32 conversion...\n");

    const char* text = "Hello";
    uint32_t len = 0;

    uint32_t* utf32 = ir_bidi_utf8_to_utf32(text, strlen(text), &len);

    if (utf32) {
        printf("Converted %zu bytes to %u UTF-32 characters\n", strlen(text), len);
        for (uint32_t i = 0; i < len; i++) {
            printf("  [%u] = U+%04X\n", i, utf32[i]);
        }
        free(utf32);
        printf("✓ Test passed\n");
        return 0;
    } else {
        printf("✗ Conversion failed\n");
        return 1;
    }
}
