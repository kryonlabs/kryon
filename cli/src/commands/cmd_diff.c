/**
 * Diff Command
 * Compare two KIR files and show differences
 */

#include "kryon_cli.h"
#include "../../ir/include/ir_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External IR functions
extern IRComponent* ir_read_json_file(const char* filename);
extern void ir_pool_free_component(IRComponent* component);

#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RESET   "\033[0m"

static void print_usage(void) {
    printf("Usage: kryon diff <file1.kir> <file2.kir>\n\n");
    printf("Compare two KIR files and show differences.\n\n");
    printf("Options:\n");
    printf("  --help, -h         Show this help\n\n");
    printf("Examples:\n");
    printf("  kryon diff old.kir new.kir\n");
}

static bool compare_styles(IRStyle* s1, IRStyle* s2, const char* prefix, bool* has_diff) {
    if (!s1 && !s2) return true;

    if (!s1 || !s2) {
        printf("%s  %sStyle: %s vs %s%s\n", prefix,
               COLOR_YELLOW, s1 ? "exists" : "null", s2 ? "exists" : "null", COLOR_RESET);
        *has_diff = true;
        return false;
    }

    // Compare dimensions
    if (s1->width.value != s2->width.value || s1->width.type != s2->width.type) {
        printf("%s  %sWidth: %.0f (type %d) vs %.0f (type %d)%s\n", prefix, COLOR_YELLOW,
               s1->width.value, s1->width.type,
               s2->width.value, s2->width.type,
               COLOR_RESET);
        *has_diff = true;
    }

    if (s1->height.value != s2->height.value || s1->height.type != s2->height.type) {
        printf("%s  %sHeight: %.0f (type %d) vs %.0f (type %d)%s\n", prefix, COLOR_YELLOW,
               s1->height.value, s1->height.type,
               s2->height.value, s2->height.type,
               COLOR_RESET);
        *has_diff = true;
    }

    // Compare background colors
    if (s1->background.type != s2->background.type ||
        memcmp(&s1->background.data, &s2->background.data, sizeof(IRColorData)) != 0) {
        printf("%s  %sBackground color differs%s\n", prefix, COLOR_YELLOW, COLOR_RESET);
        *has_diff = true;
    }

    return !(*has_diff);
}

static bool compare_components(IRComponent* c1, IRComponent* c2, int depth, bool* has_diff) {
    char indent[256] = {0};
    for (int i = 0; i < depth * 2; i++) indent[i] = ' ';

    if (!c1 && !c2) return true;

    if (!c1 || !c2) {
        printf("%s%s%s vs %s%s\n", indent, COLOR_RED,
               c1 ? ir_component_type_to_string(c1->type) : "null",
               c2 ? ir_component_type_to_string(c2->type) : "null",
               COLOR_RESET);
        *has_diff = true;
        return false;
    }

    // Compare types
    if (c1->type != c2->type) {
        printf("%s%s%s vs %s%s\n", indent, COLOR_RED,
               ir_component_type_to_string(c1->type),
               ir_component_type_to_string(c2->type),
               COLOR_RESET);
        *has_diff = true;
        return false;
    }

    // Compare IDs
    if (c1->id != c2->id) {
        printf("%s%s (ID: %u vs %u)\n", indent,
               ir_component_type_to_string(c1->type),
               c1->id, c2->id);
        *has_diff = true;
    }

    // Compare text content
    if (c1->type == IR_COMPONENT_TEXT) {
        if (!c1->text_content != !c2->text_content ||
            (c1->text_content && c2->text_content && strcmp(c1->text_content, c2->text_content) != 0)) {
            printf("%s  %sText: \"%s\" vs \"%s\"%s\n", indent, COLOR_YELLOW,
                   c1->text_content ? c1->text_content : "",
                   c2->text_content ? c2->text_content : "",
                   COLOR_RESET);
            *has_diff = true;
        }
    }

    // Compare styles
    compare_styles(c1->style, c2->style, indent, has_diff);

    // Compare child counts
    if (c1->child_count != c2->child_count) {
        printf("%s  %sChild count: %u vs %u%s\n", indent, COLOR_YELLOW,
               c1->child_count, c2->child_count, COLOR_RESET);
        *has_diff = true;
    }

    // Compare children
    uint32_t min_children = c1->child_count < c2->child_count ? c1->child_count : c2->child_count;
    for (uint32_t i = 0; i < min_children; i++) {
        compare_components(c1->children[i], c2->children[i], depth + 1, has_diff);
    }

    // Show extra children
    if (c1->child_count > min_children) {
        for (uint32_t i = min_children; i < c1->child_count; i++) {
            printf("%s  %s- %s (only in file1)%s\n", indent, COLOR_RED,
                   ir_component_type_to_string(c1->children[i]->type), COLOR_RESET);
            *has_diff = true;
        }
    }
    if (c2->child_count > min_children) {
        for (uint32_t i = min_children; i < c2->child_count; i++) {
            printf("%s  %s+ %s (only in file2)%s\n", indent, COLOR_GREEN,
                   ir_component_type_to_string(c2->children[i]->type), COLOR_RESET);
            *has_diff = true;
        }
    }

    return !(*has_diff);
}

int cmd_diff(int argc, char** argv) {
    const char* file1 = NULL;
    const char* file2 = NULL;

    // Parse arguments
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        } else if (argv[i][0] != '-') {
            if (!file1) {
                file1 = argv[i];
            } else if (!file2) {
                file2 = argv[i];
            } else {
                fprintf(stderr, "Error: Too many files specified\n\n");
                print_usage();
                return 1;
            }
        }
    }

    // Check if both files are provided
    if (!file1 || !file2) {
        fprintf(stderr, "Error: Two files required\n\n");
        print_usage();
        return 1;
    }

    // Check if files exist
    if (!file_exists(file1)) {
        fprintf(stderr, "Error: File not found: %s\n", file1);
        return 1;
    }
    if (!file_exists(file2)) {
        fprintf(stderr, "Error: File not found: %s\n", file2);
        return 1;
    }

    // Load both files
    printf("Comparing:\n");
    printf("  File 1: %s\n", file1);
    printf("  File 2: %s\n\n", file2);

    IRComponent* root1 = ir_read_json_file(file1);
    if (!root1) {
        fprintf(stderr, "Error: Failed to load %s\n", file1);
        return 1;
    }

    IRComponent* root2 = ir_read_json_file(file2);
    if (!root2) {
        fprintf(stderr, "Error: Failed to load %s\n", file2);
        ir_pool_free_component(root1);
        return 1;
    }

    // Compare
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Differences\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    bool has_diff = false;
    compare_components(root1, root2, 0, &has_diff);

    printf("\n");

    if (!has_diff) {
        printf("%s✓ Files are identical%s\n\n", COLOR_GREEN, COLOR_RESET);
    } else {
        printf("%s✗ Files have differences%s\n\n", COLOR_RED, COLOR_RESET);
    }

    ir_pool_free_component(root1);
    ir_pool_free_component(root2);

    return has_diff ? 1 : 0;
}
