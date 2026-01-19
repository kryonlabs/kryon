#include "kotlin_codegen.h"
#include <stdio.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.kir> <output.kt>\n", argv[0]);
        return 1;
    }

    const char* kir_path = argv[1];
    const char* output_path = argv[2];

    printf("Generating Kotlin code from %s to %s...\n", kir_path, output_path);

    if (ir_generate_kotlin_code(kir_path, output_path)) {
        printf("Success! Generated Kotlin code at: %s\n", output_path);
        return 0;
    } else {
        fprintf(stderr, "Failed to generate Kotlin code\n");
        return 1;
    }
}
