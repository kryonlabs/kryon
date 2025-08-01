/**
 * @file optimizer.c
 * @brief AST Optimization Passes
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 */

#include "internal/codegen.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

// =============================================================================
// OPTIMIZATION PASS IMPLEMENTATIONS
// =============================================================================

bool kryon_codegen_optimize_ast(KryonCodeGenerator *codegen, KryonASTNode *ast_root) {
    if (!codegen || !ast_root) {
        return false;
    }
    
    clock_t start_time = clock();
    
    // Run optimization passes based on configuration
    if (codegen->config.deduplicate_strings) {
        codegen->stats.strings_deduplicated = 
            kryon_codegen_deduplicate_strings(codegen, ast_root);
    }
    
    if (codegen->config.inline_styles) {
        codegen->stats.styles_inlined = 
            kryon_codegen_inline_styles(codegen, ast_root);
    }
    
    if (codegen->config.remove_unused_styles) {
        codegen->stats.styles_removed = 
            kryon_codegen_remove_unused_styles(codegen, ast_root);
    }
    
    // Always try constant folding for better performance
    codegen->stats.properties_folded = 
        kryon_codegen_fold_constants(codegen, ast_root);
    
    clock_t end_time = clock();
    codegen->stats.optimization_time = 
        ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    return true;
}

size_t kryon_codegen_inline_styles(KryonCodeGenerator *codegen, KryonASTNode *ast_root) {
    if (!codegen || !ast_root) {
        return 0;
    }
    
    // TODO: Implement style inlining
    // This would find style references and inline the style properties
    // directly into the elements that use them
    
    return 0; // Placeholder
}

size_t kryon_codegen_remove_unused_styles(KryonCodeGenerator *codegen, KryonASTNode *ast_root) {
    if (!codegen || !ast_root) {
        return 0;
    }
    
    // TODO: Implement unused style removal
    // This would traverse the AST to find which styles are actually referenced
    // and remove style blocks that are never used
    
    return 0; // Placeholder
}

size_t kryon_codegen_deduplicate_strings(KryonCodeGenerator *codegen, KryonASTNode *ast_root) {
    if (!codegen || !ast_root) {
        return 0;
    }
    
    // TODO: Implement string deduplication
    // This would find duplicate string literals and replace them with
    // references to a single copy in the string table
    
    return 0; // Placeholder
}

size_t kryon_codegen_fold_constants(KryonCodeGenerator *codegen, KryonASTNode *ast_root) {
    if (!codegen || !ast_root) {
        return 0;
    }
    
    // TODO: Implement constant folding
    // This would evaluate constant expressions at compile time
    // and replace them with their computed values
    
    return 0; // Placeholder
}

// =============================================================================
// CUSTOM ELEMENT/PROPERTY REGISTRATION
// =============================================================================

// Static arrays for custom mappings (could be made dynamic)
static struct {
    char *name;
    uint16_t hex;
} custom_elements[256];
static size_t custom_element_count = 0;

static struct {
    char *name;
    uint16_t hex;
} custom_properties[256];
static size_t custom_property_count = 0;

bool kryon_codegen_register_element(const char *element_name, uint16_t hex_code) {
    if (!element_name || custom_element_count >= 256) {
        return false;
    }
    
    // Check if hex code is already used
    for (size_t i = 0; i < custom_element_count; i++) {
        if (custom_elements[i].hex == hex_code) {
            return false;
        }
    }
    
    // Add new element
    custom_elements[custom_element_count].name = kryon_alloc(strlen(element_name) + 1);
    if (!custom_elements[custom_element_count].name) {
        return false;
    }
    
    strcpy(custom_elements[custom_element_count].name, element_name);
    custom_elements[custom_element_count].hex = hex_code;
    custom_element_count++;
    
    return true;
}

bool kryon_codegen_register_property(const char *property_name, uint16_t hex_code) {
    if (!property_name || custom_property_count >= 256) {
        return false;
    }
    
    // Check if hex code is already used
    for (size_t i = 0; i < custom_property_count; i++) {
        if (custom_properties[i].hex == hex_code) {
            return false;
        }
    }
    
    // Add new property
    custom_properties[custom_property_count].name = kryon_alloc(strlen(property_name) + 1);
    if (!custom_properties[custom_property_count].name) {
        return false;
    }
    
    strcpy(custom_properties[custom_property_count].name, property_name);
    custom_properties[custom_property_count].hex = hex_code;
    custom_property_count++;
    
    return true;
}

