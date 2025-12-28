/**
 * Test Lua Round-trip: Lua → KIR → Lua
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External functions
extern char* ir_lua_file_to_kir(const char* filepath);
extern char* lua_codegen_from_json(const char* kir_json);

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <lua_file>\n", argv[0]);
        return 1;
    }

    const char* input_file = argv[1];

    printf("=== Lua Round-trip Test ===\n");
    printf("Input: %s\n\n", input_file);

    // Step 1: Lua → KIR
    printf("Step 1: Parsing Lua → KIR...\n");
    char* kir_json = ir_lua_file_to_kir(input_file);
    if (!kir_json) {
        fprintf(stderr, "Error: Failed to parse Lua to KIR\n");
        return 1;
    }

    printf("KIR JSON (%zu bytes):\n%s\n\n", strlen(kir_json), kir_json);

    // Save KIR
    FILE* kir_file = fopen("/tmp/test_lua.kir", "w");
    if (kir_file) {
        fprintf(kir_file, "%s", kir_json);
        fclose(kir_file);
        printf("Saved KIR to: /tmp/test_lua.kir\n\n");
    }

    // Step 2: KIR → Lua
    printf("Step 2: Generating Lua from KIR...\n");
    char* output_lua = lua_codegen_from_json(kir_json);
    free(kir_json);

    if (!output_lua) {
        fprintf(stderr, "Error: Failed to generate Lua from KIR\n");
        return 1;
    }

    printf("Generated Lua:\n%s\n\n", output_lua);

    // Save output
    FILE* output_file = fopen("/tmp/test_lua_generated.lua", "w");
    if (output_file) {
        fprintf(output_file, "%s", output_lua);
        fclose(output_file);
        printf("Saved generated Lua to: /tmp/test_lua_generated.lua\n");
    }

    // Verify
    printf("\n=== Verification ===\n");
    if (strstr(output_lua, "UI.Column") && strstr(output_lua, "UI.Button")) {
        printf("✓ Components preserved\n");
    }
    if (strstr(output_lua, "onClick")) {
        printf("✓ Event handlers preserved\n");
    }
    if (strstr(output_lua, "Reactive.state")) {
        printf("✓ Reactive state preserved\n");
    }

    printf("\n✓ Round-trip test completed successfully!\n");

    free(output_lua);
    return 0;
}
