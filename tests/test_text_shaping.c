/**
 * Test program for HarfBuzz text shaping integration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir/ir_text_shaping.h"

int main(int argc, char** argv) {
    printf("=== Kryon Text Shaping Test ===\n\n");

    // Initialize text shaping system
    if (!ir_text_shaping_init()) {
        fprintf(stderr, "Failed to initialize text shaping\n");
        return 1;
    }

    printf("✓ Text shaping system initialized\n");

    // Load a font
    const char* font_path = "/nix/store/5djka4vhy98x1grxmkghpql3531qjbza-dejavu-fonts-2.37/share/fonts/truetype/DejaVuSans.ttf";
    float font_size = 16.0f;

    IRFont* font = ir_font_load(font_path, font_size);
    if (!font) {
        fprintf(stderr, "Failed to load font: %s\n", font_path);
        ir_text_shaping_shutdown();
        return 1;
    }

    printf("✓ Loaded font: %s at %.1fpt\n\n", font_path, font_size);

    // Test 1: Simple Latin text
    printf("Test 1: Simple Latin text\n");
    const char* text1 = "Hello, World!";
    IRShapeOptions opts1 = {
        .direction = IR_SHAPE_DIRECTION_LTR,
        .language = "en",
        .script = "Latn",
        .features = NULL,
        .feature_count = 0
    };

    IRShapedText* shaped1 = ir_shape_text(font, text1, strlen(text1), &opts1);
    if (shaped1) {
        printf("  Text: \"%s\"\n", text1);
        printf("  Glyphs: %u\n", shaped1->glyph_count);
        printf("  Width: %.2f px\n", ir_shaped_text_get_width(shaped1));
        printf("  Height: %.2f px\n", ir_shaped_text_get_height(shaped1));

        // Show first few glyphs
        printf("  First 5 glyphs:\n");
        for (uint32_t i = 0; i < shaped1->glyph_count && i < 5; i++) {
            IRGlyphInfo* g = &shaped1->glyphs[i];
            printf("    [%u] glyph_id=%u, x_advance=%.2f, cluster=%u\n",
                   i, g->glyph_id, g->x_advance, g->cluster);
        }

        ir_shaped_text_destroy(shaped1);
        printf("✓ Test 1 passed\n\n");
    } else {
        printf("✗ Test 1 failed\n\n");
    }

    // Test 2: Text with ligatures (if font supports them)
    printf("Test 2: Text with ligatures\n");
    const char* text2 = "fficeffifflfi";  // Should form ligatures in fonts that support them

    const char* features[] = {"liga", "kern"};
    IRShapeOptions opts2 = {
        .direction = IR_SHAPE_DIRECTION_LTR,
        .language = "en",
        .script = "Latn",
        .features = features,
        .feature_count = 2
    };

    IRShapedText* shaped2 = ir_shape_text(font, text2, strlen(text2), &opts2);
    if (shaped2) {
        printf("  Text: \"%s\"\n", text2);
        printf("  Glyphs: %u (fewer glyphs = ligatures formed)\n", shaped2->glyph_count);
        printf("  Width: %.2f px\n", ir_shaped_text_get_width(shaped2));

        ir_shaped_text_destroy(shaped2);
        printf("✓ Test 2 passed\n\n");
    } else {
        printf("✗ Test 2 failed\n\n");
    }

    // Test 3: Multi-line text
    printf("Test 3: Multi-line text\n");
    const char* text3 = "This is line one\nThis is line two\nThis is line three";

    IRShapedText* shaped3 = ir_shape_text(font, text3, strlen(text3), &opts1);
    if (shaped3) {
        printf("  Text has %zu characters\n", strlen(text3));
        printf("  Glyphs: %u\n", shaped3->glyph_count);
        printf("  Width: %.2f px\n", ir_shaped_text_get_width(shaped3));

        ir_shaped_text_destroy(shaped3);
        printf("✓ Test 3 passed\n\n");
    } else {
        printf("✗ Test 3 failed\n\n");
    }

    // Cleanup
    ir_font_destroy(font);
    ir_text_shaping_shutdown();

    printf("=== All tests completed ===\n");
    return 0;
}