// =============================================================================
// BINARY VALIDATION AND INTROSPECTION
// =============================================================================

bool kryon_codegen_check_version(const uint8_t *binary_data, size_t size,
                                uint32_t min_version, uint32_t max_version) {
    if (!binary_data || size < 8) {
        return false;
    }
    
    // Check magic number
    uint32_t magic = *(uint32_t*)binary_data;
    if (magic != 0x4B52594E) { // "KRYN"
        return false;
    }
    
    // Check version
    uint32_t version = *(uint32_t*)(binary_data + 4);
    return version >= min_version && version <= max_version;
}

void kryon_codegen_print_binary(const uint8_t *binary_data, size_t size, FILE *file) {
    if (!binary_data || size == 0) {
        return;
    }
    
    if (!file) {
        file = stdout;
    }
    
    fprintf(file, "KRB Binary Analysis\n");
    fprintf(file, "==================\n");
    fprintf(file, "Size: %zu bytes\n", size);
    
    if (size >= 4) {
        uint32_t magic = *(uint32_t*)binary_data;
        fprintf(file, "Magic: 0x%08X", magic);
        if (magic == 0x4B52594E) {
            fprintf(file, " (valid KRB)\n");
        } else {
            fprintf(file, " (invalid)\n");
            return;
        }
    }
    
    if (size >= 8) {
        uint32_t version = *(uint32_t*)(binary_data + 4);
        fprintf(file, "Version: %u.%u.%u\n", 
                (version >> 16) & 0xFFFF,
                (version >> 8) & 0xFF,
                version & 0xFF);
    }
    
    if (size >= 12) {
        uint32_t flags = *(uint32_t*)(binary_data + 8);
        fprintf(file, "Flags: 0x%08X\n", flags);
        if (flags & 0x01) fprintf(file, "  - Compressed\n");
        if (flags & 0x02) fprintf(file, "  - Debug Info\n");
    }
    
    fprintf(file, "\nBinary Dump (first 256 bytes):\n");
    size_t dump_size = size < 256 ? size : 256;
    for (size_t i = 0; i < dump_size; i += 16) {
        fprintf(file, "%04zx: ", i);
        
        // Hex bytes
        for (size_t j = i; j < i + 16 && j < dump_size; j++) {
            fprintf(file, "%02x ", binary_data[j]);
        }
        
        // Padding
        for (size_t j = dump_size - i; j < 16; j++) {
            fprintf(file, "   ");
        }
        
        fprintf(file, " ");
        
        // ASCII representation
        for (size_t j = i; j < i + 16 && j < dump_size; j++) {
            char c = binary_data[j];
            fprintf(file, "%c", (c >= 32 && c <= 126) ? c : '.');
        }
        
        fprintf(file, "\n");
    }
}

bool kryon_codegen_disassemble(const uint8_t *binary_data, size_t size, const char *output_file) {
    if (!binary_data || !output_file || size == 0) {
        return false;
    }
    
    FILE *file = fopen(output_file, "w");
    if (!file) {
        return false;
    }
    
    fprintf(file, "; KRB Disassembly\n");
    fprintf(file, "; Generated by Kryon-C Disassembler\n\n");
    
    // Print binary analysis
    kryon_codegen_print_binary(binary_data, size, file);
    
    fclose(file);
    return true;
}

int kryon_codegen_compare_binaries(const uint8_t *file1_data, size_t file1_size,
                                  const uint8_t *file2_data, size_t file2_size,
                                  const char *diff_file) {
    if (!file1_data || !file2_data) {
        return -1;
    }
    
    // Quick size check
    if (file1_size != file2_size) {
        if (diff_file) {
            FILE *diff = fopen(diff_file, "w");
            if (diff) {
                fprintf(diff, "Binary size mismatch: %zu vs %zu bytes\n", 
                       file1_size, file2_size);
                fclose(diff);
            }
        }
        return 1; // Different
    }
    
    // Byte-by-byte comparison
    for (size_t i = 0; i < file1_size; i++) {
        if (file1_data[i] != file2_data[i]) {
            if (diff_file) {
                FILE *diff = fopen(diff_file, "w");
                if (diff) {
                    fprintf(diff, "Difference at offset 0x%zx: 0x%02x vs 0x%02x\n",
                           i, file1_data[i], file2_data[i]);
                    fclose(diff);
                }
            }
            return 1; // Different
        }
    }
    
    return 0; // Identical
}