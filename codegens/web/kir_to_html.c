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

    // Check for --inline-css flag
    bool inline_css = false;
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--inline-css") == 0) {
            inline_css = true;
        }
    }

    // Load KIR file
    IRComponent* root = ir_read_json_file(kir_file);
    if (!root) {
        fprintf(stderr, "Error: Failed to load KIR file: %s\n", kir_file);
        return 1;
    }

    // Create renderer with options
    WebIRRenderer* renderer = web_ir_renderer_create();
    if (!renderer) {
        fprintf(stderr, "Error: Failed to create web renderer\n");
        ir_destroy_component(root);
        return 1;
    }

    web_ir_renderer_set_output_directory(renderer, output_dir);
    web_ir_renderer_set_inline_css(renderer, inline_css);

    // Generate HTML
    bool success = web_ir_renderer_render(renderer, root);

    // Cleanup renderer
    web_ir_renderer_destroy(renderer);

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
