/**
 * KIR to HTML Converter
 * Simple CLI tool to convert KIR files to HTML
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../ir/ir_core.h"
#include "../../ir/ir_serialization.h"
#include "ir_web_renderer.h"
#include "html_generator.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.kir> <output_dir> [--embedded-css]\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  --embedded-css  Embed CSS in <style> tag instead of external file\n");
        return 1;
    }

    const char* kir_file = argv[1];
    const char* output_dir = argv[2];
    bool embedded_css = false;

    // Parse optional flags
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--embedded-css") == 0) {
            embedded_css = true;
        }
    }

    printf("Converting KIR to HTML...\n");
    printf("  Input: %s\n", kir_file);
    printf("  Output: %s/\n", output_dir);
    if (embedded_css) {
        printf("  Mode: Embedded CSS\n");
    }

    // Load KIR file with manifest (for CSS variable support)
    IRReactiveManifest* manifest = NULL;
    IRComponent* root = ir_read_json_file_with_manifest(kir_file, &manifest);
    if (!root) {
        fprintf(stderr, "Error: Failed to load KIR file: %s\n", kir_file);
        return 1;
    }

    if (manifest) {
        printf("  CSS Variables: %u found\n", manifest->variable_count);
    }

    // Generate HTML with manifest (for CSS variable output)
    bool success = web_render_ir_component_with_manifest(root, output_dir, embedded_css, manifest);

    // Cleanup
    ir_destroy_component(root);
    if (manifest) {
        ir_reactive_manifest_destroy(manifest);
    }

    if (success) {
        printf("✓ HTML files generated successfully in %s/\n", output_dir);
        return 0;
    } else {
        fprintf(stderr, "✗ HTML generation failed\n");
        return 1;
    }
}
