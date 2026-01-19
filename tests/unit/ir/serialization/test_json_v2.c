#include "ir_serialization.h"
#include "ir_builder.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.kirb> [output.kir]\n", argv[0]);
        return 1;
    }

    const char* input = argv[1];
    const char* output = argc > 2 ? argv[2] : NULL;

    // Load IR from binary file
    printf("Loading IR from: %s\n", input);
    IRReactiveManifest* manifest = NULL;
    IRComponent* root = ir_read_binary_file_with_manifest(input, &manifest);
    if (!root) {
        fprintf(stderr, "Error: Failed to load IR from %s\n", input);
        return 1;
    }

    if (manifest) {
        printf("Loaded with reactive manifest (%u vars)\n", manifest->variable_count);
    }

    printf("IR loaded successfully\n");

    // Serialize to JSON v2
    printf("Serializing to JSON v2...\n");
    char* json = ir_serialize_json(root);
    if (!json) {
        fprintf(stderr, "Error: Failed to serialize to JSON\n");
        ir_destroy_component(root);
        return 1;
    }

    // Output JSON
    if (output) {
        printf("Writing JSON to: %s\n", output);
        if (!ir_write_json_file(root, output)) {
            fprintf(stderr, "Error: Failed to write JSON to %s\n", output);
            free(json);
            ir_destroy_component(root);
            return 1;
        }
        printf("Success!\n");
    } else {
        // Print to stdout
        printf("\nJSON Output:\n");
        printf("%s\n", json);
    }

    free(json);
    if (manifest) {
        ir_reactive_manifest_destroy(manifest);
    }
    ir_destroy_component(root);
    return 0;
}
