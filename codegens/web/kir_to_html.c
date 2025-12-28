/**
 * KIR to HTML Converter
 * Simple CLI tool to convert KIR files to HTML
 */

#include <stdio.h>
#include <stdlib.h>
#include "../../ir/ir_core.h"
#include "../../ir/ir_serialization.h"
#include "ir_web_renderer.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.kir> <output_dir>\n", argv[0]);
        return 1;
    }

    const char* kir_file = argv[1];
    const char* output_dir = argv[2];

    printf("Converting KIR to HTML...\n");
    printf("  Input: %s\n", kir_file);
    printf("  Output: %s/\n", output_dir);

    // Load KIR file
    IRComponent* root = ir_read_json_file(kir_file);
    if (!root) {
        fprintf(stderr, "Error: Failed to load KIR file: %s\n", kir_file);
        return 1;
    }

    // Generate HTML
    bool success = web_render_ir_component(root, output_dir);

    // Cleanup
    ir_destroy_component(root);

    if (success) {
        printf("✓ HTML files generated successfully in %s/\n", output_dir);
        return 0;
    } else {
        fprintf(stderr, "✗ HTML generation failed\n");
        return 1;
    }
}
