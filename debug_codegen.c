#include "include/internal/codegen.h"
#include <stdio.h>

int main() {
    printf("Testing KryonCodeGenerator creation...\n");
    
    KryonCodeGenerator *codegen = kryon_codegen_create(NULL);
    if (!codegen) {
        printf("ERROR: Failed to create codegen\n");
        return 1;
    }
    printf("✓ Codegen created successfully\n");
    
    // Test basic buffer functionality
    printf("Testing basic binary writing...\n");
    
    // Try to write the KRB header
    if (!kryon_codegen_write_header(codegen)) {
        printf("ERROR: Failed to write header\n");
        
        size_t error_count;
        const char **errors = kryon_codegen_get_errors(codegen, &error_count);
        printf("Error count: %zu\n", error_count);
        for (size_t i = 0; i < error_count; i++) {
            printf("Error %zu: %s\n", i, errors[i]);
        }
        
        kryon_codegen_destroy(codegen);
        return 1;
    }
    
    printf("✓ Header written successfully\n");
    
    size_t binary_size;
    const uint8_t *binary = kryon_codegen_get_binary(codegen, &binary_size);
    printf("Binary size: %zu bytes\n", binary_size);
    
    if (binary_size > 0) {
        printf("Binary data: ");
        for (size_t i = 0; i < binary_size && i < 16; i++) {
            printf("%02X ", binary[i]);
        }
        printf("\n");
    }
    
    kryon_codegen_destroy(codegen);
    printf("✓ Test completed successfully\n");
    return 0;
}