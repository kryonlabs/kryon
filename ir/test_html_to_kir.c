#include "parsers/html/html_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <html_file>\n", argv[0]);
        return 1;
    }

    const char* html_file = argv[1];

    printf("Converting HTML to KIR: %s\n", html_file);

    // Convert HTML file to KIR JSON
    char* kir = ir_html_file_to_kir(html_file);

    if (!kir) {
        fprintf(stderr, "❌ Failed to convert HTML to KIR\n");
        return 1;
    }

    // Output KIR JSON
    printf("\n%s\n", kir);

    // Clean up
    free(kir);

    printf("\n✅ Successfully converted HTML to KIR\n");
    return 0;
}
