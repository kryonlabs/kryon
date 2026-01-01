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
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <source_dir> <input.kir> <output_dir> [--embedded-css]\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  source_dir      Directory containing source files (for asset copying)\n");
        fprintf(stderr, "  --embedded-css  Embed CSS in <style> tag instead of external file\n");
        return 1;
    }

    const char* source_dir = argv[1];
    const char* kir_file = argv[2];
    const char* output_dir = argv[3];
    bool embedded_css = false;

    // Parse optional flags
    for (int i = 4; i < argc; i++) {
        if (strcmp(argv[i], "--embedded-css") == 0) {
            embedded_css = true;
        }
    }

    printf("Converting KIR to HTML...\n");
    printf("  Source: %s\n", source_dir);
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

    // Create renderer manually to set source directory
    WebIRRenderer* renderer = web_ir_renderer_create();
    if (!renderer) {
        fprintf(stderr, "Error: Failed to create web renderer\n");
        ir_destroy_component(root);
        if (manifest) ir_reactive_manifest_destroy(manifest);
        return 1;
    }

    // Configure renderer
    web_ir_renderer_set_output_directory(renderer, output_dir);
    web_ir_renderer_set_source_directory(renderer, source_dir);
    web_ir_renderer_set_inline_css(renderer, embedded_css);
    if (manifest) {
        web_ir_renderer_set_manifest(renderer, manifest);
    }

    // Generate HTML
    bool success = web_ir_renderer_render(renderer, root);

    // Cleanup
    web_ir_renderer_destroy(renderer);

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
